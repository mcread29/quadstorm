# Stålberg-style quad grid (raylib)

A compact C++ implementation of the grid-generation technique associated with Oskar Stålberg's *Townscaper*:

The grid generator:

1. Generates a triangular lattice with a hexagonal boundary.
2. Randomly pairs adjacent triangles into four-sided faces.
3. Subdivides every triangle and four-sided face using shared edge midpoints and a face center.
4. Applies iterative Laplacian relaxation while pinning the hexagonal boundary.

As a separate pass, the room generator consumes a neutral cell graph with physical cell area, clearance, traversal distance, shared-boundary width, and explicit entrance candidates. Its default shooter method plans a mission graph first, embeds several combat arenas, and joins them with explicit corridor regions where space permits, side branches, a meaningful start-to-exit route, and at most one deliberate loop. Legacy branching-shape and organic-growth methods remain available. Every method generates several deterministic candidates and keeps the highest-scoring valid layout.

The subdivision step guarantees that the final mesh consists entirely of quads, including where random pairing leaves unmatched triangles.

## Documentation

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
./build/stalberg_grid
```

Grid generation and room generation are independent libraries. The room library has no dependency on `StalbergGrid`; `src/integration/room_grid_adapter.cpp` is the translation layer between the generated mesh and the room module's owned `RoomGrid` snapshot. Grid-specific policy, including dual-cell measurement and selecting centers from the six-sided boundary as entrance candidates, stays in the grid and adapter layers. The demo completes relaxation before creating that snapshot so visual geometry, room scoring, and physical metrics agree. Each module has its own headless test executable.

## Controls

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

Each solid-line junction is one logical floor cell. The default **shooter layout** chooses start and exit anchors near well-separated selected entrances, distributes additional arena seeds with farthest-point sampling, grows compact multi-cell combat spaces around those seeds, and plans a spatial mission graph between them. Every planned arena connection is routed with clearance- and portal-width-aware pathfinding. Longer routes become independently identified corridor regions and are widened laterally where geometry permits. Routes shorter than an arena's approximate diameter are folded into an endpoint arena and become direct arena doorways instead of tiny connector rooms; separate connector identities are reserved for long passages. A tree supplies the main route and side branches; larger maps may receive one intentional loop. Only planned room contacts become doorways, so incidental touching does not create unwanted shortcuts.

The legacy branching method builds geometric room masks, while organic generation grows and partitions a connected noisy footprint. Every method preserves exterior negative space, connects the selected three-to-six boundary entrances, and combines the room seed with a fingerprint of the neutral topology and physical metrics. Layout metadata identifies start, exit, hub, connector, reward, and combat rooms, plus cover and enemy-spawn candidate cells kept away from published internal doorway thresholds. Games should separately filter around connected exterior entrances. Region boundaries remain continuous in this 2D diagnostic renderer, and hovering highlights the complete room under the pointer.

## Linux: missing `DISPLAY`

raylib needs access to a graphical desktop. If startup reports that `DISPLAY` is missing, run the application from a terminal inside your desktop session rather than a text console or ordinary SSH session.

For a remote machine, use one of these options:

- Connect with X11 forwarding: `ssh -X user@host`, then run the application. The local machine must have an X server.
- Use a VNC/RDP desktop session and launch it from a terminal there.
- For a non-visible CI smoke test only: `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display, so it verifies that the application starts but does not show an interactive window.
