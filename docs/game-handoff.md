# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The generated-level runtime milestone and its first encounter-preparation refactor are complete. The next objective is **Milestone 7: encounters and room progression**. The immediate prerequisite is to separate reusable combat state/update logic from the regression `Encounter`'s player ownership so `LevelSession::player()` remains the sole authoritative player in generated rooms. Then activate combat after arena entry, drive the existing room lifecycle states, lock and reopen retained doorway thresholds, and progress through the floor. Keep rewards, upgrades, multi-floor runs, and meta-progression out of this milestone.

## Current behavior

Run:

```sh
cmake -S . -B build
cmake --build build -j
./build/stalberg_game
```

Controls:

| Input | Action |
|---|---|
| WASD | Camera-relative movement |
| Mouse | Aim on the XZ ground plane |
| Hold left mouse button | Fire in the combat regression arena |
| R | Reset to Start, or restart after combat defeat/victory |
| F1 | Toggle generated traversal / combat regression arena |
| Escape/window close | Exit |

The runtime starts in a generated traversal view. It builds and retains a relaxed grid, exact dual geometry, neutral room graph, shooter layout, and doorway threshold segments in one immutable `GeneratedLevel`; renders every assigned dual-cell polygon in a cached floor mesh; walls floor/void boundaries and unauthorized room contacts; and omits only exact published doorway pairs. Mutable player, current-room, room-lifecycle, doorway-lock, and active-collision-wall state lives in `LevelSession`. The session spawns the player at the highest-clearance cell in Start, marks entered rooms, can close/reopen exact doorway thresholds for both collision and traversal, and resets completely on `R`. No room is automatically locked until generated encounters are added.

`F1` switches to the preserved hard-coded 20-by-20 combat regression arena. That path still contains the stationary target, deterministic hostile orb, separate projectile pools, swept wall and damage collision, defeat/victory freeze, restart, effects, HUD, and procedural tones described by the first-enemy milestone.

## Runtime architecture

