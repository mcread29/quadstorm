# Game Roadmap

This document tracks the path from the procedural-generation demo to a top-down 2.5D roguelite bullet hell. The immediate rule is to finish one small, playable capability at a time instead of expanding every system in parallel.

For the current implementation state and continuation instructions, see [`game-handoff.md`](game-handoff.md).

## Product direction

- Render a three-dimensional world with a tilted orthographic camera.
- Keep authoritative gameplay on the horizontal XZ plane; world Y represents visual height.
- Use mouse and keyboard twin-stick controls first, with controller support later.
- Preserve `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` as the procedural level compiler.
- Build and validate combat in a simple test plane before consuming generated levels.
- Keep the simulation deterministic enough for seeded runs and reproducible debugging.

## Current status

### Milestone 1: movement prototype — complete

The `stalberg_game` executable currently provides:

- A flat XZ ground plane.
- A sphere player with accelerated, normalized WASD movement.
- Camera-relative controls.
- Mouse aiming through screen-ray/ground-plane intersection.
- A 45-degree elevation, 45-degree heading orthographic follow camera.
- A directional diffuse-light shader.
- A lightweight projected player shadow.
- A fixed 120 Hz simulation with interpolated rendering.
- Cached ground, player, and shadow meshes.

Acceptance check: the player can move smoothly in every direction, aim independently, and remain readable under the follow camera.

### Milestone 2: projectile firing — complete

The runtime now provides:

- Left-mouse held firing with a fixed ten-shot-per-second cadence.
- A 192-slot preallocated projectile pool with safe dropped shots on exhaustion.
- Muzzle spawning from the player's facing marker.
- Fixed-step projectile movement, lifetime expiry, and stable-slot reuse.
- Previous/current projectile positions for render interpolation and later swept collision.
- Lit projectile meshes with a simple motion trail.
- Headless tests for projectile movement/interpolation, lifetime expiry, pool exhaustion/reuse, muzzle spawning, and weapon cadence.

Acceptance check: the player can move and aim while holding fire; projectile spacing and speed remain stable at different render rates, and expired projectiles reuse pool capacity without allocation.

### Milestone 3: target and hit feedback — complete

The runtime now provides:

- One stationary five-health target with a 2D circle collider.
- Swept projectile-segment collision including both target and projectile radii.
- Single-hit projectile deactivation and damage.
- A target hit flash, expanding impact cue, defeat state, and automatic reset.
- HUD diagnostics for target health, active projectile count, speed, radius, and fire rate.

Headless target-collision tests were explicitly deferred for this milestone; existing projectile and weapon tests continue to pass.

Acceptance check: fast projectiles cannot tunnel through the target, and hits feel unambiguous.

### Milestone 4: arena boundaries — complete

The runtime now provides:

- Four immutable wall segments enclosing the hard-coded 20-by-20 arena.
- Iterative player circle-versus-segment collision, including endpoint contacts.
- Velocity projection that preserves tangential movement for smooth wall sliding.
- Swept projectile circle-versus-segment collision without tunneling, including endpoint and earliest-hit handling.
- Rejection of outward-facing muzzle shots that begin beyond the closed arena loop.
- Wall meshes whose inner faces follow the authoritative X/Z segments.
- Headless tests for wall faces, endpoints, corners, sliding, fast projectiles, and outside-muzzle rejection.

Acceptance check: movement remains smooth along corners and walls, and neither player nor projectiles escape the arena.

## Next milestones

### Milestone 5: first enemy

- Add one enemy that moves toward or around the player.
- Add one readable enemy projectile pattern.
- Add player health, damage, invulnerability timing, death, and restart.
- Add minimal combat audio and effects.

Acceptance check: the hard-coded arena supports a repeatable 60–90 second survival encounter.

### Milestone 6: generated level runtime

Create an immutable runtime level package retaining the related generation artifacts:

```text
StalbergGrid
    → DualGrid
    → RoomGrid
    → RoomLayout
    → render meshes + collision + navigation + doors
```

- Triangulate assigned `DualCell` polygons into floor meshes.
- Extrude closed dual boundaries into walls.
- Open only doorway pairs published by `RoomLayout`.
- Build door-aware traversal and navigation data.
- Spawn the player in the generated start room.

Acceptance check: the player can traverse a generated level through every authorized doorway without crossing closed contacts or leaving the floor.

### Milestone 7: encounters and room progression

- Add room states: dormant, entered, locked, fighting, cleared, rewarded.
- Filter the generator's enemy-spawn candidates for gameplay constraints.
- Lock doors during encounters and reopen them after clearing.
- Treat generated room roles as structural hints; assign encounter and reward content in a separate pass.

Acceptance check: a generated floor can be entered at Start, cleared room by room, and completed at Exit.

### Milestone 8: roguelite run

- Compose multiple floors into one seeded run.
- Add reward choices and a small build-modifier system.
- Split random streams for topology, encounters, placement, rewards, and cosmetics.
- Add difficulty escalation, a boss, death, victory, and immediate restart.

Acceptance check: the game supports a complete short run with meaningful build variation.

## Deferred systems

Do not build these until a milestone requires them:

- ECS or generic scene graph.
- Scripting language for projectile patterns.
- General-purpose asset manager.
- Meta-progression and permanent unlock trees.
- Save-anywhere support.
- Real-time shadow maps.
- Infinite/chunked procedural levels.
- Network or replay protocol guarantees.

## Engineering gates

After every change:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For graphical smoke testing without a visible desktop:

```sh
xvfb-run -a ./build/stalberg_game
```

For interactive testing on the local X display used during development:

```sh
DISPLAY=:0 ./build/stalberg_game
```
