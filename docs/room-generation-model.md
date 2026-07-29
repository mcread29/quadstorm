# Room Generation Data Model and API

This document describes the renderer-independent contract between grid generation, room generation, and gameplay systems.

- Shooter algorithm: [`shooter-level-generation.md`](shooter-level-generation.md)
- Validation, scoring, retries, and tests: [`layout-quality-and-testing.md`](layout-quality-and-testing.md)
- Base quad-grid algorithm: [`demo-and-algorithm.md`](demo-and-algorithm.md)
- Planned identity metadata and runtime consumption: [`level-identity-pass.md`](level-identity-pass.md)

## Module boundary

The project separates four concerns:

```text
StalbergGrid
    mesh topology + relaxed 2D positions
        ↓
buildDualGrid()
    dual polygons + physical measurements
        ↓
makeRoomGrid()
    owned neutral graph snapshot
        ↓
RoomGenerator::generate()
    assignments + circulation + gameplay metadata
```

Libraries in `CMakeLists.txt` reflect that split:

| Target | Responsibility |
|---|---|
| `stalberg_grid_generation` | Base mesh, relaxation, and dual geometry |
| `stalberg_room_generation` | Grid-independent room algorithms and result model |
| `stalberg_room_grid_adapter` | Translation and hex-specific entrance policy |
| `stalberg_grid` | raylib diagnostic application |

The room-generation library does not depend on raylib or `StalbergGrid`. A different game grid can use `RoomGenerator` by constructing a valid `RoomGrid` directly.

## Finalize geometry before taking the snapshot

Room generation uses physical positions, areas, clearances, distances, and portal widths. Callers should finish visual relaxation before adapting the grid:

```cpp
stalberg::StalbergGrid grid;
grid.generate(radius, gridSeed);
grid.relaxToCompletion();

stalberg::rooms::RoomGrid input = stalberg::makeRoomGrid(grid);
```

`RoomGrid` owns its vectors. Once created, later changes to the source mesh cannot mutate it. `roomInputIsIndependentFromLaterRelaxation()` tests this property.

## Dual geometry

`buildDualGrid()` is declared in `src/grid/dual_grid.hpp`.

```cpp
struct DualCell {
    Point center;
    std::vector<Point> polygon;
    float area;
    float clearance;
};

struct DualConnection {
    Edge cells;
    Point first;
    Point second;
    float length;
};
```

### Logical cells

Every final `StalbergGrid` vertex becomes one logical room cell. The centers of incident quads become that cell's dual-polygon corners.

For a quad with positions `v0...v3`:

```text
quadCenter = (v0 + v1 + v2 + v3) / 4
```

Incident centers are angularly ordered around the logical-cell center. Boundary cells receive reflected ghost centers across the primal outer edge, so those dual polygons can extend beyond the primal boundary. A 3D level builder that needs a strict outer footprint should retain this behavior intentionally or clip boundary polygons.

### Area

Dual-cell area uses the shoelace formula:

```text
area = 0.5 × abs(sum(x[i] × y[i+1] - x[i+1] × y[i]))
```

This area is used for output room statistics and candidate coverage scoring.

### Clearance

Clearance is the minimum Euclidean distance from the logical-cell center to any segment of its dual polygon:

```text
clearance = min(distance(center, polygonEdge[i]))
```

It approximates how much local space exists around the cell center. It is not a full agent-radius navigation clearance field.

### Shared boundary width

Every primal grid edge connects two logical cells. Its corresponding dual segment is:

- Between the two incident quad centers for an interior edge.
- Between the real quad center and its reflection across the primal edge at the boundary.

`DualConnection::length` is the length of that segment. The room layer treats it as physical portal or shared-boundary width.

## Neutral input: `RoomGrid`

`RoomGrid` is declared in `src/rooms/room_grid.hpp`.

