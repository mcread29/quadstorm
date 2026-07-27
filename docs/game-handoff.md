# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The projectile-firing milestone is complete. The next objective is **Milestone 3: target and hit feedback**—add one stationary circle target, swept projectile collision, health/reset behavior, and clear hit feedback. Do not combine that work with enemies, arena walls, generated geometry, or upgrades.

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

The runtime currently renders a flat test plane, an aimed sphere player, a mouse-ground marker, directional diffuse lighting, a projected blob shadow, and pooled projectiles with motion trails. Holding the left mouse button fires ten projectiles per second from the facing marker. The blob is intentionally not a shadow map; it is cheap and sufficient until real level geometry makes general occlusion valuable.

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
                                      └── fixed-step movement/lifetime

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + interpolated Player/Projectiles ──→ PrototypeRenderer
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, 120 Hz accumulator, interpolation, and composition |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, facing, and player interpolation |
| `src/game/weapon.hpp/.cpp` | Fire cadence and muzzle spawning |
| `src/game/projectile_pool.hpp/.cpp` | Preallocated projectile slots, fixed-step movement/lifetime, and interpolation |
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

Every fixed projectile update copies `position` to `previousPosition` before advancing. `main.cpp` updates the player, attempts weapon firing, advances projectiles, and then updates the camera. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment for swept collision; do not replace collision with a current-position overlap test.

### Input boundary

`updatePlayer()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model` or `Shader` handles. `PrototypeRenderer` owns temporary runtime graphics resources. Projectiles expose stable simulation state to the renderer rather than issuing draw calls from their simulation module.

### Procedural generation boundary

The game target does not yet link the generator libraries. Do not integrate them during the target-and-hit milestone. Later, the level runtime must retain `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone.

Cross-room movement must eventually open only exact doorway cell pairs published by `RoomLayout`; physical contact between regions is not automatically traversable.

## Next implementation: target and hit feedback

Recommended files:

```text
src/game/target.hpp
src/game/target.cpp
```

Add one stationary target on the test plane with a 2D circle collider and a small health value. Keep collision authoritative on X/Z and keep target rendering inside `PrototypeRenderer`. Extend the projectile-pool simulation API as needed to deactivate hit slots without allocating or transferring collision ownership to the renderer.

For each projectile segment, project the target center onto `previousPosition → position`, clamp the segment parameter to `[0, 1]`, and compare squared distance against `(target.radius + PROJECTILE_RADIUS)²`. Handle a zero-length segment without dividing by zero.

Implementation order:

1. Add a minimal target simulation state containing position, radius, health, and hit-flash time.
2. Test every active projectile's `previousPosition → position` segment against the target circle after projectile movement.
3. Deactivate a projectile on its first hit and decrement target health once.
4. Render the target and a short, unambiguous hit flash or impact cue.
5. Reset the target after health reaches zero so the prototype remains continuously testable.
6. Add headless tests for direct hits, misses, tangent/near-tangent shots, fast swept hits, and single-hit deactivation.

Acceptance criteria:

- Fast projectiles cannot tunnel through the target.
- Each projectile damages the target at most once.
- Hits and target defeat/reset are visually unambiguous.
- Collision behavior remains fixed-step and independent of render rate.
- Build, tests, and graphical smoke checks pass.

## Known limitations

- There is no target, collision, enemy, audio, generated-level rendering, or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Headless game tests currently cover projectile movement/interpolation, lifetime expiry, pool exhaustion/reuse, muzzle spawning, and fire cadence; player movement and rendering still lack dedicated tests.
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
- Ground, player, projectile, and shadow VAOs upload successfully.
- Models unload before the custom shader when the window closes.

When investigating performance, still configure and compare a Release build rather than drawing final conclusions from a Debug build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
