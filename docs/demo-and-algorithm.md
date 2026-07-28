# Stålberg-Style Quad Grid Demo

This document explains the raylib demo in this repository and the grid-generation algorithm implemented in `src/grid/stalberg_grid.cpp`.

> **Name note:** The technique is associated with **Oskar Stålberg**, creator of *Townscaper*. It is sometimes incorrectly attributed to “Peter Stålberg.”

Room-generation details are split into focused documents:

- [`room-generation-model.md`](room-generation-model.md) — physical neutral input and output API.
- [`shooter-level-generation.md`](shooter-level-generation.md) — complete graph-first shooter pipeline.
- [`layout-quality-and-testing.md`](layout-quality-and-testing.md) — validation, scoring, retries, and tests.

The separate 2.5D runtime is tracked in [`game-roadmap.md`](game-roadmap.md), with continuation details in [`game-handoff.md`](game-handoff.md). It consumes the complete generation chain through immutable `GeneratedLevel` geometry and mutable `LevelSession` run state, renders exact assigned dual-cell floors, retains lockable doorway thresholds, closes unauthorized contacts, builds matching navigation, and spawns the player in Start. The hard-coded deterministic enemy encounter remains available through `F1` and now accepts injected wall geometry; generated-room combat is the next milestone.

## Overview

The program generates an irregular mesh made entirely from quadrilateral faces. It starts from a regular triangular lattice inside a hexagonal boundary, randomly merges neighboring triangles, subdivides every remaining face into quads, and then smooths the result.

The complete pipeline is:

```text
hexagonal set of lattice points
        ↓
regular triangular mesh
        ↓
randomly merge adjacent triangle pairs
        ↓
mixture of triangles and four-sided faces
        ↓
center-and-midpoint subdivision
        ↓
all-quad mesh
        ↓
iterative vertex relaxation
        ↓
organic Stålberg-style grid
        ↓
physical dual-cell and portal measurements
        ↓
graph-first arena and corridor planning
        ↓
clearance-aware route embedding
        ↓
three to six outer-edge-center connections
        ↓
best-of-N validation and scoring
        ↓
connected combat layout with controlled loops
```

The demo colors an automatically generated floor plan over the relaxed grid. A solid-line junction is one logical floor cell; the surrounding quad centers become that cell's dual-polygon corners. Rooms contain many connected cells, connect directly to neighboring rooms, and highlight as one contiguous region when hovered.

## Building and running

Configure and build the project with CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/stalberg_grid
./build/stalberg_game
```

CMake uses an installed raylib package when available. Otherwise it downloads and builds raylib 5.5 through `FetchContent`.

raylib requires a graphical desktop. On a headless Linux machine, `xvfb-run -a ./build/stalberg_grid` can be used as a non-visible smoke test, but an X11/Wayland, VNC, RDP, or forwarded display is required to interact with the demo.

## Controls

| Input | Action |
|---|---|
| `R` | Increment the seed and generate a new grid |
| `G` | Increment the room seed and generate a new floor plan |
| `M` | Cycle shooter, branching-shape, and organic-growth generation |
| Left / Right | Select the previous or next grid seed |
| Up / Down | Increase or decrease the hexagonal patch radius |
| `P` | Toggle quad-center markers |
| `F` | Fit the grid to the window |
| Mouse wheel | Zoom around the mouse cursor |
| Middle/right mouse drag | Pan the camera |

The initial radius is 6. Grid radii are limited to the range 2–14 by the interactive controls.

## Core mesh data

The implementation uses indexed mesh data. Faces refer to integer vertex indices instead of duplicating positions.

### Axial lattice coordinates

```cpp
struct Axial {
    int q;
    int r;
};
```

`Axial` identifies points in the original hexagonal/triangular lattice. It is only needed while constructing the initial mesh.

### Vertices

```cpp
struct Point {
    float x;
    float y;
};

