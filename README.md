# Stålberg-style quad grid (raylib)

A compact C++ implementation of the grid-generation technique associated with Oskar Stålberg's *Townscaper*:

The grid generator:

1. Generates a triangular lattice with a hexagonal boundary.
2. Randomly pairs adjacent triangles into four-sided faces.
3. Subdivides every triangle and four-sided face using shared edge midpoints and a face center.
4. Applies iterative Laplacian relaxation while pinning the hexagonal boundary.

As a separate pass, the room generator consumes a neutral cell graph with physical cell area, clearance, traversal distance, shared-boundary width, and explicit entrance candidates. It offers two deterministic methods: branching geometric room shapes and organic multi-source growth. Both create one connected floor plan from the center to between three and six entrances and partition it into as many as 64 irregular multi-cell rooms. Several deterministic candidates are generated and scored, narrow connectors are penalized, and the best layout receives a controlled circulation graph made from a quality-weighted spanning tree plus a small loop budget.

The subdivision step guarantees that the final mesh consists entirely of quads, including where random pairing leaves unmatched triangles.

See [`docs/demo-and-algorithm.md`](docs/demo-and-algorithm.md) for a detailed explanation of the mathematics, mesh topology, relaxation, rendering, and source-code structure.

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
| `M` | Switch between branching-shape and organic-growth room generation |
| Left / Right | Change grid seed |
| Up / Down | Change hex radius |
| `P` | Toggle quad-center markers |
| `F` | Fit grid to the window |
| Mouse wheel | Zoom around cursor |
| Middle/right drag | Pan |

Each solid-line junction is one logical floor cell. The default method begins with a central room, may add slim radial branches, extends rooms to three to six randomly selected outer-edge centers, and continues through short room connectors. Both methods assign rooms one of five growth profiles: compact, elongated, branching, irregular, or L-shaped. The default method turns those profiles into geometric room masks, while organic generation uses them to score each room's frontier expansion. The organic method first joins the center to the same kind of entrance selection, then grows a noisy connected footprint while balancing expansion across six angular sectors. Every concrete subset of three to six sides is equally likely, so the connected-side counts occur proportionally to their number of combinations: 20:15:6:1. It partitions that footprint from spatially separated seeds with weighted multi-source frontier growth, deliberately mixing compact rooms with rooms several times larger. Both methods preserve exterior negative space, produce only connected multi-cell rooms, and combine the room seed with a fingerprint of the neutral topology and physical metrics. Doorways favor wide, clear shared boundaries. They connect every room with a spanning tree and add only a few useful loops rather than opening every touching room pair. Layout metadata identifies start, exit, hub, connector, reward, and combat rooms, plus cover and enemy-spawn candidate cells kept away from door thresholds. Region boundaries remain continuous in this 2D diagnostic renderer, and hovering highlights the complete room under the pointer.

## Linux: missing `DISPLAY`

raylib needs access to a graphical desktop. If startup reports that `DISPLAY` is missing, run the application from a terminal inside your desktop session rather than a text console or ordinary SSH session.

For a remote machine, use one of these options:

- Connect with X11 forwarding: `ssh -X user@host`, then run the application. The local machine must have an X server.
- Use a VNC/RDP desktop session and launch it from a terminal there.
- For a non-visible CI smoke test only: `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display, so it verifies that the application starts but does not show an interactive window.
