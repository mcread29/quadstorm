# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The arena-boundary milestone is complete. The next objective is **Milestone 5: first enemy**—add one enemy, one readable enemy-projectile pattern, player health/damage/death/restart, and minimal combat feedback. Do not combine that work with generated geometry, room progression, or upgrades.

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
| Escape/window close | Exit |

The runtime currently renders a flat test plane enclosed by a 20-by-20 wall arena, an aimed sphere player, a mouse-ground marker, directional diffuse lighting, a projected blob shadow, pooled projectiles with motion trails, and one stationary target at X/Z `(-5, -5)`. The player circle slides along wall faces and resolves corners without leaving the arena. Holding the left mouse button fires ten projectiles per second from the facing marker. Swept projectiles stop at wall faces or endpoints without tunneling; otherwise they can damage the five-health target once, trigger a flash/expanding impact cue, and reset the defeated target after one second. The HUD publishes target health, active projectile count, speed, radius, and fire cadence.

## Runtime architecture

```text
main.cpp
    ├── reads PlayerInput through game_input
    ├── advances fixed simulation state
    ├── interpolates simulation state for rendering
    └── asks PrototypeRenderer to draw

PlayerInput ──→ updatePlayer() ──→ Player ──→ arena wall resolution
      │               │
      │               └── pure of direct raylib input calls
      └──→ updateWeapon() ──→ Weapon + ProjectilePool
                                      │
                                      ├── fixed-step movement/lifetime
                                      ├──→ arena swept wall collision
                                      └──→ updateTarget() ──→ Target

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + Arena + interpolated Player/Projectiles + Target ──→ PrototypeRenderer
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, 120 Hz accumulator, interpolation, and composition |
| `src/game/arena.hpp/.cpp` | Immutable arena segments, iterative player collision/sliding, and swept projectile-wall collision |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, facing, and player interpolation |
| `src/game/weapon.hpp/.cpp` | Fire cadence and muzzle spawning |
| `src/game/projectile_pool.hpp/.cpp` | Preallocated projectile slots, fixed-step movement/lifetime, reuse, and interpolation |
| `src/game/target.hpp/.cpp` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.hpp/.cpp` | Camera creation/following, camera-relative movement, ground projection, and camera interpolation |
| `src/game/game_input.hpp/.cpp` | All current polling of raylib keyboard and mouse input |
| `src/game/prototype_renderer.hpp/.cpp` | GPU resource ownership and all prototype drawing |
| `src/game/directional_shader.hpp` | Embedded GLSL and shared directional-light vector |
| `tests/game_tests.cpp` | Headless arena collision, projectile pool, and weapon simulation coverage |
| `CMakeLists.txt` | Runtime/test source lists, raylib linkage, warnings, and Debug runtime optimization |

The renderer's destructor unloads models before unloading the shared lighting shader. It must be destroyed before `CloseWindow()`, which is why it lives inside an inner scope in `main.cpp`.

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

Simulation advances in fixed `1/120` second steps. Rendering interpolates between the previous and current player, camera, and projectile states. New gameplay behavior—especially weapon cooldowns, projectile movement, collision, and enemy logic—belongs in the fixed update loop, not the render path.

Frame time is clamped to 50 ms before entering the accumulator to avoid an unbounded catch-up spiral after pauses or debugger stops.

### Projectile contract

Projectile state uses `Vector2` X/Z coordinates and converts to `Vector3` only in the renderer. The pool contains 192 stable slots and scans from the beginning for the first inactive slot. A full pool drops the attempted shot; the weapon still consumes its cooldown so exhaustion cannot create a burst when a slot becomes available.

Current tuning constants are:

| Constant | Value |
|---|---:|
| Fire interval | 0.1 seconds |
| Muzzle distance from player center | 1.3 world units |
| Projectile speed | 22 world units/second |
| Projectile lifetime | 1.8 seconds |
| Projectile radius | 0.16 world units |

