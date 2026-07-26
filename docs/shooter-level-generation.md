# Graph-First Shooter Level Generation

This document describes the default `RoomGenerationMethod::ShooterLayout` pipeline implemented in `src/rooms/room_generator.cpp`. It covers mission-graph construction, arena placement and growth, physical routing, corridor materialization, entrance handling, planned doorways, role assignment, tactical annotations, and fallback behavior.

For the input/output types and physical geometry contract, see [`room-generation-model.md`](room-generation-model.md). For candidate validation, scoring, deterministic retries, and tests, see [`layout-quality-and-testing.md`](layout-quality-and-testing.md).

## Goals

The shooter method is designed for a 3D twin-stick game whose simulation remains planar. It aims to produce:

- Several distinct combat arenas rather than one continuous organic blob.
- Explicit connector regions that can become visible 3D corridors.
- A start and exit separated by a meaningful circulation route.
- Side branches and hub rooms.
- At most one deliberate alternate-route loop.
- Negative space between rooms for walls, void, scenery, or inaccessible terrain.
- Routes that prefer physical clearance and wide portals.
- An authoritative logical doorway graph without accidental shortcuts.
- Stable gameplay metadata for navigation, spawning, cover, and encounter systems.

The generator works over the irregular dual-cell graph produced by the Stålberg grid. It does not require square tiles or a regular Cartesian grid.

## Complete pipeline

```text
fully relaxed StalbergGrid
        ↓
build dual polygons and physical portal measurements
        ↓
create an owned, neutral RoomGrid snapshot
        ↓
validate and canonicalize neutral topology
        ↓
select one fixed three-to-six entrance brief
        ↓
choose start, exit, and additional arena seeds
        ↓
plan a spatial arena tree and optional loop candidates
        ↓
grow connected combat arenas around every seed
        ↓
route required tree edges through unoccupied cells
        ↓
materialize and opportunistically widen connector regions
        ↓
connect every selected exterior entrance to existing floor
        ↓
open only mission-graph-required doorways
        ↓
assign roles and tactical candidate cells
        ↓
validate and score the candidate
        ↓
keep the best deterministic candidate
```

The demo completes all grid relaxation before calling `makeRoomGrid()`. Production callers should do the same because room sizes, routing costs, portal widths, and the input fingerprint all depend on final geometry.

```cpp
stalberg::StalbergGrid grid;
grid.generate(radius, gridSeed);
grid.relaxToCompletion();

const auto roomGrid = stalberg::makeRoomGrid(grid);
const auto layout = stalberg::rooms::RoomGenerator {}.generate(
    roomGrid,
    roomSeed,
    stalberg::rooms::RoomGenerationMethod::ShooterLayout);
```

## Stage 1: validate and normalize the input

`RoomGenerator::generate()` first validates the neutral `RoomGrid`. Invalid topology produces an empty, assignment-aligned layout rather than attempting partial generation.

The generator creates a sorted adjacency snapshot from physical connections. This makes traversal independent of caller-supplied neighbor order. Entrance candidates are also sorted before the entrance brief is selected.

The generation center used as a fallback is the buildable cell nearest world origin. It is not the average of all cell positions.