```text
main.cpp
    ├── constructs immutable GeneratedLevel generation + geometry data
    ├── reads PlayerInput through game_input
    ├── advances generated traversal or combat-regression fixed state
    ├── interpolates simulation state for rendering
    └── asks PrototypeRenderer to draw

GeneratedLevel
    ├── retains StalbergGrid + DualGrid + RoomGrid + RoomLayout
    ├── triangulates every assigned exact dual polygon
    ├── retains walls plus exact published doorway threshold segments
    └── publishes Start spawn + immutable door-aware navigation

LevelSession
    ├── owns player + current room + lifecycle states
    ├── locks/unlocks doorway collision and traversal together
    ├── publishes active walls for simulation and rendering
    └── performs generated traversal fixed updates and reset

PlayerInput + active wall span ──→ updateEncounter() ──→ Player ──→ wall resolution
      │                 │                └── health/invulnerability/death
      │                 ├──→ Weapon + player ProjectilePool ──→ Target + Enemy
      │                 ├──→ Enemy movement + fan pattern
      │                 ├──→ enemy ProjectilePool ──→ Player damage
      │                 └──→ deterministic reset on post-defeat/victory R
      └── populated only by game_input

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + GeneratedLevel + interpolated player ──→ PrototypeRenderer
Camera3D + Arena + interpolated combat state ──→ PrototypeRenderer
EncounterStepResult ──→ CombatAudio
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, generated/combat view switching, 120 Hz accumulator, interpolation, audio-event forwarding, and composition |
| `src/game/generated_level.hpp/.cpp` | Immutable generator artifact package, exact floor triangles, closed walls, retained doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.hpp/.cpp` | Mutable generated traversal, room lifecycle/location, doorway locking, active walls, and reset |
| `src/game/encounter.hpp/.cpp` | Fixed-step combat update order, injected wall geometry, and deterministic whole-encounter reset |
| `src/game/enemy.hpp/.cpp` | Deterministic orbit movement, fan cadence, health/damage, and hostile projectile profile |
| `src/game/combat_audio.hpp/.cpp` | Audio-device ownership and generated combat tones |
| `src/game/arena.hpp/.cpp` | Immutable arena segments, reusable circle collision/sliding, and projectile-wall resolution |
| `src/game/collision_2d.hpp/.cpp` | Reusable closest-point, swept-circle, segment, earliest-hit, and closed-loop containment queries |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, facing, health/damage/invulnerability, and interpolation |
| `src/game/weapon.hpp/.cpp` | Fire cadence and muzzle spawning |
| `src/game/projectile_pool.hpp/.cpp` | Profile-driven preallocated projectile slots, fixed-step movement/lifetime, reuse, and interpolation |
| `src/game/target.hpp/.cpp` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.hpp/.cpp` | Camera creation/following, camera-relative movement, ground projection, and camera interpolation |
| `src/game/game_input.hpp/.cpp` | All current polling of raylib keyboard and mouse input |
| `src/game/prototype_renderer.hpp/.cpp` | GPU resource ownership and all prototype drawing |
| `src/game/directional_shader.hpp` | Embedded GLSL and shared directional-light vector |
| `tests/generated_level_tests.cpp` | Artifact alignment, exact floor area, wall/door authorization, dynamic locking, lifecycle/reset, Start spawn, and reachability coverage |
| `tests/game_tests.cpp` | Headless 2D/arena collision, projectile ownership/profile/pool, weapon, enemy determinism/damage/defeat, player damage/death, interpolation freeze, victory, and restart coverage |
| `CMakeLists.txt` | Runtime/test source lists, raylib linkage, warnings, and Debug runtime optimization |

The renderer's destructor unloads models before unloading the shared lighting shader. `CombatAudio` unloads sounds before closing its audio device. Both presentation owners must be destroyed before `CloseWindow()`, which is why they live inside an inner scope in `main.cpp`.

## Technical decisions and invariants

### Coordinate model

```text
world X/Z = authoritative gameplay plane
world Y   = visual height
```

The sphere center stays at `PLAYER_RADIUS` above `Y = 0`. Aim points are ray intersections with the infinite `Y = 0` plane.

### Camera and controls

The camera uses orthographic projection with both a 45-degree elevation and diagonal heading. Movement is derived from the camera's planar forward/right vectors; do not restore a hardcoded isometric input matrix.

### Timing

Simulation advances in fixed `1/120` second steps. Rendering interpolates between the previous and current player, enemy, camera, and projectile states. New gameplay behavior—especially weapon cooldowns, projectile movement, collision, and enemy logic—belongs in the fixed update loop, not the render path.

Frame time is clamped to 50 ms before entering the accumulator to avoid an unbounded catch-up spiral after pauses or debugger stops.

### Projectile contract

Projectile state uses `Vector2` X/Z coordinates and converts to `Vector3` only in the renderer. Each `ProjectilePool` owns a read-only `ProjectileProfile`, so player and enemy pools can use different tuning without ownership flags or per-projectile configuration. Spawning requires finite positive speed/lifetime and a finite nonnegative radius. Every pool contains 192 stable slots and scans from the beginning for the first inactive slot. A full pool drops the attempted shot; the weapon still consumes its cooldown so exhaustion cannot create a burst when a slot becomes available.

The current projectile profiles are:

| Constant | Player | Enemy |
|---|---:|---:|
| Fire interval | 0.1 seconds | 1.15 seconds |
| Projectile speed | 22 world units/second | 6.5 world units/second |
| Projectile lifetime | 1.8 seconds | 3.4 seconds |
| Projectile radius | 0.16 world units | 0.22 world units |

The player muzzle is 1.3 world units from the player center. The enemy emits three directions at 0 and ±14 degrees from its current player-facing direction.

Every fixed projectile update copies `position` to `previousPosition` before advancing. `updateEncounter()` updates the player, resolves player-wall contacts, attempts player firing, advances player projectiles, resolves wall hits, advances/resolves the enemy, resolves enemy then stationary-target hits, emits and advances hostile shots, resolves their wall hits, and finally resolves player damage. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment and keep wall collision before enemy/target/player collision; do not replace swept collision with a current-position overlap test.

### Target contract

The target is fixed at X/Z `(-5, -5)` with radius `0.85`, five health, a `0.16` second hit flash, and a one-second reset delay. `updateTarget()` calls the shared swept circle-versus-circle query for each active projectile, using the projectile pool profile's radius plus the target radius. Zero-length sweeps and initial overlaps are handled without division.

A hit immediately deactivates the projectile and removes one health. Processing stops when a hit defeats the target, so later slots in that fixed step remain active. While defeated, the target does not collide; it resets at the same position with full health after the delay. Target collision has no dedicated headless coverage by explicit request, so preserve and manually verify direct hits, fast crossing hits, misses, and single-hit behavior when changing it.

### Enemy and player-combat contract

The enemy starts at X/Z `(5, 5)`, has 20 health, moves at 2.4 units/second, circles counter-clockwise relative to the player, and adds a clamped radial correction toward a five-unit preferred distance. Its state contains no random source, and identical fixed-step inputs must produce identical movement and shots. Player hits use projectile motion relative to the enemy's previous/current positions, include both radii, consume the projectile, and remove one health. At zero health the encounter freezes in a victory state.

The player has five health. Hostile collision uses projectile motion relative to the player's previous/current fixed-step positions, includes both radii, deactivates a shot on contact even during invulnerability, and removes at most one health before starting 0.8 seconds of invulnerability. At zero player health the encounter freezes in a defeat state. A post-victory or post-defeat `restartPressed` input assigns a fresh `Encounter`, clearing both pools and restoring player, weapon, target, and enemy state exactly.

### Player ownership boundary

There are currently two deliberate player owners in mutually exclusive runtime views: `LevelSession` owns the generated traversal player, while the preserved regression `Encounter` owns its arena player. Do not copy or synchronize those players to add generated combat. First extract reusable combat state/update logic that can operate on a caller-owned `Player&`, injected walls, and deterministic combat state. Keep the existing `Encounter` API as a regression wrapper so its restart semantics and tests remain intact. Generated-room combat must use `LevelSession`'s player directly.

### Arena contract

`ARENA_WALLS` contains four ordered X/Z segments forming a square from `-10` to `10` on each axis. `updateEncounter()` now accepts an injected wall span; its three-argument regression overload forwards `ARENA_WALLS`. Player collision treats the player as a `PLAYER_RADIUS` circle, resolves contacts with four fixed passes, and projects away only velocity into each contact normal so tangential movement survives. The pre-movement player position selects the stable side of wall-face contacts.

Projectile collision uses the shared `collision_2d` queries to treat each projectile as a moving circle and test its full previous-to-current path against segment faces and endpoint circles. The query accepts the pool profile's radius, selects the earliest hit across all walls, and lets the arena resolver clip the position to contact and deactivate the projectile. Because the muzzle can extend beyond a wall while the player remains inside, projectile centers already outside the closed convex wall loop are also deactivated before they can escape. Wall meshes extend outward from the ordered arena segments, leaving each mesh's inner face aligned with its simulation segment.

### Input boundary

`updatePlayer()` and `updateEncounter()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. Restart is an edge input and `main.cpp` latches it until one fixed step consumes it. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model`, `Shader`, or `Sound` handles. `PrototypeRenderer` owns temporary runtime graphics resources, while `CombatAudio` owns the audio device and generated sounds. Projectiles expose stable simulation state to presentation modules rather than issuing draw or audio calls from simulation code.

### Procedural generation boundary

The game target links the generator libraries only through `GeneratedLevel`. The package retains `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone. It also recovers and retains one `DoorwayThreshold` segment per published doorway. Its public API is read-only after construction.