Every fixed projectile update copies `position` to `previousPosition` before advancing. `main.cpp` updates the player, resolves player-wall contacts, attempts weapon firing, advances projectiles, resolves projectile-wall hits, resolves target hits, and then updates the camera. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment and keep wall collision before target collision; do not replace swept collision with a current-position overlap test.

### Target contract

The target is fixed at X/Z `(-5, -5)` with radius `0.85`, five health, a `0.16` second hit flash, and a one-second reset delay. `updateTarget()` projects the target center onto each active projectile's clamped previous-to-current segment and compares squared distance against the combined target/projectile radius. Zero-length segments are handled without division.

A hit immediately deactivates the projectile and removes one health. Processing stops when a hit defeats the target, so later slots in that fixed step remain active. While defeated, the target does not collide; it resets at the same position with full health after the delay. Target collision has no dedicated headless coverage by explicit request, so preserve and manually verify direct hits, fast crossing hits, misses, and single-hit behavior when changing it.

### Arena contract

`ARENA_WALLS` contains four ordered X/Z segments forming a square from `-10` to `10` on each axis. Player collision treats the player as a `PLAYER_RADIUS` circle, resolves contacts with four fixed passes, and projects away only velocity into each contact normal so tangential movement survives. The pre-movement player position selects the stable side of wall-face contacts.

Projectile collision treats each projectile as a moving circle and tests its full previous-to-current path against segment faces and endpoint circles. It selects the earliest hit across all walls, clips the position to contact, and deactivates the projectile. Because the muzzle can extend beyond a wall while the player remains inside, projectile centers already outside the closed convex wall loop are also deactivated before they can escape. Wall meshes extend outward from the ordered arena segments, leaving each mesh's inner face aligned with its simulation segment.

### Input boundary

`updatePlayer()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model` or `Shader` handles. `PrototypeRenderer` owns temporary runtime graphics resources. Projectiles expose stable simulation state to the renderer rather than issuing draw calls from their simulation module.

### Procedural generation boundary

The game target does not yet link the generator libraries. Do not integrate them during the first-enemy milestone. Later, the level runtime must retain `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone.

Cross-room movement must eventually open only exact doorway cell pairs published by `RoomLayout`; physical contact between regions is not automatically traversable.

## Next implementation: first enemy

Keep the first encounter inside the hard-coded arena. Add one enemy with a simple deterministic movement behavior and one readable projectile pattern, then add player health, damage, invulnerability timing, death, and restart. Minimal combat audio and effects belong in this milestone; generated-level integration does not.

Implementation order:

1. Define enemy simulation state and deterministic fixed-step movement.
2. Add a separate enemy-projectile pool or an explicitly typed projectile owner; do not infer ownership in rendering.
3. Add one slow, readable pattern with swept collision against arena walls and the player circle.
4. Add player health, invulnerability timing, hit feedback, death, and explicit restart input through `PlayerInput`.
5. Add a minimal encounter reset path and focused headless tests for damage, invulnerability, death/restart, and projectile ownership.
6. Add restrained combat audio/effects and manually verify a repeatable 60–90 second encounter.

Acceptance criteria:

- The enemy and projectile pattern are readable while moving and aiming.
- Player damage respects invulnerability timing and cannot apply repeatedly from one hit.
- Death and restart reset all encounter state deterministically.
- Existing arena, target, build, tests, and graphical smoke checks pass.

## Known limitations

- There are no enemies, player health/damage, enemy projectile patterns, audio, generated-level rendering, or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Headless game tests currently cover projectile movement/interpolation, lifetime expiry, pool exhaustion/reuse, muzzle spawning, fire cadence, player-wall faces/endpoints/corners/sliding, projectile wall faces/endpoints/earliest hits, and outside-muzzle rejection. Target collision, free player movement, and rendering lack dedicated tests.
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
- Ground, wall, player, projectile, target, and shadow VAOs upload successfully.
- Models unload before the custom shader when the window closes.

When investigating performance, still configure and compare a Release build rather than drawing final conclusions from a Debug build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
