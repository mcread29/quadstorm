# Stålberg-style quad grid (raylib)

A compact C++ implementation of the grid-generation technique associated with Oskar Stålberg's *Townscaper*:

1. Generate a triangular lattice with a hexagonal boundary.
2. Randomly pair adjacent triangles into four-sided faces.
3. Subdivide every triangle and four-sided face using shared edge midpoints and a face center.
4. Apply iterative Laplacian relaxation while pinning the hexagonal boundary.
5. Grow one connected floor plan from the center to at least three outer-edge centers, then partition it into irregular multi-cell rooms.

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

The grid-generation core is a separate library, so its topology and relaxation tests run without opening a graphical window.

## Controls

| Input | Action |
|---|---|
| `R` | Generate the next random grid |
| `G` | Generate a new room layout |
| Left / Right | Change grid seed |
| Up / Down | Change hex radius |
| Space | Pause or resume relaxation |
| `N` | Run one relaxation step |
| `P` | Toggle quad-center markers |
| `F` | Fit grid to the window |
| Mouse wheel | Zoom around cursor |
| Middle/right drag | Pan |

Each solid-line junction is one logical floor cell. Generation begins with a central room, extends connected rooms to the centers of at least three randomly selected outer edges, then grows outward through short room connectors. Grid and room seeds are mixed together so compact grids do not repeatedly produce the same silhouette. New rooms use several structural templates—soft rectangles, galleries, capsules, and L-shapes—adapted to the irregular grid. Rooms always contain multiple connected cells, and every room receives a unique color. The generator preserves exterior negative space instead of filling the patch. Region boundaries remain continuous without doorway gaps, and hovering highlights the complete room under the pointer.

## Linux: missing `DISPLAY`

raylib needs access to a graphical desktop. If startup reports that `DISPLAY` is missing, run the application from a terminal inside your desktop session rather than a text console or ordinary SSH session.

For a remote machine, use one of these options:

- Connect with X11 forwarding: `ssh -X user@host`, then run the application. The local machine must have an X server.
- Use a VNC/RDP desktop session and launch it from a terminal there.
- For a non-visible CI smoke test only: `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display, so it verifies that the application starts but does not show an interactive window.