Within one room, neighboring assigned cells are traversable. Across rooms, immutable navigation uses only exact cell pairs published by `RoomLayout::getDoorways()`; physical contact between regions is never automatically traversable. `LevelSession` owns mutable lock state and adds locked threshold segments to both its active collision walls and traversal checks. Connected exterior entrance cells remain enclosed because Milestone 6 has no floor-transition system.

## Next implementation: room encounters and progression

Drive the prepared generated-room lifecycle without replacing the current deterministic combat modules:

1. Extract reusable combat state/update logic from `Encounter` so it accepts a caller-owned `Player&` and injected walls. Preserve `Encounter` as the hard-coded regression wrapper and preserve its deterministic whole-state restart.
2. Add a generated-room encounter coordinator that reacts to `LevelSessionStepResult::enteredRoom` for dormant `Combat` and `Hub` rooms.
3. Filter the room's published enemy-spawn candidates for player distance, occupancy, local clearance, and doorway thresholds; initially select one deterministic enemy spawn.
4. Transition entered → locked → fighting and close the room's retained doorway thresholds for collision, rendering, and traversal.
5. Run the one-enemy combat state against `LevelSession::player()` and `LevelSession::activeWalls()`.
6. On enemy defeat, transition to cleared, reopen the doors, and allow progression toward Exit.

Do not add rewards/upgrades, multi-floor run state, an ECS, a generic asset manager, or inferred openings at incidental room contacts during this milestone. Keep the `F1` hard-coded arena as the focused combat regression path while generated-room state is introduced.

## Known limitations

- The encounter contains only one hard-coded enemy and one stationary target; there are no spawn waves or encounter timer yet.
- Room lifecycle and doorway-lock primitives exist, but no generated-room encounter currently drives them. Reusable combat logic still lives inside a regression `Encounter` that owns a separate player, so player ownership must be separated before integration. There is no reward/progression system or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The combat regression ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Generated-level tests cover retained artifact alignment, exact assigned-floor area, wall/door authorization, doorway clearance, Start spawning, complete door-aware reachability, session lifecycle/reset, and dynamic doorway collision/navigation locking. Combat tests cover injected encounter walls, reusable swept-circle queries, profile-driven projectile movement/lifetime/radius, pool ownership/exhaustion/reuse, muzzle spawning, fire cadence, deterministic enemy behavior, enemy damage/defeat/victory restart, player damage/invulnerability/death/restart, player-wall faces/endpoints/corners/sliding, projectile wall faces/endpoints/earliest hits, and outside-muzzle rejection. Target collision, free player movement, audio, and rendering lack dedicated tests.
- Debug runtime builds use debugger-friendly optimization (`-Og` with GCC/Clang or `/O1` with MSVC) for `stalberg_game` and a bundled raylib while retaining debug symbols and assertions. Configure with `-DSTALBERG_OPTIMIZE_DEBUG_RUNTIME=OFF` when fully unoptimized instruction-by-instruction stepping is required. Use a separate Release build when profiling performance.

## Validation and debugging

Run all checks:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Graphical smoke test:

```sh
timeout 3s xvfb-run -a ./build/stalberg_game
```

Interactive local display:

```sh
DISPLAY=:0 ./build/stalberg_game
```

Useful runtime evidence in raylib logs:

- Custom vertex and fragment shaders compile successfully.
- Ground, wall, player, projectile, target, enemy, and shadow VAOs upload successfully.
- Models unload before the custom shader when the window closes.

When investigating performance, still configure and compare a Release build rather than drawing final conclusions from a Debug build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
