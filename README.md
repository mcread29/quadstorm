# Stålberg grid and 2.5D game prototype (raylib)

A compact C++ implementation of the grid-generation technique associated with Oskar Stålberg's *Townscaper*:

The grid generator:

1. Generates a triangular lattice with a hexagonal boundary.
2. Randomly pairs adjacent triangles into four-sided faces.
3. Subdivides every triangle and four-sided face using shared edge midpoints and a face center.
4. Applies iterative Laplacian relaxation while pinning the hexagonal boundary.

As a separate pass, the room generator consumes a neutral cell graph with physical cell area, clearance, traversal distance, shared-boundary width, and explicit entrance candidates. Its default shooter method plans a mission graph first, embeds several combat arenas—including occasional dense two-to-three-room clusters or one larger landmark room when the map supports them—and joins them with explicit corridor regions where space permits, side branches, a meaningful start-to-exit route, and at most one deliberate loop. Legacy branching-shape and organic-growth methods remain available. Every method generates several deterministic candidates and keeps the highest-scoring valid layout.

The subdivision step guarantees that the final mesh consists entirely of quads, including where random pairing leaves unmatched triangles.

The repository now also contains the first three runtime slices of a top-down 2.5D roguelite bullet hell. The `stalberg_game` executable supports movement, aiming, fixed-cadence pooled projectile firing, and a resettable target with swept hit detection over a flat test plane; generated-level integration follows after the core combat sandbox is proven.

## Documentation

- [`docs/game-roadmap.md`](docs/game-roadmap.md) — ordered milestones from the movement prototype through a complete roguelite run.
- [`docs/game-handoff.md`](docs/game-handoff.md) — current runtime architecture, decisions, limitations, validation, and exact next work.
- [`docs/demo-and-algorithm.md`](docs/demo-and-algorithm.md) — base mesh mathematics, topology, relaxation, rendering, and source map.
- [`docs/room-generation-model.md`](docs/room-generation-model.md) — neutral physical input, output API, roles, doorways, and gameplay integration contract.
- [`docs/shooter-level-generation.md`](docs/shooter-level-generation.md) — complete graph-first arena, route, corridor, entrance, doorway, and tactical-annotation pipeline.
- [`docs/layout-quality-and-testing.md`](docs/layout-quality-and-testing.md) — deterministic candidates, validation, scoring formulas, fallback behavior, and test coverage.

## Build

A system raylib installation is used when available. Otherwise CMake downloads raylib 5.5 automatically.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/stalberg_game       # minimal 2.5D target-practice prototype
./build/stalberg_grid       # procedural-generation diagnostic demo
```

The target-practice prototype is intentionally small: use **WASD** to move the sphere, the **mouse** to aim its facing marker across the XZ ground plane, and hold the **left mouse button** to fire. A five-health target flashes on swept projectile hits, shows a defeat cue, and resets after one second. A 45-degree tilted orthographic camera follows the player. Simulation runs at a fixed 120 Hz; rendering interpolates player, camera, and projectile state. Firing uses a preallocated stable-slot pool and does not allocate per shot.

Debug builds apply debugger-friendly optimization to the game runtime and bundled raylib so interactive frame pacing remains representative while symbols and assertions stay enabled. Configure with `-DSTALBERG_OPTIMIZE_DEBUG_RUNTIME=OFF` when fully unoptimized stepping is required.

Grid generation and room generation are independent libraries. The room library has no dependency on `StalbergGrid`; `src/integration/room_grid_adapter.cpp` is the translation layer between the generated mesh and the room module's owned `RoomGrid` snapshot. Grid-specific policy, including dual-cell measurement and selecting centers from the six-sided boundary as entrance candidates, stays in the grid and adapter layers. The demo completes relaxation before creating that snapshot so visual geometry, room scoring, and physical metrics agree. Each module has its own headless test executable.

## Game prototype controls

| Input | Action |
|---|---|
| WASD | Move relative to the camera |
| Mouse | Aim on the ground plane |
| Hold left mouse button | Fire |
| Escape/window close | Exit |

Target collision and hit feedback are complete. The next milestone adds hard-coded arena walls, player collision/sliding, and projectile-wall collision.

## Generator demo controls

| Input | Action |
|---|---|
| `R` | Generate the next random grid |
| `G` | Generate a new room layout |
| `M` | Cycle shooter, branching-shape, and organic-growth generation |
| Left / Right | Change grid seed |
| Up / Down | Change hex radius |
| `P` | Toggle quad-center markers |
| `F` | Fit grid to the window |
| Mouse wheel | Zoom around cursor |
| Middle/right drag | Pan |

Each solid-line junction is one logical floor cell. The default **shooter layout** chooses start and exit anchors near well-separated selected entrances, usually distributes additional arena seeds with farthest-point sampling, occasionally concentrates two or three substantial rooms into a local cluster or gives one site a larger landmark arena, and plans a spatial mission graph between them. Every planned arena connection is routed with clearance- and portal-width-aware pathfinding. Longer routes become independently identified corridor regions and are widened laterally where geometry permits. Routes shorter than an arena's approximate diameter are folded into an endpoint arena and become direct arena doorways instead of tiny connector rooms; separate connector identities are reserved for long passages. A tree supplies the main route and side branches; larger maps may receive one intentional loop. Only planned room contacts become doorways, so incidental touching does not create unwanted shortcuts.

The legacy branching method builds geometric room masks, while organic generation grows and partitions a connected noisy footprint. Every method preserves exterior negative space, connects the selected three-to-six boundary entrances, and combines the room seed with a fingerprint of the neutral topology and physical metrics. Layout metadata identifies start, exit, hub, connector, reward, and combat rooms, plus cover and enemy-spawn candidate cells kept away from published internal doorway thresholds. Games should separately filter around connected exterior entrances. Region boundaries remain continuous in this 2D diagnostic renderer, and hovering highlights the complete room under the pointer.

## Linux: missing `DISPLAY`

raylib needs access to a graphical desktop. If startup reports that `DISPLAY` is missing, run the application from a terminal inside your desktop session rather than a text console or ordinary SSH session.

For a remote machine, use one of these options:

- Connect with X11 forwarding: `ssh -X user@host`, then run the application. The local machine must have an X server.
- Use a VNC/RDP desktop session and launch it from a terminal there.
- For a non-visible CI smoke test only: `xvfb-run -a ./build/stalberg_game` or `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display, so it verifies that an application starts but does not show an interactive window. The development workstation used for the prototype can launch interactively with `DISPLAY=:0 ./build/stalberg_game`.