Detailed neutral-grid invariants are documented in [`room-generation-model.md`](room-generation-model.md#neutral-input-validation).

## Stage 2: choose the fixed entrance brief

The normal hex adapter supplies six boundary-side-center candidates. Generation selects between three and six of them.

For `n` candidates, count `k` is weighted by the number of concrete subsets:

```text
weight(k) = C(n, k)
```

The candidates are then shuffled and the first `k` become the selected entrance set. With six candidates:

| Selected sides | Concrete subsets | Relative weight |
|---:|---:|---:|
| 3 | 20 | 20 |
| 4 | 15 | 15 |
| 5 | 6 | 6 |
| 6 | 1 | 1 |

This makes every concrete subset containing three to six sides equally likely before layout generation.

The entrance brief is generated once per top-level `generate()` call. Every best-of-N candidate must connect exactly that same set. Candidate scoring therefore cannot silently replace a difficult entrance or bias entrance-count distribution.

## Stage 3: choose start and exit anchors

`interiorCellForEntrance()` performs a breadth-first search from a selected boundary entrance until it reaches a buildable interior cell.

The shooter method uses:

1. The first selected entrance's interior cell as the start arena seed.
2. The interior cell associated with the selected entrance farthest in Euclidean distance from the start as the exit arena seed.
3. The buildable cell farthest from the start as a fallback when no distinct entrance-derived exit exists.

Start and exit are always the first two arena seeds. Later mission-graph planning forbids a direct start-to-exit tree edge when more than two arenas exist.

## Stage 4: choose additional arena seeds

Let `B` be the number of buildable cells.

```text
maximumArenaCount = max(2, B / 8)
desiredArenaCount = min(maximumArenaCount, clamp(B / 55, 4, 20))
```

Integer division is used. The second cap leaves room for corridor regions and entrance spurs while staying below the global 64-room limit.

Additional seeds are selected by noise-weighted farthest-point sampling. For each buildable candidate:

```text
nearestDistance = distance to the nearest already selected seed
score = nearestDistance × (0.9 + 0.2 × deterministicNoise)
```

The highest score wins. The distance term distributes arenas across the available patch; the small noise term varies composition without overpowering spatial separation.

Every requested seed is reserved before room growth so another arena cannot consume it.

## Stage 5: plan the abstract arena graph

`planArenaConnections()` runs before arena geometry is grown.

### Required tree

A Prim-like process starts from arena `0` and repeatedly attaches one unconnected arena. Candidate tree edges use:

```text
cost = physicalDistance × (0.92 + 0.16 × deterministicNoise)
```

The direct `(start, exit)` edge is forbidden when there are more than two arenas. This encourages the main route to pass through intermediate combat spaces.

The resulting tree provides:

- Global connectivity.
- The main start-to-exit route.
- Side branches.
- Natural hub arenas where tree degree is at least three.

Every tree edge is required. If any required tree route cannot be embedded, the candidate is rejected.

### Optional loop

Maps with at least five arenas may receive one deliberate loop. Loop endpoints:

- Cannot be the start or exit arena.
- Cannot already share a tree edge.
- Must be at least three tree edges apart.

Candidate score:

```text
loopScore = 1000 × treeGraphDistance - physicalDistance
```

This strongly prefers edges that close a long graph cycle while using a relatively short physical route. Up to six loop candidates are retained as routing alternatives. They are attempted in score order after the tree; only the first successfully embedded loop is kept.

A failed loop does not invalidate an otherwise valid tree. Shooter layouts therefore contain zero or one deliberate loop.

## Stage 6: grow combat arenas

The arena budget targets 36% of buildable cells before corridors and entrance spurs are added.

```text
averageTarget = 0.36 × B / requestedArenaCount
variation = uniform random value in [0.82, 1.18]
targetSize = clamp(integer(averageTarget × variation), 4, 60)
```

`growArenaRoom()` begins with the seed and repeatedly chooses one connected frontier cell. Candidate score:

```text
scale = max(2 × seedCellClearance, 1)

score = 1.15 × sameRoomNeighborCount
      + 0.45 × clamp(candidateClearance / scale, 0, 1.5)
      - 0.12 × distanceFromSeed / scale
      + 0.35 × deterministicNoise
```

This balances:

- Compactness through same-room neighbor count.
- Physically usable local geometry through clearance.
- A weak radial penalty that discourages long tendrils.
- Controlled shape variation.

Growth constraints:

- The arena remains connected because only frontier cells are added.
- Reserved seeds cannot be consumed by another arena.
- Frontier additions touching an already assigned room are rejected, usually preserving a buffer for walls and corridors. The initial reserved seed is inserted unconditionally, so exact separation is not guaranteed when two seeds are already adjacent.
- Every arena must contain at least four cells.

If any requested arena cannot reach four cells, the whole candidate is rejected rather than silently changing start, exit, or mission topology.

## Stage 7: physically route arena connections

Required graph edges are embedded sequentially with `routeBetweenRooms()`. The search is multi-source Dijkstra initialized with every source-room cell.

Only unassigned buildable cells may become route centerline cells. The transition into the target room is included in route cost.

### Base connection cost

For a physical cell connection:

```text
edgeDistance = max(connection.distance, 0.001)

preferredClearance = edgeDistance × 0.32
clearancePenalty =
    max(0, preferredClearance - destination.clearance)
    / preferredClearance

preferredWidth = edgeDistance × 0.55
widthPenalty =
    max(0, preferredWidth - sharedBoundaryLength)
    / preferredWidth

connectionCost = edgeDistance
    × (1
       + 2.5 × clearancePenalty
       + 1.8 × widthPenalty²)
```

`connectionCost()` contains a center-distance fallback for defensive internal use, but every valid `RoomGrid` must provide positive physical metadata for every neighbor, so accepted inputs use their `CellConnection` values.

### Avoiding unrelated rooms

A route cell close to unrelated assigned regions receives an additional multiplier:

```text
stepCost = connectionCost
    × (1 + 2.5 × foreignAssignedNeighborCount)
```

This discourages corridors from scraping along other rooms or creating incidental contacts.

The returned path contains only the cells between source and target; arena cells themselves remain in their arenas.

## Stage 8: materialize explicit corridors

`addCorridorRoom()` translates a routed centerline into room assignments and required doorway pairs.

### Rooms already touch

If routing returns no intermediate cells, generation checks whether the source and target physically touch. If they do, it records a direct required doorway. If they do not, the required tree edge failed.

### Short routes

The generator estimates an arena's diameter from the average target arena size:

```text
maximumDirectConnectionLength = clamp(sqrt(averageTarget), 2, 8)
```

After retaining one explicit connector for shooter structure, a route no longer than this limit is absorbed into the source arena and recorded as a direct source-to-target doorway. Separate connector identities are therefore reserved for passages that are long relative to their arenas instead of being inserted on nearly every mission-graph edge.

When the first routed connection contains only one cell, it is too small to publish directly because every room must contain at least two cells. The generator attempts to pair the bridge with a safe donor cell from an endpoint arena. It protects the start and exit seeds and verifies that the donor arena remains connected with at least two cells. If no safe donor exists, the bridge is absorbed into the source arena and the source/target doorway is recorded directly.

### Longer routes

The first suitable routed connection and every route longer than the direct-link limit become new `GeneratedRoom` instances. Each receives two required doorway relationships:

```text
source arena ↔ connector
connector ↔ target arena
```

### Opportunistic widening

`widenCorridor()` visits every interior centerline cell, excluding route endpoints. It may add at most one adjacent side cell per centerline step.

A widening candidate must:

- Be unassigned and buildable.
- Not already be selected for this corridor.
- Avoid contact with a room other than the two endpoint arenas.

Among valid candidates, the one with greatest clearance is selected. Widening is opportunistic; constrained routes may remain one logical cell across.

## Stage 9: connect the selected exterior entrances

After arena and mission-corridor construction, each selected exterior entrance is connected to the existing floor with minimum accumulated clearance- and width-aware connection cost through `pathToFloor()`. This need not be the floor cell with the smallest Euclidean distance.

The boundary entrance itself may be included as the first route cell even though fixed boundary cells are normally non-buildable.

Entrance path handling:

- No path: mark the candidate incomplete.
- Already on floor: record the entrance without adding cells.
- A route no longer than the active direct-link limit: absorb it into the destination room.
- A longer route: create and opportunistically widen a connector room, then require a connector-to-destination doorway.

The route-cell count includes the boundary entrance itself, so a one-cell route is commonly the entrance cell directly adjacent to destination floor.

The top-level candidate is accepted only when its connected entrance set exactly equals the fixed entrance brief.

## Stage 10: open only planned doorways

Physical contact does not automatically imply circulation.

`generatePlannedDoorways()` receives canonical required region pairs from mission construction. For each required pair, it finds every adjacent cell pair crossing that boundary and selects the threshold maximizing:

```text
doorCandidateScore = sharedBoundaryWidth
    + 0.35 × min(firstCellClearance, secondCellClearance)
    + 0.05 × deterministicNoise
```

If a required pair has no physical contact, the candidate is rejected.

Published doorway quality is normalized against average physical portal width across the complete neutral grid:

```text
quality = clamp(
    (width + 0.35 × minEndpointClearance)
    / max(1.25 × averageGridPortalWidth, 0.001),
    0,
    1)
```

Contacts not listed in the mission graph are ignored. A future 3D wall builder should therefore create openings only for `RoomLayout::getDoorways()`, even when two differently assigned regions touch elsewhere.

`RoomLayout` stores cell IDs and physical doorway width, but not dual-segment endpoints. Geometry consumers must retain or rebuild the final `DualGrid` to recover the exact segment used for a wall opening. `getConnectedEntrances()` similarly contains boundary cell IDs rather than exterior doorway geometry.

## Stage 11: assign roles

Common annotation first computes room area and a preliminary role. Shooter post-processing then makes mission semantics authoritative.

Shooter roles:

- Arena containing the start seed: `Start`.
- Arena containing the exit seed: `Exit`.
- Other planned arenas with doorway degree at least three: `Hub`.
- Other planned arenas: `Combat`.
- Every explicit corridor or entrance-spur room: `Connector`.

`Reward` is currently produced only by the legacy methods' generic role inference; shooter arenas are normalized to the roles above.

The output always uses dense room IDs equal to the room's position in `RoomLayout::getRooms()`.

## Stage 12: produce tactical candidates

Doorway endpoint cells seed a multi-source BFS that remains inside each room.

For a cell to be considered, it must be at least one graph step from every doorway threshold in its room.

### Cover candidates

A cell becomes a cover candidate when:

- Its doorway distance is at least one.
- At least one graph neighbor has a different room assignment, including `EMPTY_CELL`.

These are wall-adjacent placement suggestions. They are not instantiated cover objects and do not include orientation or dimensions.

### Enemy spawn candidates

A cell becomes an enemy-spawn candidate when:

- Its doorway distance is at least two.
- Its physical clearance is positive.

Cover and spawn candidate sets may overlap. The BFS is seeded only by published internal doorway endpoints; connected exterior entrance cells are not automatically excluded. A gameplay content pass should filter around `getConnectedEntrances()` when exterior thresholds need exclusion, then apply additional checks such as line of sight, distance from the player, occupancy, cover density, and enemy footprint size.

## Shooter-specific acceptance rules

In addition to general layout validation, an accepted shooter candidate must have:

- At least three non-connector rooms.
- At least one explicit connector room.
- No connector-to-connector doorway.
- Connector doorway degree of one or two.
- One start and one exit.
- A shortest start-to-exit doorway-graph distance of at least three edges.

Tree connectivity and planned-doorway generation ensure every published room participates in one circulation graph.

## Scaling behavior

The same formulas support the demo's radius range of 2–14.

### Compact grids

Small grids are constrained by:

- A minimum of four requested arena seeds when capacity permits.
- A four-cell arena target minimum.
- A two-cell published-room minimum.
- Possible direct arena doorways when there is not enough space for a separate corridor.

The normal candidate budget is six. If all normal shooter candidates fail, generation continues deterministically up to candidate index 31. This additional work is paid only for constrained seeds that did not produce any valid candidate in the requested budget.

### Large grids

Large grids can request up to 20 arenas and target up to 60 cells per arena. The arena budget remains proportional to buildable-cell count, while additional floor comes from corridors and entrance spurs.

The room-count ceiling remains 64. Current shooter sizing is chosen so typical arena, tree-corridor, optional-loop, and entrance-spur counts remain below that limit.

### Very small or malformed inputs

Fewer than eight buildable cells cannot produce the graph-first shooter layout. Invalid topology, missing buildable space, failed required routes, failed required doorways, or exhaustion of all candidate variants returns an empty layout with one `EMPTY_CELL` assignment per input cell.

The shooter method does not silently fall back to a legacy algorithm because that would change gameplay semantics for the same requested method.

## Compact pseudocode

```text
function generateShooterCandidate(grid, entranceBrief, candidateSeed):
    start = firstBuildableCellFrom(entranceBrief[0])
    exit = farthestSelectedEntranceInteriorFrom(start)

    arenaCount = scaleArenaCount(buildableCellCount)
    seeds = [start, exit]
    while seeds.size < arenaCount:
        seeds += noiseWeightedFarthestBuildableCell(seeds)

    missionEdges = spatialTree(seeds, forbidDirectStartExit=true)
    loopAlternatives = usefulLongCycleEdges(missionEdges, excludingStartExit=true)

    reserve all seeds
    for each seed:
        arena = growCompactConnectedArena(seed)
        require arena.size >= 4

    for each required tree edge:
        route = physicalDijkstra(edge)
        require route success
        materializeDirectDoorOrConnector(route)

    for loop in loopAlternatives:
        if routeAndMaterialize(loop) succeeds:
            break

    for entrance in entranceBrief:
        require connectEntranceToNearestFloor(entrance)

    require createEveryPlannedDoorway()
    annotateAreasRolesCoverAndSpawns()
    require genericAndShooterValidation()
    return candidate
```

## Source map

| Function | Responsibility |
|---|---|
| `interiorCellForEntrance()` | Move a boundary entrance to a usable interior arena anchor |
| `growArenaRoom()` | Grow one connected compact combat arena |
| `planArenaConnections()` | Build the required spatial tree and optional loop alternatives |
| `routeBetweenRooms()` | Find a physical-cost route between two assigned arenas |
| `connectionCost()` | Penalize distance, low clearance, and narrow portals |
| `widenCorridor()` | Opportunistically add high-clearance lateral corridor cells |
| `roomRemainsConnectedWithout()` | Verify a one-cell donor can safely become corridor floor |
| `addCorridorRoom()` | Materialize direct links, bridge corridors, and normal corridors |
| `generateShooterLayout()` | Coordinate arena, graph, route, loop, and entrance stages |
| `generatePlannedDoorways()` | Open only mission-authorized room boundaries |
| `annotateRooms()` | Compute areas, roles, cover candidates, and spawn candidates |
| `shooterCandidateIsValid()` | Enforce shooter-specific structural invariants |
| `candidateScore()` | Score valid layouts for best-of-N selection |