struct Vertex {
    Point position;
    bool fixed;
};
```

A fixed vertex is part of the final boundary and is not moved by relaxation.

### Edges

An `Edge` stores its smaller endpoint first:

```text
Edge(a, b) = (min(a, b), max(a, b))
```

This canonical representation means `(3, 8)` and `(8, 3)` identify the same undirected edge. It is used to:

- Find triangles sharing an edge.
- Share subdivision midpoints between neighboring faces.
- Count final edge usage.
- Build vertex adjacency.

### Faces

The initial mesh uses three-index triangles. The final mesh uses four-index quads:

```cpp
using VertexIndex = std::size_t;
using Triangle = std::array<VertexIndex, 3>;
using Quad = std::array<VertexIndex, 4>;
```

Intermediate faces are `std::vector<VertexIndex>` because they can contain either three or four corners.

## Stage 1: generate a hexagonal triangular lattice

The program uses axial coordinates satisfying the hex-radius constraint:

```text
max(|q|, |r|, |q + r|) ≤ R
```

The code expresses the same constraint by iterating:

```text
q      = -R … R
r_min  = max(-R, -q - R)
r_max  = min( R, -q + R)
r      = r_min … r_max
```

Each axial coordinate is converted to a 2D position using two axes separated by 60 degrees:

```text
x = spacing × (q + r/2)
y = spacing × (√3/2 × r)
```

The demo uses a spacing of 58 world units.

A radius-`R` patch contains:

```text
1 + 3R(R + 1)
```

original lattice points. For the default radius 6, that is 127 points.

The important distinction is that the algorithm does not begin with hexagonal cells. It begins with **points on a triangular lattice whose outer boundary is a hexagon**.

## Stage 2: triangulate the lattice

Every elementary region in the lattice is connected as a triangle. From a lattice coordinate `(q, r)`, the code tests two triangle templates:

```text
up triangle:
    (q, r), (q + 1, r), (q, r + 1)

down triangle:
    (q, r), (q + 1, r - 1), (q + 1, r)
```

A triangle is emitted only when all three points exist inside the hexagonal patch.

A radius-`R` patch contains:

```text
T = 6R²
```

triangles. The default radius 6 therefore starts with 216 triangles.

No Delaunay triangulation is required because connectivity in a regular triangular lattice is already known.

## Stage 3: randomly pair triangles

The program builds a map from each canonical edge to the triangles that use it. An edge with exactly two incident triangles is an interior edge and is a possible merge candidate.

The candidate edges are shuffled with `std::mt19937` using the selected seed. The program then performs a greedy matching:

```text
mark every triangle as unpaired
shuffle all interior edges

for each candidate edge:
    find its two incident triangles A and B
    if A or B is already paired:
        skip the edge
    otherwise:
        remove their shared edge conceptually
        collect their four unique outer vertices
        emit one four-sided face
        mark A and B as paired

emit every unpaired triangle unchanged
```

Two adjacent equilateral triangles always form a convex rhombus, so the initial lattice does not require an additional quad-quality test.

The matching is intentionally incomplete. A greedy selection can leave triangles that no longer have an available unpaired neighbor. This is not an error; the subdivision stage handles both face types.

Random pairing changes mesh **topology**, not just vertex positions. Different seeds create different arrangements of extraordinary vertices and therefore different relaxed patterns.

## Stage 4: subdivide every face into quads

This is the step that guarantees an all-quad result.

For an ordered face with corners

```text
v₀, v₁, …, vₙ₋₁
```

the program creates:

1. A midpoint for every edge:

   ```text
   mᵢ = (vᵢ + vᵢ₊₁) / 2
   ```

2. A face center:

   ```text
   c = (v₀ + v₁ + … + vₙ₋₁) / n
   ```

3. One quad at every original corner:

   ```text
   qᵢ = [vᵢ, mᵢ, c, mᵢ₋₁]
   ```

Indices wrap around the face boundary.

Therefore:

- A triangle produces 3 quads.
- A four-sided face produces 4 quads.

Midpoints are looked up by canonical edge key. Adjacent faces consequently reuse the same midpoint vertex, keeping the mesh connected and watertight.

If `M` triangle pairs were merged from `T` original triangles, then `T - 2M` triangles remain. The final number of quads is:

```text
Q = 4M + 3(T - 2M)
  = 3T - 2M
