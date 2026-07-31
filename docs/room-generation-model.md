# Room Generation Data Model and API

This document uses ASD-STE100 Simplified Technical English for explanatory text.

This document describes the renderer-independent contract between grid generation, room generation, and gameplay systems.

- Shooter algorithm: [`shooter-level-generation.md`](shooter-level-generation.md)
- Validation, scoring, retries, and tests: [`layout-quality-and-testing.md`](layout-quality-and-testing.md)
- Base quad-grid algorithm: [`demo-and-algorithm.md`](demo-and-algorithm.md)
- Planned identity metadata and runtime consumption: [`level-identity-pass.md`](level-identity-pass.md)

Current production status:

- Radius-8 production maps require exactly two useful cycles.
- Each production Shortcut must save at least two arena transitions.
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

The libraries in `CMakeLists.txt` use this separation:

| Target | Responsibility |
|---|---|
| `stalberg_grid_generation` | Base mesh, relaxation, and dual geometry |
| `stalberg_room_generation` | Grid-independent room algorithms and result model |
| `stalberg_room_grid_adapter` | Translation and hex-specific entrance policy |
| `stalberg_grid` | raylib diagnostic application |

The room-generation library does not depend on raylib. It also does not depend on `StalbergGrid`. A different game grid can use `RoomGenerator`. The caller must directly construct a valid `RoomGrid`.

## Finalize geometry before taking the snapshot

Room generation uses physical positions. It also uses areas, clearances, distances, and portal widths. Complete visual relaxation before you adapt the grid:

```cpp
stalberg::StalbergGrid grid;
grid.generate(radius, gridSeed);
grid.relaxToCompletion();

stalberg::rooms::RoomGrid input = stalberg::makeRoomGrid(grid);
```

`RoomGrid` owns its vectors. A later source-mesh change cannot change an existing `RoomGrid`. `roomInputIsIndependentFromLaterRelaxation()` tests this property.

## Dual geometry

`src/grid/dual_grid.hpp` declares `buildDualGrid()`.

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

Each final `StalbergGrid` vertex becomes one logical room cell. The centers of incident quads become the corners of the dual polygon for that cell.

For a quad with positions `v0...v3`:

```text
quadCenter = (v0 + v1 + v2 + v3) / 4
```

The program orders incident centers by angle around the logical-cell center. Boundary cells get reflected ghost centers across the primal outer edge. Thus, their dual polygons can extend beyond the primal boundary. The current implementation keeps these full boundary polygons. It does not clip them to the primal boundary. Clipping for a strict outer footprint would be a separate policy change.

### Area

The dual-cell area uses the shoelace formula:

```text
area = 0.5 × abs(sum(x[i] × y[i+1] - x[i+1] × y[i]))
```

Output room statistics use this area. Candidate coverage scoring also uses this area.

### Clearance

Clearance is the minimum Euclidean distance from the logical-cell center to a segment of its dual polygon:

```text
clearance = min(distance(center, polygonEdge[i]))
```

This value estimates the local space around the cell center. It is not a complete navigation-clearance field for an agent radius.

### Shared boundary width

Each primal grid edge connects two logical cells. The related dual segment has one of these forms:

- For an interior edge, it is between the two incident quad centers.
- For a boundary edge, it is between the real quad center and its reflection across the primal edge.

`DualConnection::length` is the length of this segment. The room layer uses this value as a physical portal width or shared-boundary width.

## Neutral input: `RoomGrid`

