# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The target-and-hit milestone is complete. The next objective is **Milestone 4: arena boundaries**—add hard-coded wall segments, player circle collision with sliding, and projectile-wall collision. Do not combine that work with enemies, generated geometry, audio, or upgrades.

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

The runtime currently renders a flat test plane, an aimed sphere player, a mouse-ground marker, directional diffuse lighting, a projected blob shadow, pooled projectiles with motion trails, and one stationary target at X/Z `(-5, -5)`. Holding the left mouse button fires ten projectiles per second from the facing marker. Swept hits damage the five-health target once per projectile, trigger a flash/expanding impact cue, and reset the defeated target after one second. The HUD publishes target health, active projectile count, speed, radius, and fire cadence.

## Runtime architecture

```text
main.cpp
    ├── reads PlayerInput through game_input
    ├── advances fixed simulation state
    ├── interpolates simulation state for rendering
    └── asks PrototypeRenderer to draw

PlayerInput ──→ updatePlayer() ──→ Player
      │               │
      │               └── pure of direct raylib input calls
      └──→ updateWeapon() ──→ Weapon + ProjectilePool
                                      │
                                      ├── fixed-step movement/lifetime
                                      └──→ updateTarget() ──→ Target

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + interpolated Player/Projectiles + Target ──→ PrototypeRenderer
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, 120 Hz accumulator, interpolation, and composition |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, facing, and player interpolation |
| `src/game/weapon.hpp/.cpp` | Fire cadence and muzzle spawning |
| `src/game/projectile_pool.hpp/.cpp` | Preallocated projectile slots, fixed-step movement/lifetime, reuse, and interpolation |
| `src/game/target.hpp/.cpp` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.hpp/.cpp` | Camera creation/following, camera-relative movement, ground projection, and camera interpolation |
| `src/game/game_input.hpp/.cpp` | All current polling of raylib keyboard and mouse input |
| `src/game/prototype_renderer.hpp/.cpp` | GPU resource ownership and all prototype drawing |
| `src/game/directional_shader.hpp` | Embedded GLSL and shared directional-light vector |
| `tests/game_tests.cpp` | Headless projectile pool and weapon simulation coverage |
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

Every fixed projectile update copies `position` to `previousPosition` before advancing. `main.cpp` updates the player, attempts weapon firing, advances projectiles, resolves target hits, and then updates the camera. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment for future wall collision; do not replace swept collision with a current-position overlap test.

### Target contract

The target is fixed at X/Z `(-5, -5)` with radius `0.85`, five health, a `0.16` second hit flash, and a one-second reset delay. `updateTarget()` projects the target center onto each active projectile's clamped previous-to-current segment and compares squared distance against the combined target/projectile radius. Zero-length segments are handled without division.

A hit immediately deactivates the projectile and removes one health. Processing stops when a hit defeats the target, so later slots in that fixed step remain active. While defeated, the target does not collide; it resets at the same position with full health after the delay. Target collision has no dedicated headless coverage by explicit request, so preserve and manually verify direct hits, fast crossing hits, misses, and single-hit behavior when changing it.

### Input boundary

`updatePlayer()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model` or `Shader` handles. `PrototypeRenderer` owns temporary runtime graphics resources. Projectiles expose stable simulation state to the renderer rather than issuing draw calls from their simulation module.

### Procedural generation boundary

The game target does not yet link the generator libraries. Do not integrate them during the arena-boundary milestone. Later, the level runtime must retain `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone.

Cross-room movement must eventually open only exact doorway cell pairs published by `RoomLayout`; physical contact between regions is not automatically traversable.

## Next implementation: arena boundaries

Recommended files:

```text
src/game/arena.hpp
src/game/arena.cpp
```

Define a small immutable array of 2D wall segments around the existing test area. Keep wall geometry and collision authoritative on X/Z; convert segments to wall meshes or primitives only in `PrototypeRenderer`.

Implementation order:

1. Define `WallSegment` start/end points and one hard-coded closed arena.
2. Preserve the player's pre-movement position, resolve its `PLAYER_RADIUS` circle against walls after `updatePlayer()`, and remove only velocity into each contact normal so movement slides along walls.
3. Resolve corners iteratively with a small fixed pass count rather than allocating contact collections.
4. Perform swept moving-projectile-versus-segment collision, including wall endpoints, after projectile movement and before target collision; deactivate a projectile at its first wall hit.
5. Render wall geometry that matches the exact simulation segments.
6. Add focused collision tests for face contact, endpoint contact, corners, sliding, and fast projectiles.

Acceptance criteria:

- The player cannot leave the arena or penetrate corners.
- Movement slides smoothly along walls instead of stopping tangential motion.
- Fast projectiles deactivate on walls without tunneling.
- Wall visuals and authoritative collision segments agree.
- Existing target behavior, build, tests, and graphical smoke checks pass.

## Known limitations

- There are no arena boundaries, player-wall collision, projectile-wall collision, enemies, audio, generated-level rendering, or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Headless game tests currently cover projectile movement/interpolation, lifetime expiry, pool exhaustion/reuse, muzzle spawning, and fire cadence. Target collision, player movement, and rendering lack dedicated tests.
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
- Ground, player, projectile, target, and shadow VAOs upload successfully.
- Models unload before the custom shader when the window closes.

When investigating performance, still configure and compare a Release build rather than drawing final conclusions from a Debug build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
