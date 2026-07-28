# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The first-enemy milestone is complete. The next objective is **Milestone 6: generated level runtime**—retain the related generator artifacts in one immutable runtime package, render exact generated floors and walls, open only published doorway pairs, and spawn the player in the generated start room. Do not combine that work with room encounters, rewards, progression, or upgrades.

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
| Hold left mouse button | Fire |
| R after defeat or victory | Restart the encounter |
| Escape/window close | Exit |

The runtime currently renders a flat test plane enclosed by a 20-by-20 wall arena, an aimed sphere player, a mouse-ground marker, directional diffuse lighting, a projected blob shadow, pooled projectiles with motion trails, one stationary target at X/Z `(-5, -5)`, and one hostile orb. The player circle slides along wall faces and resolves corners without leaving the arena. Holding the left mouse button fires ten projectiles per second from the facing marker. Swept player projectiles stop at walls or damage the stationary target and the moving 20-health enemy. The enemy deterministically circles the player near a five-unit preferred range and emits a slow three-shot fan every 1.15 seconds. Hostile shots use a separate pool, stop at walls, and damage the five-health player through swept collision. Player defeat or enemy defeat freezes the encounter until `R` resets all simulation state. The HUD, hit/death/victory effects, and procedural tones provide minimal combat feedback.

## Runtime architecture

```text
main.cpp
    ├── reads PlayerInput through game_input
    ├── advances fixed simulation state
    ├── interpolates simulation state for rendering
    └── asks PrototypeRenderer to draw

PlayerInput ──→ updateEncounter() ──→ Player ──→ arena wall resolution
      │                 │                └── health/invulnerability/death
      │                 ├──→ Weapon + player ProjectilePool ──→ Target + Enemy
      │                 ├──→ Enemy movement + fan pattern
      │                 ├──→ enemy ProjectilePool ──→ Player damage
      │                 └──→ deterministic reset on post-defeat/victory R
      └── populated only by game_input

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + Arena + interpolated combat state ──→ PrototypeRenderer
EncounterStepResult ──→ CombatAudio
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, 120 Hz accumulator, interpolation, audio-event forwarding, and composition |
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

The game target does not yet link the generator libraries. Integrate them through the generated-level runtime package, which must retain `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone.

Cross-room movement must eventually open only exact doorway cell pairs published by `RoomLayout`; physical contact between regions is not automatically traversable.

## Next implementation: generated level runtime

Create an immutable runtime level package that retains `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together. Triangulate exact assigned `DualCell` floor polygons, extrude closed dual boundaries into walls, open only exact doorway cell pairs published by `RoomLayout`, build door-aware traversal/navigation data, and spawn the player in the generated start room.

Do not add room encounter states, progression, rewards, upgrades, an ECS, or a generic asset manager during this milestone. Preserve the hard-coded combat arena as a focused regression path until generated floor rendering and collision are independently validated.

Acceptance criteria:

- Generated floors match exact assigned dual-cell polygons without gaps or unauthorized bridges.
- Wall collision follows closed boundaries and opens only published doorway pairs.
- The player spawns in Start and can traverse every authorized doorway without leaving the floor.
- Existing generator tests, hard-coded combat tests, build, and graphical smoke checks pass.

## Known limitations

- The encounter contains only one hard-coded enemy and one stationary target; there are no spawn waves or encounter timer yet.
- There is no generated-level rendering, room encounter state, reward/progression system, or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Headless game tests currently cover reusable swept-circle queries, profile-driven projectile movement/lifetime/radius, pool ownership/exhaustion/reuse, muzzle spawning, fire cadence, deterministic enemy behavior, enemy damage/defeat/victory restart, player damage/invulnerability/death/restart, player-wall faces/endpoints/corners/sliding, projectile wall faces/endpoints/earliest hits, and outside-muzzle rejection. Target collision, free player movement, audio, and rendering lack dedicated tests.
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