```

The default radius-6, seed-1 grid currently produces 460 quads.

## Stage 5: rebuild topology and identify the boundary

After subdivision, every quad contributes four edges. The program counts how many quads use each canonical edge:

- Usage count 2: interior edge.
- Usage count 1: boundary edge.

Both endpoints of every boundary edge are marked `fixed`.

The program also builds a neighbor set for every vertex from the final quad edges. This adjacency is used during relaxation and remains unchanged afterward.

Pinning the boundary serves two purposes:

1. It prevents Laplacian smoothing from shrinking the whole patch toward its center.
2. It preserves the exact outer hexagon, which is useful when placing compatible patches next to one another.

## Stage 6: relax the points

The initial all-quad mesh still strongly resembles the triangular lattice. Relaxation moves interior vertices toward the average position of their connected neighbors.

For an interior vertex `i` with neighbor set `N(i)`, the target is:

```text
             1
averageᵢ = ───────  Σ positionⱼ
           |N(i)|  j∈N(i)
```

The updated position is:

```text
positionᵢ' = positionᵢ
           + λ(averageᵢ - positionᵢ)
```

The demo uses:

```text
λ = 0.12
maximum iterations = 240
```

All new positions are computed into a separate array and applied simultaneously. Synchronous updates avoid making the result depend on vertex iteration order.

Fixed boundary vertices do not move.

### Why relaxation creates an organic grid

The random triangle matching creates vertices with different local connectivity. After subdivision, Laplacian smoothing distributes those topological irregularities spatially. Straight lattice rows bend around unusual valences, while ordinary regions settle into more regular four-sided cells.

The connectivity never changes during relaxation. Only geometry changes.

### Relationship to Stålberg’s refined method

This demo implements the commonly used **Laplacian relaxation** version of the technique. Stålberg also discussed relaxation rules that more explicitly encourage equal neighbor distances and square-like faces. A more advanced implementation could add a per-quad square-fitting force or a constrained optimization pass.

Ordinary Laplacian smoothing is simpler and demonstrates the topology-generation pipeline clearly, but it does not mathematically guarantee square quads or equal edge lengths.

## Rendering

The final mesh is rendered inside a raylib `Camera2D`:

- Every unique final edge is drawn once, with a small circle at each vertex to produce clean joins at every valence.
- An oriented cross marks each quad centroid; its four arms point toward the quad's edge midpoints.
- Every solid-line vertex acts as the center of a dual cell whose corners are the surrounding quad centers.
- Every generated region receives a unique translucent fill. Shooter corridors are separate logical rooms but use the same diagnostic rendering style as arenas.
- Shared edges inside one room disappear, leaving a connected exterior outline with quadratic rounded corners.
- Boundaries between regions remain continuous; logical doorways do not create visible wall gaps.
- Hovering one cell highlights its complete connected room.
- Marker size and line thickness are divided by camera zoom, keeping them approximately constant in screen pixels.

Interior dual-cell polygons are formed by angularly ordering the centers of all quads incident on the logical cell. Shared `DualGrid` geometry now supplies polygons, area, center clearance, and shared-boundary width. The adapter combines that data with grid-vertex positions to calculate center-to-center traversal distance for the neutral `RoomGrid`. The integration adapter owns the hex-specific policy that selects six boundary-side centers. The demo completes relaxation before taking this snapshot. The generator combines the room seed with a fingerprint of its neutral topology and physical metrics and supports three methods selectable with `M`.

The default **shooter layout** starts with an abstract mission graph. It anchors start and exit arenas near well-separated selected boundary entrances, distributes additional arena seeds using farthest-point sampling, and grows compact combat rooms around them while retaining negative space. A noise-perturbed Prim-like spatial tree creates the main route and side branches; maps with enough routing capacity may receive one deliberate long-cycle loop. Each planned edge is routed through unoccupied cells with physical costs that penalize low clearance and narrow portals. Longer routes become separate connector rooms and gain lateral cells where space allows. Links shorter than an arena's approximate diameter are folded into an endpoint arena and become direct arena doorways, avoiding a separate tiny connector for every mission-graph edge. At least one route remains an explicit connector, while additional connector identities are reserved for long passages. Opportunistic widening does not mathematically guarantee that every connector is narrower than every arena. Only planned arena/corridor contacts become logical doorways, so incidental physical contact cannot introduce an unintended shortcut.

The legacy **branching shapes** method starts with a central room, uses geometric compact, elongated, branching, irregular, and L-shaped masks, and extends radial branches. The **organic growth** method joins the selected entrances to the center, expands a noisy footprint balanced across six angular sectors, and partitions it with weighted multi-source growth. These legacy methods use a quality-weighted spanning tree plus a bounded loop budget over their resulting room contacts.

All methods select uniformly from every concrete subset containing three to six candidate entrances. There are 20 three-side, 15 four-side, 6 five-side, and 1 six-side subsets, so those counts occur in a proportional 20:15:6:1 ratio. The selected entrance brief remains fixed while several deterministic candidates are generated and scored. Every accepted layout has connected multi-cell regions, distinct start and exit rooms, intentional negative space, and no more than 64 rooms. Each doorway publishes physical width and quality. Rooms publish physical area and a gameplay role: start, exit, combat, connector, hub, or reward. A tactical annotation pass marks wall-adjacent cover candidates and enemy-spawn candidates away from published internal doorway thresholds. Exterior connected entrances require separate downstream filtering. The diagnostic renderer still keeps boundaries visually continuous and rounds the resulting outlines.

The HUD shows:

- Current patch radius.
- Random seed.
- Final vertex count.
- Final quad count.
- Completed relaxation progress, candidate quality, and selected candidate index.

## Camera behavior

`F` computes zoom from `grid.getBounds()` and centers the camera on the current bounds midpoint.

Mouse-wheel zoom preserves the world position under the cursor:

1. Record the world coordinate below the cursor.
2. Change camera zoom.
3. Recalculate the world coordinate below the cursor.
4. Offset the camera target by the difference.

Middle- or right-button dragging translates the camera target in world space.

## Source-code map

The main generation methods in `StalbergGrid` correspond directly to the algorithm stages:

| Method | Responsibility |
|---|---|
| `generate()` | Run the complete generation pipeline |
| `generateHexagonalLattice()` | Create axial points inside the hex boundary |
| `triangulateLattice()` | Connect lattice points into elementary triangles |
| `randomlyPairTriangles()` | Greedily merge random adjacent triangle pairs |
| `orderedFace()` | Put face corners in consistent angular order |
| `subdivideFaces()` | Convert every intermediate face into quads |
| `rebuildTopology()` | Build unique edges, neighbors, and boundary flags |
| `relaxOnce()` | Perform one synchronous smoothing iteration |

Grid generation, room generation, integration, and rendering are separate areas:

| File / function | Responsibility |
|---|---|
| `src/grid/stalberg_grid.hpp` | Public mesh types and read-only topology views |
| `src/grid/stalberg_grid.cpp` | Grid generation, topology rebuilding, and relaxation |
| `src/grid/dual_grid.hpp` | Renderer-independent dual polygons, cell measurements, and shared portal widths |
| `src/rooms/room_grid.hpp` | Grid-independent cells, physical connections, buildability, and entrance candidates |
| `src/rooms/room_generator.cpp` | Graph-first shooter planning, legacy growth methods, weighted routing, controlled circulation, candidate scoring, and role assignment |
| `src/rooms/room_layout.hpp` | Read-only rooms, physical statistics, roles, tactical candidates, assignments, doorways, and quality metadata |
| `src/integration/room_grid_adapter.cpp` | Translate `StalbergGrid` and select its hex-boundary entrances |
| `src/grid_renderer.cpp` / `drawGrid()` | Render room fills, connected rounded boundaries, and dual centers |
| `src/main.cpp` | Compose generation modules, process controls, update, and render the diagnostic demo |
| `src/game/main.cpp` | Run the fixed-step 2.5D traversal/combat loop and compose runtime modules |
| `src/game/generated_level.*` | Retain generation artifacts and build exact floors, walls, doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.*` | Own mutable traversal, room lifecycle/location, dynamic doorway locks, active walls, and reset |
| `src/game/encounter.*` | Order combat simulation against injected walls and reset the complete encounter deterministically |
| `src/game/enemy.*` | Enemy movement, health, damage, fan pattern, and hostile projectile profile |
| `src/game/combat_audio.*` | Own the audio device and generated combat tones |
| `src/game/arena.*` | Define arena walls and resolve circle/projectile wall collision |
| `src/game/collision_2d.*` | Reusable swept-circle, segment, earliest-hit, and containment queries |
| `src/game/player.*` | Player movement, facing, health, damage, invulnerability, and interpolation |
| `src/game/weapon.*` | Fixed fire cadence and facing-marker muzzle spawning |
| `src/game/projectile_pool.*` | Profile-driven preallocated projectile slots, movement, lifetime, reuse, and interpolation |
| `src/game/target.*` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.*` | Orthographic follow camera, camera-relative controls, and ground projection |
| `src/game/game_input.*` | Poll raylib input into simulation-facing `PlayerInput` data |
| `src/game/prototype_renderer.*` | Own game GPU resources and render the current prototype scene |
| `tests/grid_tests.cpp` | Headless grid topology and relaxation tests |
| `tests/room_generation_tests.cpp` | Headless room connectivity, doorway, and determinism tests |
| `tests/generated_level_tests.cpp` | Headless runtime artifact, floor, wall/door, dynamic locking, lifecycle/reset, spawn, and navigation tests |
| `tests/game_tests.cpp` | Headless 2D/arena collision, projectile ownership/profile/pool, weapon cadence, enemy determinism/damage, player damage/invulnerability, death, victory, and restart tests |

## Compact pseudocode

```text
function generate(radius, seed):
    points = makeHexLattice(radius)
    triangles = connectTriangularLattice(points)

    candidates = all edges shared by two triangles
    shuffle(candidates, seed)

    faces = []
    paired = set()

    for edge in candidates:
        a, b = triangles incident to edge
        if a not in paired and b not in paired:
            faces.append(merge(a, b))
            paired.add(a)
            paired.add(b)

    for triangle in triangles:
        if triangle not in paired:
            faces.append(triangle)

    quads = []
    sharedMidpoints = map()

    for face in faces:
        center = average(face corners)
        for edge in face:
            midpoint[edge] = getOrCreateSharedMidpoint(edge)
        for corner in face:
            quads.append([
                corner,
                next edge midpoint,
                center,
                previous edge midpoint
            ])

    count quad edges
    pin vertices belonging to one-use edges
    build vertex-neighbor lists

function relaxOnce():
    for each non-boundary vertex:
        target = average(connected neighbors)
        nextPosition = lerp(position, target, 0.12)
    apply all next positions simultaneously
```

## Current scope and limitations

The generation demo intentionally focuses on a single understandable patch. The separate game executable consumes generated layouts for exact floor rendering, closed wall collision, authorized doorway traversal, dynamic doorway locking, navigation, room lifecycle/location state, and Start spawning. It also preserves the deterministic combat regression arena with injectable walls, profile-separated projectile pools, swept combat collision, player health and invulnerability, death/victory restart, feedback, camera behavior, lighting, audio, and fixed-step timing. The project does not currently implement:

- Infinite chunk generation.
- Cross-chunk relaxation.
- Explicit square-fitting forces.
- Face-quality optimization after relaxation.
- Generated-room combat states, door locking, cover placement, or encounter spawning.
- Mesh export.
- General-purpose three-dimensional asset extrusion beyond runtime floor and wall geometry.

An infinite version would generate compatible hexagonal chunks with deterministic seeds, preserve shared boundary topology, and relax either a larger neighborhood or overlapping chunks so seams remain smooth.