`src/rooms/room_grid.hpp` declares `RoomGrid`.

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
    std::vector<std::vector<CellConnection>> connections;
    std::vector<CellIndex> entranceCandidates;

    auto neighbors(CellIndex cell) const
    {
        return connections[cell]
            | std::views::transform(
                [](const CellConnection& connection) { return connection.cell; });
    }
};
```

### Cell fields

| Field | Meaning |
|---|---|
| `position` | 2D logical-cell center used for distance and directional calculations |
| `area` | Physical area of the cell's dual polygon |
| `clearance` | Minimum center-to-dual-boundary distance |
| `buildable` | Whether normal room/corridor growth may claim this cell |

The Stålberg adapter sets `buildable = !vertex.fixed`. Thus, boundary cells are usually blocked. Entrance routing can still include explicitly selected entrances.

### Topology and physical connections

`RoomGrid` stores `cells`, `connections`, and `entranceCandidates`. It does not store an independent neighbor vector. `connections[cell]` contains destination indices and physical metadata. `neighbors(cell)` is a non-owning projection of the destination indices in `connections[cell]`.

Adapter-generated input has these properties:

- Connections are symmetric.
- Connection lists are sorted by destination index.
- `distance` is the Euclidean distance between the two primal cell centers.
- `sharedBoundaryLength` is the dual segment length.

The generator accepts equivalent connection-list order. It makes canonical sorted adjacency from the stored connections.

### Entrance candidates

The adapter supplies the entrance-candidate policy. `RoomGenerator` does not infer this policy.

For the hexagonal Stålberg patch, `boundarySideCenters()` checks six directions:

```text
angle(side) = -π/2 + side × π/3
```

For each side, it selects one fixed boundary vertex with this order:

1. Greatest projection in the side direction.
2. Least tangent distance within projection tolerance `0.001`.
3. Lowest cell index within tangent tolerance `0.001`.

The adapter removes duplicate results. Normal generated patches supply six unique candidates.

## Neutral input validation

`topologyIsValid()` rejects malformed input before generation.

### Vector and scalar checks

- `connections.size() == cells.size()`.
- Positions are finite.
- Areas are finite and strictly positive.
- Clearances are finite and nonnegative.
- Connection distances and widths are finite and strictly positive.

### Adjacency checks

For each cell:

- Destination indices are in range.
- Self-connections are not permitted.
- Duplicate destinations are not permitted.
- Each connection has a reverse connection.
- Reverse distance and width agree within:

```text
tolerance = max(value, 1) × 0.0001
```

`topologyIsValid()` derives destination indices from `connections`. It does not compare two independent topology containers.

### Entrance checks

- Entrance indices are in range.
- Entrance indices are unique.

The neutral validator does not require exactly six entrances. It does not require entrances on a geometric boundary. These requirements belong to adapter policy.

## Canonical topology fingerprint

`canonicalTopologyFingerprint()` makes physical geometry part of generation identity. It includes these values:

- Cell count.
- Exact float bit patterns for each position, area, and clearance.
- Buildability.
- Canonically sorted directed connections, including distance and width bits.
- Canonically sorted entrance candidates.

Equivalent connection order within each source cell produces the same fingerprint. Equivalent `entranceCandidates` order also produces the same fingerprint. A change to relaxed geometry changes the fingerprint. A change to connection metadata also changes the fingerprint.

## Public generator API

`src/rooms/room_generator.hpp` supplies this API:

```cpp
struct RoomGenerationOptions {
    RoomGenerationMethod method = RoomGenerationMethod::ShooterLayout;
    std::size_t candidateCount = 6;
    std::optional<SmallMapRecipe> smallMapRecipe;
    std::optional<LargeMapArchetype> largeMapArchetype;
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

The generation methods have these uses:

| Method | Intended use |
|---|---|
| `ShooterLayout` | Default graph-first arenas, corridors, branches, and controlled loops |
| `BranchingShapes` | Legacy geometric room masks and radial branches |
| `OrganicGrowth` | Legacy connected noisy footprint and weighted multi-source partition |

The generator limits `candidateCount` to `[1,32]`. The shooter method can continue to candidate index 31. It does this only if the requested budget has no valid layout.

`smallMapRecipe` can fix the small-map recipe. `largeMapArchetype` can fix the large-map archetype. If an option is empty, the generator makes the related deterministic selection from the seed and grid context.

## Output: `RoomLayout`

`src/rooms/room_layout.hpp` declares `RoomLayout`.

```cpp
inline constexpr int EMPTY_CELL = -1;
inline constexpr std::size_t MAX_GENERATED_ROOMS = 64;
```

### Cell assignments

`getCellAssignments()` returns one integer for each input cell:

- `EMPTY_CELL`: The cell has no generated floor.
- Nonnegative value: The value is a dense room ID.

An accepted layout guarantees that each nonnegative ID indexes `getRooms()`. It also guarantees that the cells of each room make one connected component.

`getCellAssignment(cell)` returns `EMPTY_CELL` if the index is out of range.

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

The roles have these meanings:

| Role | Meaning |
|---|---|
| `Start` | Player-entry arena |
| `Combat` | Ordinary combat arena |
| `Connector` | Explicit corridor or entrance spur |
| `Hub` | Arena with at least three circulation connections |
| `Reward` | Optional reward arena selected by shooter planning or legacy annotation |
| `Exit` | Goal/transition arena |

Shooter layouts normalize planned arena and corridor roles after common annotation. Small recipes reserve their authored Reward arena. Each large shooter layout assigns exactly one Hub. It then promotes a different Combat room to Reward. It selects a leaf Combat room if one is available. Thus, Reward is not a legacy-only role.

### Planned identity metadata

`RoomRole` is a semantic gameplay classification. It does not describe geometry. Radius-5 shooter layouts separately publish `SmallMapRecipe` metadata. The possible recipes are Hub Circuit, Broken Ring, and Twin Wings. Larger layouts publish `LargeMapArchetype` metadata. The possible archetypes are Hub and Spokes, Ring and Branches, Main Spine, Twin Districts, and Dense Core/Sparse Branch.

Larger layouts also publish a deterministic `TopologySignature`. It contains room and connector counts. It contains contracted and direct edge counts. It contains the direct-link ratio and alternation length. It contains the degree histogram and maximum degree. It contains the junction count and cycle rank. It contains useful-cycle count and ordinary Combat leaf count. It contains minimum Shortcut savings. It also contains branch depth and Start-to-Exit distance.

Production radius-8 validation requires `usefulCycleCount == 2`. It also requires `minimumShortcutSavingsTransitions >= 2`. These values are contracted-arena-graph metrics. A useful cycle must have an alternate path with at least three arena transitions. Each production Shortcut must save at least two arena transitions.

The latest audit requested 100 match seeds. It accepted 94 non-fallback production maps. Six requests used the fallback fixture. The mean useful-cycle count across the 94 production maps is 2.00. For each map, the multi-entry ratio is `multiDoorSubstantialRoomCount / substantialRoomCount`. The mean of the 94 per-map ratios is 66.7%. The 70% multi-entry hard gate is not implemented.

Production random maps still need room/combat grammar. They still need generator-owned semantic anchors. The full quest compiler must bind selected quest recipes to generated roles and anchor types. It must not use fixed room IDs or world coordinates. Bounded leaves and physical route separation also remain open.

A layout archetype describes graph structure across rooms. A room shape describes the generated geometry of one substantial room. District or landmark metadata describes a presentation or gameplay group. These concepts are separate. Thus, two `Combat` rooms can have different shapes and landmarks. The project does not need new gameplay roles for these differences.

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

A doorway identifies one adjacent cell pair across two regions. It is logical circulation data. It is not only a visual marker.

- The two cell assignments match their region IDs.
- The cells are physical graph neighbors.
- `width` is the shared dual-boundary length.
- `quality` is normalized to `[0,1]`.

For shooter layouts, only mission-authorized contacts become doorways. A 3D wall or navigation system must not infer openings from room contact alone.

### Layout-level metadata

| Getter | Meaning |
|---|---|
| `getSeed()` | Requested user seed, not internal variant seed |
| `getMethod()` | Requested generation method |
| `hasSmallMapRecipe()` / `getSmallMapRecipe()` | Fixed systems-recipe metadata when selected |
| `hasLargeMapArchetype()` / `getLargeMapArchetype()` | Large-map graph brief when selected |
| `getTopologySignature()` | Measured contracted graph and alternation signature |
| `getQualityScore()` | Best candidate's weighted score |
| `getSelectedCandidate()` | Winning candidate index |
| `getConnectedEntrances()` | Published boundary cell IDs for downstream exterior-entrance policy |

A selected shooter candidate can have an index above the configured default budget. This occurs when constrained-grid retries are necessary.

`getConnectedEntrances()` publishes metadata for downstream use. Connected entrances are not `Doorway` objects. They do not contain an exterior segment. They do not contain width, quality, or opening geometry. They identify selected boundary floor cells. Connected entrances are not current runtime exits. The current runtime does not make an exterior transition from this metadata. Single-owner exterior floor edges become walls.

## Gameplay consumption

> **Runtime status**
>
> `stalberg_game` uses the complete generation chain through immutable `GeneratedLevel`.
> It renders exact assigned floors.
> It keeps exact doorway thresholds.
> It publishes door-aware navigation.
> Normal play selects a new replayable match seed.
> It derives radius-8 **Fortress V1** grid and room inputs at `worldScale = 0.22`.
> It retries candidates within a bounded budget.
> It visibly reports deterministic radius-5 fixture fallback.
> `R` resets the same accepted map.
> `N` requests another map.
> `--seed` replays one map in the same build and toolchain.
> Each production layout publishes a validated graph archetype and signature.
> Each large shooter layout publishes exactly one Hub and one Reward.
> Application validation checks actor-relative physical metrics.
> It checks stage progression.
> It runs the static post-gate spawn-packing check.
> Semantic anchors remain open.
> The full quest compiler remains open.
> Room/combat grammar remains open.
> Bounded leaves remain open.
> Physical route separation remains open.
> Initial-lock and visibility spawn checks remain open.
> See [`level-identity-pass.md`](level-identity-pass.md) and [`game-handoff.md`](game-handoff.md).

### Runtime physical scale

`RoomGrid` and `RoomLayout` keep source physical metrics. `GeneratedLevelConfig::worldScale` converts these metrics to game-world units. This value does not depend on grid radius. **Fortress V1** increases actor-relative world scale from the fixture baseline `0.16F` to `0.22F`. Player and enemy body sizes do not change.

Application acceptance measures doorway width. It measures substantial-room and Anchor area. It measures objective clearance and Anchor room-center span. It measures route distance and statically usable cross-room ingress separation. It measures all-open spawn capacity and Hub doorway degree after conversion. It rejects growth that changes only radius.

Static spawn usability requires all-open reachability. The dual-cell clearance in world units must be at least `ENEMY_RADIUS + 0.08F`. The cell center must stay at least `ENEMY_RADIUS + 0.04F` from each immutable wall. Current locks are runtime checks. Player distance is also a runtime check. Active-wall clearance is a runtime check. Current occupancy is a runtime check. Enemy separation from current occupants is a runtime check. The Anchor span is a size estimate. It does not guarantee line of sight.

Stage progression checks simulate the gate sequence. Each checked gate must be approachable. Its objective must be reachable after the gate opens. A checked progression gate is valid if it adds reachable cells or saves the required route transitions. This is an OR rule. The Reward gate does not require route value. Its relay targets must still be reachable.

The static post-gate spawn-packing check uses deterministic distance packing. Static packing separates candidate centers by at least `ENEMY_RADIUS * 2.0F + 0.08F`. Each checked post-gate Fortress state requires 18 packed candidate slots in at least two rooms. The check uses the simulated reachable component. It does not use player distance. It does not use active walls for spawn clearance. It does not use current occupancy. It does not measure separation from current enemies. Runtime placement requires `ENEMY_RADIUS * 2.0F + 0.12F` from each living enemy. Thus, this admission check does not prove that runtime placement always succeeds. The initial-lock state is not included. Visibility bands and role compatibility are also not included. Connector-specific metrics, true-sightline metrics, and traversal-time metrics remain open.

### Walkability

A cell is floor if its assignment is not `EMPTY_CELL`.

### Door-aware navigation

All adjacent assigned cells in one room are traversable. A pair of cells in different rooms is traversable only if that exact pair is in `getDoorways()`.

Do not let AI traverse each physical cross-room adjacency. Shooter generation intentionally closes incidental contacts with walls.

### 3D conversion

Map input coordinates to a horizontal world plane:

```text
CellPoint.x → world X
CellPoint.y → world Z
height      → world Y
```

`RoomGrid` and `RoomLayout` do not keep dual-polygon vertices. They also do not keep dual-segment endpoints. A level builder must keep the `DualGrid` that adaptation used. As an alternative, it can rebuild the `DualGrid` deterministically from the final `StalbergGrid`. Use `DualGrid::cells[cell].polygon` for floor triangulation. Use the matching `DualConnection` segment for wall and door geometry. Keep collision and navigation planar when you render full 3D assets.

### Tactical metadata

Cover and spawn vectors are candidate sets. They are not final placements. A horde-map content pass must also apply these checks:

- Agent radius, local occupancy, and simultaneous-enemy limits.
- Line of sight and spawn visibility to the player.
- Path distance through the current permanent-gate and temporary-lock state.
- Round phase, enemy role, composition, and spawn direction.
- Distance from powered devices, quest interactions, and intermission services.
- Cover spacing, orientation, crowd flow, and telegraph readability.
- Door swing, lock, or threshold exclusion zones.
- Filtering around `getConnectedEntrances()` if a downstream system adds exterior entrance thresholds.

The tactical BFS starts only at published internal `Doorway` endpoints. Exterior connected-entrance cells are not doorway endpoints. Thus, the tactical candidate vectors do not automatically exclude these cells.

The static post-gate spawn-packing check measures packed capacity in simulated gate states. It does not use player distance, active-wall clearance, current occupancy, or runtime enemy separation. It does not check the initial-lock spawn state. It also does not check visibility bands or role compatibility. Runtime occupancy recovery remains separate work.

## Empty-layout behavior

Input can be invalid. Generation and validation can also reject all candidates. In these cases, the result contains these values:

- The requested seed and method.
- An assignment vector with one `EMPTY_CELL` value for each input cell.
- No rooms, doorways, or connected entrances.
- Quality score `0`.
- Selected candidate index `0`.

Callers must treat `getRoomCount() == 0` as generation failure. The game-level new-match pipeline reports the rejection. It tries the next deterministic derivation in a bounded budget. It can publish a known-valid emergency fixture only after it uses all attempts. Diagnostics must show fallback use.
