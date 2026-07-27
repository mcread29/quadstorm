# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence and the generator documents for procedural-level contracts.

## Current objective

The movement milestone is complete. The next objective is **Milestone 2: projectile firing**—hold the left mouse button to emit pooled projectiles from the player's facing marker. Do not combine that work with enemies, damage, arena walls, generated geometry, or upgrades.

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
| Escape/window close | Exit |

The runtime currently renders a flat test plane, an aimed sphere player, a mouse-ground marker, directional diffuse lighting, and a projected blob shadow. The blob is intentionally not a shadow map; it is cheap and sufficient until real level geometry makes general occlusion valuable.

## Runtime architecture

```text
main.cpp
    ├── reads PlayerInput through game_input
    ├── advances fixed simulation state
    ├── interpolates simulation state for rendering
    └── asks PrototypeRenderer to draw

PlayerInput ──→ updatePlayer() ──→ Player
                      │
                      └── pure of direct raylib input calls

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + interpolated Player ──→ PrototypeRenderer
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, 120 Hz accumulator, interpolation, and composition |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, facing, and player interpolation |
| `src/game/game_camera.hpp/.cpp` | Camera creation/following, camera-relative movement, ground projection, and camera interpolation |
| `src/game/game_input.hpp/.cpp` | All current polling of raylib keyboard and mouse input |
| `src/game/prototype_renderer.hpp/.cpp` | GPU resource ownership and all prototype drawing |
| `src/game/directional_shader.hpp` | Embedded GLSL and shared directional-light vector |
| `CMakeLists.txt` | `stalberg_game` source list and raylib linkage |

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

Simulation advances in fixed `1/120` second steps. Rendering interpolates between the previous and current player/camera states. New gameplay behavior—especially weapon cooldowns, projectile movement, collision, and enemy logic—belongs in the fixed update loop, not the render path.

Frame time is clamped to 50 ms before entering the accumulator to avoid an unbounded catch-up spiral after pauses or debugger stops.

### Input boundary

`updatePlayer()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code should not own raylib `Model` or `Shader` handles. `PrototypeRenderer` owns temporary runtime graphics resources. Projectiles should expose render state to the renderer rather than issuing draw calls from their simulation module.

### Procedural generation boundary

The game target does not yet link the generator libraries. Do not integrate them during the shooting milestone. Later, the level runtime must retain `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone.

Cross-room movement must eventually open only exact doorway cell pairs published by `RoomLayout`; physical contact between regions is not automatically traversable.

## Next implementation: projectile firing

Recommended files:

```text
src/game/weapon.hpp
src/game/weapon.cpp
src/game/projectile_pool.hpp
src/game/projectile_pool.cpp
```

Suggested minimal state:

```cpp
struct Weapon {
    float cooldownRemaining = 0.0F;
};

struct Projectile {
    Vector2 position;
    Vector2 previousPosition;
    Vector2 velocity;
    float remainingLifetime = 0.0F;
    bool active = false;
};
```

Use X/Z gameplay coordinates in the simulation structs; convert them to `Vector3` only for drawing. Reserve or pre-size the pool once. Firing must scan/reuse an inactive slot and must not allocate during gameplay.

Implementation order:

1. Add `bool fireHeld` to `PlayerInput` and populate it with `IsMouseButtonDown(MOUSE_BUTTON_LEFT)`.
2. Add a weapon cooldown updated at the fixed timestep.
3. Spawn from `player.position + player.facing * muzzleDistance` on X/Z.
4. Store previous/current projectile positions for later swept collision and render interpolation.
5. Advance active projectiles and expire them by lifetime or distance.
6. Interpolate active projectiles for rendering.
7. Add a simple projectile mesh or primitive to `PrototypeRenderer`.
8. Stress the pool above the intended on-screen projectile count.

Acceptance criteria:

- Holding fire produces consistent projectile spacing.
- Moving and rotating while firing works.
- Behavior is stable under VSync and uncapped graphical smoke runs.
- No allocation occurs per shot.
- Pool exhaustion fails safely by skipping a shot or reusing a documented slot policy.
- Build and existing tests pass.

## Known limitations

- There is no shooting, target, collision, enemy, audio, generated-level rendering, or run state yet.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- The game runtime has no dedicated headless tests yet; simulation/input separation now makes player and projectile tests straightforward to add.
- The CMake build shown above is whatever build type the build directory was configured with; use a separate Release build when profiling performance.

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
- Ground, player, and shadow VAOs upload successfully.
- Models unload before the custom shader when the window closes.

When investigating performance, first configure and compare a Release build rather than drawing conclusions from the current Debug build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
