# Stålberg-Style Quad Grid Demo

This document uses ASD-STE100 Simplified Technical English for explanatory text.

This document describes the raylib demo in this repository. It also describes the grid-generation algorithm in `src/grid/stalberg_grid.cpp`.

> **Name note:** This technique is associated with **Oskar Stålberg**. Oskar Stålberg created *Townscaper*. Some sources incorrectly use the name “Peter Stålberg.”

These documents contain the room-generation details:

- [`room-generation-model.md`](room-generation-model.md) describes the physical neutral input and output API.
- [`shooter-level-generation.md`](shooter-level-generation.md) describes the complete graph-first shooter pipeline.
- [`layout-quality-and-testing.md`](layout-quality-and-testing.md) describes validation, scoring, retries, and tests.
- [`level-identity-pass.md`](level-identity-pass.md) describes small-map recipes and the remaining identity work.

[`game-roadmap.md`](game-roadmap.md) tracks the separate 2.5D runtime. [`game-handoff.md`](game-handoff.md) contains continuation details. The runtime puts the generation chain in a replayable match-seed boundary. It uses bounded **Fortress V1** candidate retries. It supports same-map restart. It also supports a separate New Match operation. It validates physical metrics relative to actors. It shows the radius-5 fixture fallback. `F1` starts the hard-coded deterministic arena. This arena is a separate regression wrapper.

Current production status:

- Normal production maps use radius 8.
- Each accepted production map has exactly two useful cycles.
- Each production Shortcut saves at least two arena transitions.
- Stage progression checks exist.
- The static post-gate spawn-packing check exists.
- The latest audit requested match seeds 1 through 100.
- The audit accepted 94 non-fallback production maps.
- Six requests used the fallback fixture.
- The mean per-map multi-entry ratio for the 94 production maps is 66.7%.

Open work includes these items:

- Semantic anchors.
- The full quest compiler.
- Room/combat grammar.
- Bounded leaves.
- Physical route separation.
- Initial-lock spawn checks.
- Visibility spawn checks.

## Overview

The program generates an irregular mesh. All faces in the final mesh are quadrilaterals. The program starts with a regular triangular lattice in a hexagonal boundary. It randomly merges adjacent triangles. It subdivides each remaining face into quads. It then smooths the result.

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

The demo draws a generated floor plan on the relaxed grid. One solid-line junction is one logical floor cell. The adjacent quad centers are the corners of the dual polygon for that cell. One room contains many connected cells. A room connects directly to adjacent rooms. The demo highlights the complete connected room when the pointer is over one of its cells.

## Building and running

Use CMake to configure and build the project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/stalberg_grid
./build/stalberg_game
```

CMake uses an installed raylib package if the package is available. If the package is not available, CMake uses `FetchContent` to download and build raylib 5.5.

raylib needs a graphical desktop. On a headless Linux machine, use `xvfb-run -a ./build/stalberg_grid` for a non-visible smoke test. Use X11/Wayland, VNC, RDP, or a forwarded display to operate the demo.

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

The initial radius is 6. The interactive controls limit the radius to the range 2–14.

## Core mesh data

The implementation uses indexed mesh data. A face refers to integer vertex indices. A face does not contain duplicate vertex positions.

### Axial lattice coordinates

```cpp
struct Axial {
    int q;
    int r;
};
```

`Axial` identifies a point in the initial hexagonal and triangular lattice. The program uses this type only when it constructs the initial mesh.

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

A fixed vertex is on the final boundary. Relaxation does not move a fixed vertex.

### Edges

An `Edge` stores the endpoint with the smaller index first:

```text
Edge(a, b) = (min(a, b), max(a, b))
```

Thus, `(3, 8)` and `(8, 3)` identify the same undirected edge. The program uses this canonical representation for these operations:

- Find triangles that share an edge.
- Share subdivision midpoints between adjacent faces.
- Count final edge use.
- Build vertex adjacency.

### Faces

The initial mesh uses triangles with three indices. The final mesh uses quads with four indices:

```cpp
using VertexIndex = std::size_t;
using Triangle = std::array<VertexIndex, 3>;
using Quad = std::array<VertexIndex, 4>;
```

An intermediate face has the type `std::vector<VertexIndex>`. Thus, it can contain three or four corners.

## Stage 1: generate a hexagonal triangular lattice

The program uses axial coordinates that satisfy this hex-radius constraint:

```text
max(|q|, |r|, |q + r|) ≤ R
```

The code applies the same constraint with these iterations:

```text
q      = -R … R
r_min  = max(-R, -q - R)
r_max  = min( R, -q + R)
r      = r_min … r_max
```

The program converts each axial coordinate to a 2D position. The two axes have an angle of 60 degrees:

```text
x = spacing × (q + r/2)
y = spacing × (√3/2 × r)
```

The demo uses a spacing of 58 world units.

A radius-`R` patch contains this number of initial lattice points:

```text
1 + 3R(R + 1)
```

The default radius is 6. Thus, the default patch contains 127 points.

The algorithm does not start with hexagonal cells. It starts with points on a triangular lattice. The outer boundary of the lattice is a hexagon.

## Stage 2: triangulate the lattice

The program connects each elementary lattice region as a triangle. For a lattice coordinate `(q, r)`, the code tests two triangle templates:

```text
up triangle:
    (q, r), (q + 1, r), (q, r + 1)