```cpp
using CellIndex = std::size_t;

struct CellPoint {
    float x;
    float y;
};

struct Cell {
    CellPoint position;
    float area;
    float clearance;
    bool buildable;
};

struct CellConnection {
    CellIndex cell;
    float distance;
    float sharedBoundaryLength;
};

struct RoomGrid {
    std::vector<Cell> cells;
    std::vector<std::vector<CellIndex>> neighbors;
    std::vector<std::vector<CellConnection>> connections;
    std::vector<CellIndex> entranceCandidates;
};
```

### Cell fields

| Field | Meaning |
|---|---|
| `position` | 2D logical-cell center used for distance and directional calculations |
| `area` | Physical area of the cell's dual polygon |
| `clearance` | Minimum center-to-dual-boundary distance |
| `buildable` | Whether normal room/corridor growth may claim this cell |

The Stålberg adapter maps `buildable = !vertex.fixed`. Boundary cells are therefore normally blocked, while explicitly selected entrances may still be included by entrance routing.

### Topology and physical connections

`neighbors[cell]` is the lightweight index-only graph view. `connections[cell]` contains the same destinations with physical metadata.

For adapter-generated input:

- Connections are symmetric.
- Connection lists are sorted by destination index.
- `distance` is Euclidean distance between the two primal cell centers.
- `sharedBoundaryLength` is the dual segment length.

The generator accepts equivalent input ordering because it validates sets and creates a canonical sorted adjacency internally.

### Entrance candidates

Entrance candidates are explicit policy supplied by the adapter. They are not inferred by `RoomGenerator`.

For the hexagonal Stålberg patch, `boundarySideCenters()` checks six directions:

```text
angle(side) = -π/2 + side × π/3
```

For each side, it selects a fixed boundary vertex by:

1. Greatest projection in the side direction.
2. Least tangent distance within projection tolerance `0.001`.
3. Lowest cell index within tangent tolerance `0.001`.

Duplicate results are removed. Normal generated patches provide six unique candidates.

## Neutral input validation

`topologyIsValid()` rejects malformed input before generation.

### Vector and scalar checks

- `neighbors.size() == cells.size()`.
- `connections.size() == cells.size()`.
- Positions are finite.
- Areas are finite and strictly positive.
- Clearances are finite and nonnegative.
- Connection distances and widths are finite and strictly positive.

### Adjacency checks

For every cell:

- `neighbors` and `connections` describe the same destination set.
- Destination indices are in range.
- Self-connections are forbidden.
- Duplicate destinations are forbidden.
- Every connection has a reverse connection.
- Reverse distance and width match within:

```text
tolerance = max(value, 1) × 0.0001
```

### Entrance checks

- Entrance indices are in range.
- Entrance indices are unique.

The neutral validator does not require exactly six entrances or enforce that entrances belong to a geometric boundary. Those are adapter policies.

## Deterministic input fingerprint

`gridFingerprint()` makes physical geometry part of generation identity. It includes:

- Cell count.
- Exact float bit patterns for each position, area, and clearance.
- Buildability.
- Canonically sorted directed connections, including distance and width bits.
- Canonically sorted entrance candidates.

Equivalent neighbor/connection/entrance ordering therefore does not change generation. Changing relaxed geometry does.

## Public generator API

`src/rooms/room_generator.hpp` exposes:

```cpp
struct RoomGenerationOptions {
    RoomGenerationMethod method = RoomGenerationMethod::ShooterLayout;
    std::size_t candidateCount = 6;
};

class RoomGenerator {
public:
    RoomLayout generate(
        const RoomGrid& grid,
        std::uint32_t seed,
        RoomGenerationMethod method = RoomGenerationMethod::ShooterLayout) const;

    RoomLayout generate(
        const RoomGrid& grid,
        std::uint32_t seed,
        const RoomGenerationOptions& options) const;
};
```

Methods:

| Method | Intended use |
|---|---|
| `ShooterLayout` | Default graph-first arenas, corridors, branches, and controlled loops |
| `BranchingShapes` | Legacy geometric room masks and radial branches |
| `OrganicGrowth` | Legacy connected noisy footprint and weighted multi-source partition |

