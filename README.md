# Stålberg-style quad grid (raylib)

A compact C++ implementation of the grid-generation technique associated with Oskar Stålberg's *Townscaper*:

1. Generate a triangular lattice with a hexagonal boundary.
2. Randomly pair adjacent triangles into four-sided faces.
3. Subdivide every triangle and four-sided face using shared edge midpoints and a face center.
4. Apply iterative Laplacian relaxation while pinning the hexagonal boundary.

The subdivision step guarantees that the final mesh consists entirely of quads, including where random pairing leaves unmatched triangles.

See [`docs/demo-and-algorithm.md`](docs/demo-and-algorithm.md) for a detailed explanation of the mathematics, mesh topology, relaxation, rendering, and source-code structure.

## Build

A system raylib installation is used when available. Otherwise CMake downloads raylib 5.5 automatically.

```sh
cmake -S . -B build
cmake --build build -j
./build/stalberg_grid
```

## Controls

| Input | Action |
|---|---|
| `R` | Generate the next random layout |
| Left / Right | Change random seed |
| Up / Down | Change hex radius |
| Space | Pause or resume relaxation |
| `N` | Run one relaxation step |
| `P` | Toggle points |
| `F` | Fit grid to the window |
| Mouse wheel | Zoom around cursor |
| Middle/right drag | Pan |

Gold points are pinned boundary vertices. Their fixed positions keep the outer hexagon intact during relaxation.

## Linux: missing `DISPLAY`

raylib needs access to a graphical desktop. If startup reports that `DISPLAY` is missing, run the application from a terminal inside your desktop session rather than a text console or ordinary SSH session.

For a remote machine, use one of these options:

- Connect with X11 forwarding: `ssh -X user@host`, then run the application. The local machine must have an X server.
- Use a VNC/RDP desktop session and launch it from a terminal there.
- For a non-visible CI smoke test only: `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display, so it verifies that the application starts but does not show an interactive window.