down triangle:
    (q, r), (q + 1, r - 1), (q + 1, r)
```

The program emits a triangle only if all three points are in the hexagonal patch.

A radius-`R` patch contains this number of triangles:

```text
T = 6R²
```

Thus, the default radius-6 patch starts with 216 triangles.

The program does not need Delaunay triangulation. The regular triangular lattice has known connectivity.

## Stage 3: randomly pair triangles

The program maps each canonical edge to the triangles that use the edge. An interior edge has exactly two incident triangles. Each interior edge is a possible merge candidate.

The program uses the selected seed with `std::mt19937`. It shuffles the candidate edges. It then does this greedy matching:

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

Two adjacent equilateral triangles always make a convex rhombus. Thus, the initial lattice does not need another quad-quality test.

The greedy matching is intentionally incomplete. A greedy selection can leave a triangle without an available unpaired neighbor. This result is valid. The subdivision stage accepts triangles and four-sided faces.

Random pairing changes the mesh topology. It does not only change vertex positions. Different seeds create different arrangements of extraordinary vertices. These arrangements produce different relaxed patterns.

## Stage 4: subdivide every face into quads

This stage makes the result an all-quad mesh.

For an ordered face with these corners:

```text
v₀, v₁, …, vₙ₋₁
```

the program creates these items:

1. A midpoint for each edge:

   ```text
   mᵢ = (vᵢ + vᵢ₊₁) / 2
   ```

2. A center for the face:

   ```text
   c = (v₀ + v₁ + … + vₙ₋₁) / n
   ```

3. One quad at each initial corner:

   ```text
   qᵢ = [vᵢ, mᵢ, c, mᵢ₋₁]
   ```

The indices wrap around the face boundary.

The results are:

- A triangle produces 3 quads.
- A four-sided face produces 4 quads.

The program finds midpoints with the canonical edge key. Thus, adjacent faces use the same midpoint vertex. This rule keeps the mesh connected and watertight.

Assume that the program merges `M` triangle pairs from `T` initial triangles. Then `T - 2M` triangles remain. The final number of quads is:

```text
Q = 4M + 3(T - 2M)
  = 3T - 2M