`candidateCount` is clamped to `[1,32]`. The shooter method may continue to candidate index 31 only when the requested budget found no valid layout.

## Output: `RoomLayout`

`RoomLayout` is declared in `src/rooms/room_layout.hpp`.

```cpp
inline constexpr int EMPTY_CELL = -1;
inline constexpr std::size_t MAX_GENERATED_ROOMS = 64;
```

### Cell assignments

`getCellAssignments()` returns one integer per input cell:

- `EMPTY_CELL`: no generated floor.
- Nonnegative value: dense room ID.

Accepted layouts guarantee that every nonnegative ID indexes `getRooms()` and that each room's cells form one connected component.

`getCellAssignment(cell)` returns `EMPTY_CELL` for an out-of-range index.

### Generated rooms

```cpp
struct GeneratedRoom {
    int id;
    std::size_t cellCount;
    float area;
    RoomRole role;
    std::vector<CellIndex> coverCandidates;
    std::vector<CellIndex> enemySpawnCandidates;
};
```

| Field | Contract |
|---|---|
| `id` | Dense ID equal to the room's index in `getRooms()` |
| `cellCount` | Exact count of cells assigned to this ID |
| `area` | Sum of member dual-cell areas |
| `role` | Gameplay classification |
| `coverCandidates` | Wall-adjacent cells away from published internal doorway thresholds |
| `enemySpawnCandidates` | Positive-clearance cells at least two in-room steps from published internal doorway thresholds |

Roles:

| Role | Meaning |
|---|---|
| `Start` | Player-entry arena |
| `Combat` | Ordinary combat arena |
| `Connector` | Explicit corridor or entrance spur |
| `Hub` | Arena with at least three circulation connections |
| `Reward` | Legacy-method leaf room selected by generic role annotation |
| `Exit` | Goal/transition arena |

Shooter layouts normalize planned arena and corridor roles after common annotation. `Reward` is not currently assigned to shooter arenas.

### Planned identity metadata

`RoomRole` remains a semantic gameplay classification and is not overloaded to describe geometry. Radius-5 shooter layouts now independently publish `SmallMapRecipe` metadata for Hub Circuit, Broken Ring, or Twin Wings. These are regression fixtures for the current systems slice. Production random maps need broader topology metadata, room-shape grammar, and generator-owned semantic anchors so a selected quest recipe can bind to generated roles and anchor types without fixed room IDs or world coordinates.

A layout archetype describes graph structure across rooms. A room shape describes the generated geometry of one substantial room. District or landmark metadata describes presentation/gameplay grouping. These concepts remain separate so, for example, two `Combat` rooms can have different shapes and landmarks without inventing new gameplay roles.

### Doorways

```cpp
struct Doorway {
    int firstRegion;
    CellIndex firstCell;
    int secondRegion;
    CellIndex secondCell;
    float width;
    float quality;
};
```

A doorway identifies one adjacent cell pair crossing two regions. It is logical circulation data, not merely a visual marker.

- The two cell assignments match their corresponding region IDs.
- The cells are physical graph neighbors.
- `width` is the shared dual-boundary length.
- `quality` is normalized to `[0,1]`.

For shooter layouts, only mission-authorized contacts become doorways. A 3D wall or navigation system must not infer openings from room contact alone.

### Layout-level metadata

| Getter | Meaning |
|---|---|
| `getSeed()` | Requested user seed, not internal variant seed |
| `getMethod()` | Requested generation method |
| `getQualityScore()` | Best candidate's weighted score |
| `getSelectedCandidate()` | Winning candidate index |
| `getConnectedEntrances()` | Boundary cell IDs for the exact selected exterior entrances connected to floor |

A selected shooter candidate may have an index above the configured default budget when constrained-grid retries were required.

Connected entrances are not `Doorway` objects. They do not include an exterior segment, width, quality, or opening geometry; they identify boundary floor cells so the game can build its own exterior transition.

