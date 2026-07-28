# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The generated-level runtime milestone is complete. The next objective is **Milestone 7: encounters and room progression**—add room lifecycle states, activate combat after entering an arena, lock and reopen its published doors, and progress through the generated floor. Keep rewards, upgrades, multi-floor runs, and meta-progression out of this milestone.

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

The runtime starts in a generated traversal view. It builds and retains a relaxed grid, exact dual geometry, neutral room graph, and shooter layout in one `GeneratedLevel`; renders every assigned dual-cell polygon in a cached floor mesh; walls floor/void boundaries and unauthorized room contacts; omits only exact published doorway pairs; and spawns the player at the highest-clearance cell in Start. The same door policy drives immutable navigation adjacency. Player collision uses the generated wall segments, and `R` resets the player to Start.

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
    ├── walls floor edges and unauthorized cross-region contacts
    ├── opens exact RoomLayout doorway cell pairs
    └── publishes Start spawn + door-aware navigation

PlayerInput ──→ updateEncounter() ──→ Player ──→ arena wall resolution
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
| `src/game/generated_level.hpp/.cpp` | Immutable generator artifact package, exact floor triangles, authorized walls, door-aware navigation, and Start spawn |
| `src/game/encounter.hpp/.cpp` | Fixed-step combat update order and deterministic whole-encounter reset |
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
| `tests/generated_level_tests.cpp` | Artifact alignment, exact floor area, wall/door authorization, doorway clearance, Start spawn, and door-aware reachability coverage |
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

### Arena contract

`ARENA_WALLS` contains four ordered X/Z segments forming a square from `-10` to `10` on each axis. Player collision treats the player as a `PLAYER_RADIUS` circle, resolves contacts with four fixed passes, and projects away only velocity into each contact normal so tangential movement survives. The pre-movement player position selects the stable side of wall-face contacts.

Projectile collision uses the shared `collision_2d` queries to treat each projectile as a moving circle and test its full previous-to-current path against segment faces and endpoint circles. The query accepts the pool profile's radius, selects the earliest hit across all walls, and lets the arena resolver clip the position to contact and deactivate the projectile. Because the muzzle can extend beyond a wall while the player remains inside, projectile centers already outside the closed convex wall loop are also deactivated before they can escape. Wall meshes extend outward from the ordered arena segments, leaving each mesh's inner face aligned with its simulation segment.

### Input boundary

`updatePlayer()` and `updateEncounter()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. Restart is an edge input and `main.cpp` latches it until one fixed step consumes it. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model`, `Shader`, or `Sound` handles. `PrototypeRenderer` owns temporary runtime graphics resources, while `CombatAudio` owns the audio device and generated sounds. Projectiles expose stable simulation state to presentation modules rather than issuing draw or audio calls from simulation code.

### Procedural generation boundary

The game target links the generator libraries only through `GeneratedLevel`. The package retains `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone. Its public API is read-only after construction.

Within one room, neighboring assigned cells are traversable. Across rooms, both wall openings and navigation use only exact cell pairs published by `RoomLayout::getDoorways()`; physical contact between regions is never automatically traversable. Connected exterior entrance cells remain enclosed because Milestone 6 has no floor-transition system.

## Next implementation: room encounters and progression

Add a small generated-room lifecycle without replacing the current deterministic combat modules:

- Track room states such as dormant, entered, locked, fighting, cleared, and rewarded.
- Detect player entry through `GeneratedLevel` cell/region lookup.
- Close the current room's published doorway thresholds during combat and reopen them after clearing.
- Select enemy positions from the room's published spawn candidates after filtering for player distance, occupancy, and doorway clearance.
- Progress from Start toward Exit over the existing door-aware room graph.

Do not add rewards/upgrades, multi-floor run state, an ECS, a generic asset manager, or inferred openings at incidental room contacts during this milestone. Keep the `F1` hard-coded arena as the focused combat regression path while generated-room state is introduced.

## Known limitations

- The encounter contains only one hard-coded enemy and one stationary target; there are no spawn waves or encounter timer yet.
- There is no generated-room encounter state, door locking, reward/progression system, or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The combat regression ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Generated-level tests cover retained artifact alignment, exact assigned-floor area, wall/door authorization, doorway clearance, Start spawning, and complete door-aware reachability. Combat tests cover reusable swept-circle queries, profile-driven projectile movement/lifetime/radius, pool ownership/exhaustion/reuse, muzzle spawning, fire cadence, deterministic enemy behavior, enemy damage/defeat/victory restart, player damage/invulnerability/death/restart, player-wall faces/endpoints/corners/sliding, projectile wall faces/endpoints/earliest hits, and outside-muzzle rejection. Target collision, free player movement, audio, and rendering lack dedicated tests.
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