```

The canonical radius-6, seed-1 grid produces 458 quads.

## Stage 5: rebuild topology and identify the boundary

After subdivision, each quad contributes four edges. The program counts the quads that use each canonical edge:

- Usage count 2: interior edge.
- Usage count 1: boundary edge.

The program marks both endpoints of each boundary edge as `fixed`.

The program also makes a neighbor set for each vertex. It uses the final quad edges for this set. Relaxation uses this adjacency. The adjacency does not change after this stage.

The fixed boundary has two functions:

1. It stops Laplacian smoothing from shrinking the complete patch toward the center.
2. It keeps the exact outer hexagon for compatible adjacent patches.

## Stage 6: relax the points

The initial all-quad mesh still looks similar to the triangular lattice. Relaxation moves each interior vertex toward the average position of its connected neighbors.

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

The demo uses these values:

```text
λ = 0.12
maximum iterations = 240
```

The program calculates all new positions in a separate array. It then applies all positions at the same time. These synchronous updates make the result independent of vertex iteration order.

Fixed boundary vertices do not move.

### Why relaxation creates an organic grid

Random triangle matching creates vertices with different local connectivity. Laplacian smoothing distributes these topology differences across the mesh. Straight lattice rows bend near interior vertices with valence not equal to 4. Regions around interior vertices with valence 4 become more regular four-sided cells.

Relaxation does not change connectivity. It changes only geometry.

### Relationship to Stålberg’s refined method

This demo uses the common **Laplacian relaxation** version of the technique. Stålberg also described rules that more directly make neighbor distances equal. Those rules also make faces more similar to squares. An advanced implementation can add a square-fitting force for each quad. It can also use constrained optimization.

Laplacian smoothing is simpler. It shows the topology-generation pipeline clearly. It does not mathematically guarantee square quads. It also does not guarantee equal edge lengths.

## Rendering

The program renders the final mesh in a raylib `Camera2D`:

- It draws each unique final edge one time.
- It draws a small circle at each vertex for clean joins at all valences.
- An oriented cross marks each quad centroid.
- The four cross arms point toward the quad edge midpoints.
- Each solid-line vertex is the center of one dual cell.
- The adjacent quad centers are the corners of that dual cell.
- Each generated region has a unique translucent fill.
- Shooter corridors are separate logical rooms.
- Shooter corridors use the same diagnostic style as arenas.
- Shared edges in one room are not visible.
- The result is one connected outer outline with quadratic rounded corners.
- Boundaries between regions stay continuous.
- Logical doorways do not make visible wall gaps.
- A pointer over one cell highlights the complete connected room.
- Marker size and line thickness are divided by camera zoom.
- Thus, their screen size stays approximately constant.

The geometry and adapter sequence is:

- The program orders incident quad centers by angle around each logical cell.
- This operation makes the interior dual-cell polygons.
- Shared `DualGrid` geometry supplies polygons, area, center clearance, and shared-boundary width.
- The adapter combines these values with grid-vertex positions.
- The adapter calculates center-to-center traversal distance for the neutral `RoomGrid`.
- The integration adapter selects six boundary-side centers.
- The demo completes relaxation before it takes the `RoomGrid` snapshot.
- `canonicalTopologyFingerprint()` includes neutral topology and physical metrics in generation identity.
- The generator combines this fingerprint with the room seed.

The generator supports three methods. Use `M` to select a method.

The default **shooter layout** starts with an abstract mission graph. It puts the start and exit arenas near selected boundary entrances. These entrances are far from each other. It uses farthest-point sampling for additional arena seeds. It grows compact combat rooms around these seeds. It keeps negative space between rooms.

The shooter planner selects graph structure by map size:

- Five-arena systems maps use the selected small recipe.
- Larger maps use one of five validated graph archetypes.
- Compact four-arena layouts use a noise-perturbed Prim-like spatial tree.

The shooter planner uses these routing rules:

- It routes each planned edge through unoccupied cells.
- Physical costs increase for low clearance and narrow portals.
- Longer routes become separate connector rooms.
- The planner adds adjacent cells along a connector route if space is available.
- This operation widens the connector.
- For a short link, the planner assigns the route cells to one endpoint arena.
- The short link then becomes a direct arena doorway.
- This rule prevents a small connector for each mission-graph edge.
- At least one route stays as an explicit connector.
- The planner reserves additional connector identities for long passages.
- Opportunistic widening does not guarantee that each connector is narrower than each arena.
- Only planned arena and corridor contacts become logical doorways.
- Incidental physical contact cannot make an unintended shortcut.

Radius-5 maps select Hub Circuit, Broken Ring, or Twin Wings before placement. These maps stay as regression fixtures. Normal **Fortress V1** generation uses radius 8 and `worldScale = 0.22`. It uses the larger spatial planner. Each large shooter map assigns exactly one Hub. It promotes a different Combat room to Reward. It selects a leaf Combat room for Reward if one is available. Actor-relative metric gates reject candidates with dimensions below the required limits. They also reject candidates with insufficient capacity.

Production radius-8 maps have exactly two useful cycles. A useful cycle has an alternate path with at least three arena transitions. Each production Shortcut saves at least two arena transitions. Candidate validation rejects maps that do not meet these limits.

Stage progression checks validate the gate sequence. A checked progression gate is valid if it adds reachable cells or saves the required route transitions. The gate must also be approachable. Its objective must be reachable after the gate opens. The Reward gate does not require route value. It must still be approachable. Its relay targets must be reachable.

The static post-gate spawn-packing check requires 18 packed candidate slots in at least two rooms for each checked Fortress state. The check uses the simulated reachable component. It does not use player distance. It does not use active walls for spawn clearance. It does not use current occupancy. It does not measure separation from current enemies. Thus, the check does not prove that runtime placement always succeeds.

The audit accepted 94 non-fallback production maps from 100 requested match seeds. For each map, the multi-entry ratio uses all substantial rooms in that map as its denominator. The mean of the 94 per-map ratios is 66.7%. The generator does not yet enforce the 70% target.

The identity pass is not complete. These items remain open:

- Semantic anchors.
- The full quest compiler.
- Room/combat grammar.
- Bounded leaves.
- Physical route separation.
- District metadata.
- Additional topology and quest recipes.
- Initial-lock spawn checks.
- Visibility spawn checks.

See [`level-identity-pass.md`](level-identity-pass.md).

The legacy **branching shapes** method starts with a central room. It uses compact, elongated, branching, irregular, and L-shaped geometric masks. It extends radial branches. The **organic growth** method connects the selected entrances to the center. It expands a noisy footprint across six balanced angular sectors. It partitions the footprint with weighted multi-source growth. These legacy methods make a quality-weighted spanning tree. They then add a bounded number of loops from room contacts.

All methods select uniformly from each concrete subset that contains three to six candidate entrances. There are 20 three-side subsets. There are 15 four-side subsets. There are 6 five-side subsets. There is 1 six-side subset. Thus, the counts have a proportional `20:15:6:1` ratio. The selected entrance brief stays fixed while the generator makes and scores deterministic candidates.

Each accepted layout has connected multi-cell regions. It has different start and exit rooms. It has intentional negative space. It has no more than 64 rooms. Each doorway publishes physical width and quality. Each room publishes physical area and a gameplay role. The roles are start, exit, combat, connector, hub, and reward.

A tactical annotation pass marks wall-adjacent cover candidates. It also marks enemy-spawn candidates away from published internal doorway thresholds. `getConnectedEntrances()` publishes metadata for downstream use. The current runtime does not use these cells as exits. Single-owner exterior floor edges become walls. The diagnostic renderer keeps region boundaries continuous. It also rounds the region outlines.

The HUD shows:

- Current patch radius.
- Random seed.
- Final vertex count.
- Final quad count.
- Completed relaxation progress.
- Candidate quality.
- Selected candidate index.

## Camera behavior

`F` gets the bounds from `grid.getBounds()`. It calculates the zoom. It centers the camera on the bounds midpoint.

Mouse-wheel zoom keeps the same world position under the cursor:

1. Record the world coordinate below the cursor.
2. Change camera zoom.
3. Calculate the world coordinate below the cursor again.
4. Move the camera target by the difference.

A middle-button or right-button drag moves the camera target in world space.

## Source-code map

The main `StalbergGrid` methods map directly to the algorithm stages:

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

The project keeps grid generation, room generation, integration, and rendering in separate areas:

| File / function | Responsibility |
|---|---|
| `src/grid/stalberg_grid.hpp` | Public mesh types and read-only topology views |
| `src/grid/stalberg_grid.cpp` | Grid generation, topology rebuilding, and relaxation |
| `src/grid/dual_grid.hpp` | Renderer-independent dual polygons, cell measurements, and shared portal widths |
| `src/rooms/room_grid.hpp` | Grid-independent cells, stored physical connections, non-owning neighbor projection, buildability, entrance candidates, and canonical topology fingerprint |
| `src/rooms/room_generator.cpp` | Graph-first shooter planning, legacy growth methods, weighted routing, controlled circulation, candidate scoring, and role assignment |
| `src/rooms/room_layout.hpp` | Read-only rooms, physical statistics, roles, tactical candidates, assignments, doorways, and quality metadata |
| `src/integration/room_grid_adapter.cpp` | Translate `StalbergGrid` and select its hex-boundary entrances |
| `src/grid_renderer.cpp` / `drawGrid()` | Render room fills, connected rounded boundaries, and dual centers |
| `src/main.cpp` | Compose generation modules, process controls, update, and render the diagnostic demo |
| `src/game/main.cpp` | Run the fixed-step 2.5D traversal/combat loop and compose runtime modules |
| `src/game/generated_level.*` | Retain generation artifacts and build exact floors, walls, doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.*` | Own mutable traversal, room lifecycle/location, dynamic doorway locks, active walls, and reset |
| `src/game/horde_match.*` | Own the automatic endless director, bounded recipe-aware difficulty, economy/gates/tiered upgrades/Hub repair, scaled horde combat, concurrent objectives, extraction, and reset |
| `src/game/generated_encounter.*` | Preserve the earlier generated-room regression coordinator, deterministic multi-spawn filtering, and collection locking/clearing coverage |
| `src/game/combat.*` | Promote player attack state and update caller-owned regression players, enemies, and projectile pools against injected walls |
| `src/game/encounter.*` | Order regression-arena combat against injected walls and reset the complete encounter deterministically |
| `src/game/enemy.*` | Enemy movement, health, single-enemy damage, fan pattern, and hostile projectile profile |
| `src/game/enemy_collection.*` | Stable enemy identities/order, collection movement/firing, earliest swept-hit selection, and all-defeated queries |
| `src/game/combat_audio.*` | Own the audio device and generated combat tones |
| `src/game/arena.*` | Define arena walls and resolve circle/projectile wall collision |
| `src/game/collision_2d.*` | Reusable swept-circle, segment, earliest-hit, and containment queries |
| `src/game/player.*` | Player movement, dash, facing, health, damage, invulnerability, and interpolation |
| `src/game/weapon.*` | Fixed fire cadence and facing-marker muzzle spawning |
| `src/game/projectile_pool.*` | Profile-driven preallocated projectile slots, movement, lifetime, reuse, and interpolation |
| `src/game/target.*` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.*` | Orthographic follow camera, camera-relative controls, and ground projection |
| `src/game/game_input.*` | Poll raylib input into simulation-facing `PlayerInput` data |
| `src/game/game_renderer.*` | Own game GPU resources and render the game scene |
| `tests/grid_tests.cpp` | Headless grid topology and relaxation tests |
| `tests/room_generation_tests.cpp` | Headless room connectivity, doorway, and determinism tests |
| `tests/grid_renderer_cache_tests.cpp` | `stalberg_grid_renderer_cache_tests` coverage for dual-cell count, center-point hit testing, and rejection of a misaligned room layout |
| `tests/generated_level_tests.cpp` | Headless runtime artifact, floor, wall/door, dynamic locking, lifecycle/reset, traversal firing/preservation, deterministic multi-spawn/identity, partial/all-enemies clear, hostile cleanup, and navigation tests |
| `tests/game_tests.cpp` | Headless dash/wall collision, projectile ownership/profile/pool, weapon cadence, single-enemy and collection determinism/damage, earliest-hit/identity tie-breaking, simultaneous defeat, closed-wall containment, player damage/invulnerability, death, victory, and restart tests |
| `tests/horde_match_tests.cpp` | Automatic director, bounded scaling/schedules, overflow safety, recipe economy/gates, scaled enemies, concurrent objectives, extraction, tiered upgrades/Hub repair, and complete reset tests |
| `tests/match_generation_tests.cpp` | `stalberg_match_generation_tests` coverage for match-seed replay, fixed briefs, bounded attempts, deterministic fallback, physical gates, progression stages, the static post-gate spawn-packing check, rejection records, and best-valid ranking |

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

The generation demo uses one easy-to-inspect patch. The separate game executable uses generated layouts. It renders exact floors. It uses closed-wall collision. It permits traversal only through authorized doorways. It supports dynamic doorway locks. It supplies navigation and room lifecycle state. It tracks room location. It creates the Start spawn. It supports traversal firing and dashing. It runs the automatic endless horde match.

The game also keeps two regression paths. One path is the earlier generated-room coordinator. The other path is the deterministic combat arena. These paths support injected walls and separate projectile profiles. They use swept combat collision. They support player health, invulnerability, death, victory, and restart. They also include feedback, camera behavior, lighting, audio, and fixed-step timing.

The project does not implement these items:

- Infinite chunk generation.
- Cross-chunk relaxation.
- Explicit square-fitting forces.
- Face-quality optimization after relaxation.
- Full tuning of the radius-8, `0.22F` Fortress V1 profile for movement, projectiles, interactions, lighting, detail density, and navigation performance.
- Large-map crowd and navigation scaling beyond current ingress and spawn-capacity gates.
- Semantic anchors for generated production maps.
- A full quest compiler for immutable semantic anchors.
- Published room/combat grammar.
- Published district metadata.
- Additional topology and quest recipes.
- Bounded ordinary Combat leaves for all archetypes.
- Physical centerline and doorway-angle separation for alternate routes.
- Initial-lock spawn-capacity checks.
- Visibility-band and role-compatible spawn checks.
- Cross-seed structural-diversity acceptance.
- Traps, multiple services, multi-anchor recipe-authored quests, bespoke bosses, or alternate extraction choices.
- Mesh export.
- General-purpose three-dimensional asset extrusion beyond runtime floor and wall geometry.

Implemented runtime systems and admission checks include these items:

- Gate economy.
- Tiered upgrades.
- Automatic endless director.
- Bounded Elite-event scaling.
- Anchor/Hub/relay quest.
- Voluntary Exit extraction.
- Stage progression validation.
- The static post-gate spawn-packing check is implemented for admission.

The static post-gate spawn-packing check does not guarantee runtime placement.

An infinite version can generate compatible hexagonal chunks with deterministic seeds. It must keep shared boundary topology. It must relax a larger neighborhood or overlapping chunks. This rule keeps the seams smooth.