## Gameplay consumption

> Runtime status: `stalberg_game` consumes the complete generation chain through immutable `GeneratedLevel`, renders exact assigned floors, retains exact doorway thresholds, and publishes door-aware navigation. Normal play chooses a fresh replayable match seed, derives radius-8 Fortress V1 grid/room inputs at `worldScale = 0.22`, retries candidates within a bounded budget, and visibly reports deterministic radius-5 fixture fallback. `R` resets the same accepted map, `N` requests another, and `--seed` replays one within the same build/toolchain. Larger layouts publish one Hub and one leaf-preferred Reward arena, while application validation checks the current quest plan plus actor-relative physical and spawn-capacity metrics. Semantic quest binding and broader topology/shape validation remain. See [`level-identity-pass.md`](level-identity-pass.md) and [`game-handoff.md`](game-handoff.md).

### Runtime physical scale

`RoomGrid` and `RoomLayout` preserve source physical metrics. `GeneratedLevelConfig::worldScale` converts those metrics into game-world units; it is independent of grid radius. Fortress V1 raises actor-relative world scale from the fixture baseline `0.16F` to `0.22F` without changing player/enemy bodies. Application acceptance measures doorway, substantial/Anchor area, objective clearance, Anchor room-center span, route distance, statically usable cross-room ingress separation, all-open spawn capacity, and Hub doorway degree after conversion, and rejects radius-only growth. Static spawn usability includes all-open reachability, enemy-sized clearance, and immutable-wall distance; current locks, player distance, and occupancy remain runtime checks. The Anchor span is a size proxy rather than a line-of-sight guarantee. Connector-specific, true-sightline, and traversal-time metrics remain future additions.

### Walkability

A cell is floor when its assignment is not `EMPTY_CELL`.

### Door-aware navigation

Within one room, all neighboring assigned cells are traversable. Across different room IDs, traversal is allowed only when that exact cell pair appears in `getDoorways()`.

Do not let AI traverse every physical cross-room adjacency; shooter generation deliberately walls off incidental contacts.

### 3D conversion

Map input coordinates onto a horizontal world plane:

```text
CellPoint.x → world X
CellPoint.y → world Z
height      → world Y
```

`RoomGrid` and `RoomLayout` do not retain dual-polygon vertices or dual-segment endpoints. A level builder must retain the `DualGrid` used during adaptation or rebuild it deterministically from the final `StalbergGrid`. Use `DualGrid::cells[cell].polygon` for floor triangulation and the matching `DualConnection` segment for wall/door geometry. Keep collision and navigation planar even when rendering full 3D assets.

### Tactical metadata

Cover and spawn vectors are candidate sets, not final placements. A horde-map content pass should still apply:

- Agent radius, local occupancy, and simultaneous-enemy limits.
- Line of sight and whether a spawn is visible to the player.
- Path distance through the current permanent-gate and temporary-lock state.
- Round phase, enemy role, composition, and spawn direction.
- Distance from powered devices, quest interactions, and intermission services.
- Cover spacing, orientation, crowd flow, and telegraph readability.
- Door swing, lock, or threshold exclusion zones.
- Explicit filtering around `getConnectedEntrances()` when exterior entrance thresholds also need cover/spawn exclusion.

The tactical BFS is seeded only by published internal `Doorway` endpoints. Exterior connected-entrance cells are not doorway endpoints and are not automatically excluded from the tactical candidate vectors.

## Empty-layout behavior

If input is invalid or no candidate passes generation and validation, the result contains:

- The requested seed and method.
- An assignment vector matching input cell count, filled with `EMPTY_CELL`.
- No rooms, doorways, or connected entrances.
- Quality score `0`.
- Selected candidate index `0`.

Callers should treat `getRoomCount() == 0` as generation failure. The game-level new-match pipeline should report the rejection and try the next deterministic derivation within a bounded budget. Only after that budget is exhausted may it publish a known-valid emergency fixture, and fallback use must be visible in diagnostics rather than silent.
