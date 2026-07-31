# Graph-First Shooter Level Generation

This document uses ASD-STE100 Simplified Technical English where practical. Code, formulas, identifiers, and required technical terms keep their exact forms.

This document describes the default `RoomGenerationMethod::ShooterLayout` pipeline. The implementation is in `src/rooms/room_generator.cpp`. The document covers the mission graph, arena growth, physical routes, corridors, entrances, doorways, room roles, tactical data, validation, and fallback behavior.

See [`room-generation-model.md`](room-generation-model.md) for the input and output types. That document also gives the physical geometry contract. See [`layout-quality-and-testing.md`](layout-quality-and-testing.md) for validation, scores, retries, determinism, and tests. See [`level-identity-pass.md`](level-identity-pass.md) for the planned room grammar, topology identity, runtime overview, and landmarks.

The `stalberg_game` runtime reads an immutable `GeneratedLevel`. This object keeps the source grid and dual geometry. It also keeps the neutral room graph, topology and recipe metadata, exact floor, and doorway thresholds.

Normal play uses radius 8 and the Fortress V1 profile. Fortress V1 uses `worldScale = 0.22`. `MatchGenerationRequest` supplies the physical profile. The match seed does not select the physical profile. One match seed produces a bounded candidate stream. The seed supports replay only in the same build and toolchain. Fixed radius-5 presets remain systems fixtures and the deterministic fallback.

Large layouts select one of five graph archetypes. These layouts publish a measured topology signature. Application gates validate actor-relative physical scale, stage progression, and static stage-spawn capacity.

The remaining work includes room grammar and bounded leaves. It also includes physical route separation, semantic anchors, and a quest compiler. Initial-lock spawn checks, visibility checks, role-based spawn checks, runtime recovery, and endurance tests also remain.

`LevelSession` owns the authoritative player. It also owns the synchronized collision and navigation locks. `HordeMatch` owns persistent match state.

## Goals

The shooter method supports a 3D twin-stick game. The simulation stays planar. The method has these goals:

- Create several separate combat arenas.
- Create explicit connector regions for visible 3D corridors.
- Separate the start and exit with a useful route.
- Create side branches and hub rooms.
- Create exactly two useful cycles in production radius-8 layouts.
- Keep compact systems fixtures on their zero-or-one-cycle rules.
- Keep negative space for walls, void, scenery, or inaccessible terrain.
- Prefer routes with physical clearance and wide portals.
- Publish one authoritative logical doorway graph.
- Prevent accidental shortcuts.
- Publish stable data for navigation, spawning, cover, and encounters.

The generator uses the irregular dual-cell graph from the Stålberg grid. It does not require square tiles. It does not require a Cartesian grid.

### Current identity limitation and next pass

Large shooter layouts select Hub and Spokes, Ring and Branches, Main Spine, Twin Districts, or Dense Core/Sparse Branch before routing. All substantial rooms still use the same compact growth process. Normal Fortress V1 generation uses radius 8 and a `0.22` generated-to-world scale. Actor-relative gates accept or reject each map. Radius-5 fixtures use `0.16`.

The random new-match flow is implemented. The first large-world boundary is implemented. Topology archetypes and measured graph signatures are implemented.

Production-size layouts require exactly two useful cycles. Each production Shortcut must save at least two arena transitions. Stage progression validation is implemented. Static post-gate spawn packing is also implemented.

Graph-signature scoring is partly implemented. Application candidate ranking scores the multi-entry substantial-room ratio, useful-cycle count, and ordinary Combat leaf ratio. District scoring remains incomplete. Other graph-signature score terms also remain incomplete.

The remaining work in [`level-identity-pass.md`](level-identity-pass.md) includes explicit room-shape grammar and bounded leaves. It includes district construction and district scoring. It also includes semantic quest anchors, a quest compiler, and physical route separation. Initial-lock and visibility checks remain. Role-based spawn checks, runtime recovery, pacing, endurance tests, and cross-seed structure checks also remain.

The six fixed F2 configurations remain inspection fixtures. They are not the normal gameplay pool.

### Horde-map interpretation

The generated mission graph defines spatial structure. It does not require one-time room clearing.

| Generated role | Persistent horde-map responsibility |
|---|---|
| `Start` | Opening survival area and match reset location |
| `Hub` | Central machine, map-state display, and quest convergence |
| `Combat` | Crowd-training arena, Grid Anchor, holdout, or elite site |
| `Connector` | Chokepoint, purchasable route, trap site, or dangerous shortcut |
| `Reward` | Perk, weapon service, quest component, or hidden chamber |
| `Exit` | Warden arena, extraction, or alternate finale |

This table gives possible horde-map uses for each role. It does not describe current recipe assignments.

Only published doorways can become gates. An open gate changes the legal route network. The change applies to the player and all navigating enemies. Incidental physical contacts stay closed as walls. Wide arenas and multiple approaches support crowd movement. Useful cycles support alternate routes. Connectors create controlled pressure and spending choices.

Normal play generates a new validated map from a random match seed. The seed selects the grid and room streams. It also selects the topology archetype and the current recipe identity. `MatchGenerationRequest` supplies the physical profile. Bounded retries reject invalid combinations. Seed replay is valid only in the same build and toolchain.

The current recipe identity does not assign clues or services. It does not publish typed semantic anchors. It is not a full quest plan. The application derives gates from doorway paths and room roles. It selects Hub, Anchor, and Exit objective sites by clearance heuristics. It selects relay cells from the Reward room. Curated seeds are tests and emergency fallback data only.

## Seed compatibility

The grid code canonicalizes topology inputs before seeded triangle pairing. It sorts triangles, pairing candidates, published edges, and neighbor lists. This removes standard-library hash iteration order from seeded generation. This change alters historical grid output for old seeds. For example, radius 6 with seed 1 now has 458 quads instead of 460. The change creates a canonical topology baseline. A topology-fingerprint regression test protects this baseline.

The canonical topology baseline does not make complete seeded replay portable. C++ standard random distributions can map values differently in different standard-library implementations. Seeded room and match replay is therefore supported only in the same build and toolchain.

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
select one fixed small-map gameplay recipe when applicable
        ↓
choose start, exit, and additional arena seeds
        ↓
use spatial-tree, fixed-recipe, or selected-archetype edges
        ↓
grow connected combat arenas around every seed
        ↓
choose route order; put required cycle edges before required tree edges
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

The demo finishes all grid relaxation before it calls `makeRoomGrid()`. Production callers must do the same. Final geometry controls room sizes, route costs, portal widths, and the input fingerprint.

The production game adds an application acceptance phase. This phase derives an application plan from room roles, doorway paths, and clearance heuristics. It derives gates, objective sites, and relays. It does not bind a full quest plan.

The phase validates role availability and gate stages. It validates objective reachability, gate value, static stage-spawn capacity, and actor-relative physical scale. It rejects the complete derived candidate when a requirement fails.

Future work must add room-shape capabilities and immutable semantic anchors. It must also add a full quest compiler. The quest compiler must validate dependency solvability and extraction availability. Opening-component circulation and economy checks remain. Initial-lock spawn capacity, visibility, role compatibility, runtime recovery, and endurance checks also remain. Do not put these application rules in fixed coordinates inside the neutral room generator.

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

`RoomGenerator::generate()` first validates the neutral `RoomGrid`. Invalid topology makes each candidate fail. The generator does not try partial generation. It returns an empty assignment-aligned layout only after it exhausts all permitted variants.

`RoomGrid` stores cells and physical connection lists. It does not store an independent neighbor vector. The generator derives adjacency from each connection destination. It sorts the derived adjacency snapshot. The generator also sorts entrance candidates before it selects the entrance brief.

The fallback generation center is the buildable cell nearest the world origin. It is not the average position of all cells.

See [`room-generation-model.md`](room-generation-model.md#neutral-input-validation) for the neutral-grid invariants.

## Stage 2: choose the fixed entrance brief

The normal hex adapter supplies six candidates. Each candidate is at the center of one boundary side. Generation selects from three to six candidates.

For `n` candidates, count `k` is weighted by the number of concrete subsets:

```text
weight(k) = C(n, k)
```

The generator then shuffles the candidates. The first `k` candidates become the selected entrance set. Six candidates give these weights:

| Selected sides | Concrete subsets | Relative weight |
|---:|---:|---:|
| 3 | 20 | 20 |
| 4 | 15 | 15 |
| 5 | 6 | 6 |
| 6 | 1 | 1 |

Before layout generation, each concrete subset with three to six sides has the same probability.

The generator creates the entrance brief once for each top-level `generate()` call. Each best-of-N candidate must connect the same set. The score cannot replace a difficult entrance. The score cannot change the entrance-count distribution.

## Stage 3: choose start and exit anchors

`interiorCellForEntrance()` uses breadth-first search. The search starts at a selected boundary entrance. It stops at the first buildable interior cell.

The shooter method uses these rules:

1. Use the interior cell for the first selected entrance as the start arena seed.
2. Use the selected entrance interior with the greatest Euclidean distance from the start as the exit arena seed.
3. Use the buildable cell farthest from the start when no separate entrance-derived exit exists.

Start and exit are the first two arena seeds. The spatial-tree fallback does not allow a direct Start-to-Exit edge when it has more than two arenas. Fixed recipe edges and archetype edges use their own contracts.

## Stage 4: choose additional arena seeds

Let `B` be the number of buildable cells.

```text
maximumArenaCount = max(2, B / 8)
minimumArenaCount = B >= 160 ? 5 : 4
desiredArenaCount = min(maximumArenaCount, clamp(B / 55, minimumArenaCount, 20))
```

The formulas use integer division. `maximumArenaCount` can limit the request to two or three arenas on sufficiently small inputs. The lower clamp value of four does not override this maximum. A map with at least 160 buildable cells requests at least five arenas. The limits reserve cells for corridors and entrance spurs. They also help keep the map below the global limit of 64 rooms.

Most additional seeds use noise-weighted farthest-point sampling. The generator calculates this data for each buildable candidate:

```text
nearestDistance = distance to the nearest already selected seed
score = nearestDistance × (0.9 + 0.2 × deterministicNoise)
```

The candidate with the highest score wins. The distance term spreads arenas across the patch. The small noise term changes the composition. It does not override spatial separation.

A map can get one deterministic density feature when it has at least 160 buildable cells and five arenas. The requested room seed fixes the feature type for all best-of-N candidates:

- **Dispersed** (45%): Use normal farthest-point placement and sizing.
- **Landmark** (27.5%): Make one arena about twice the normal target size. Do not select the start or exit arena.
- **Cluster** (27.5%): Put two substantial arenas near one anchor. Use three arenas when at least seven arenas fit.

Cluster satellites use an annulus around a normal anchor. Their preferred center spacing uses the estimated physical cell scale and the normal arena radius. The spacing must leave separate connected footprints and walls. If the annulus has no valid candidate, placement uses farthest-point sampling. A missing cluster location does not fail a required arena.

The generator reserves all requested seeds before room growth. Growth also protects the one-cell ring around every other reserved seed. This stops an earlier arena from enclosing a later seed.

## Stage 5: plan the abstract arena graph

Arena count selects the graph planner:

- Fewer than five arenas use `planSpatialTreeConnections()`.
- Exactly five arenas use `planRecipeArenaConnections()`.
- More than five arenas use `planArenaConnections()` for the selected `LargeMapArchetype`.

### Fewer than five arenas

The spatial-tree fallback starts with arena `0`. It repeatedly connects one unconnected arena to the connected set. Candidate edges use:

```text
cost = physicalDistance × (0.92 + 0.16 × deterministicNoise)
```

The fallback does not use a direct Start-to-Exit edge when it has more than two arenas. It creates only the required tree. It does not add scored loop alternatives.

### Exactly five arenas

The requested room seed fixes one `SmallMapRecipe` for all candidates. The recipe is Hub Circuit, Broken Ring, or Twin Wings. `planRecipeArenaConnections()` publishes the fixed recipe edges before physical routing. Hub is arena `2`. Anchor is arena `3`. Reward is leaf arena `4`. Exit is arena `1`.

Hub Circuit has four required spoke edges. It has no Start → Anchor edge. Broken Ring has a fixed optional Start → Anchor edge. Twin Wings has a fixed optional Anchor → Exit edge. Thus, recipe edges can include Start or Exit. The required recipe graph does not depend on an optional edge.

Tests found a failure in which the first progression gate and the Anchor gate were beside the same destination. Candidate validation now requires a normalized Hub branch-direction dot product of `0.42` or less. This is the exact code threshold. Broken Ring also puts intermediate seeds on a bent Start-to-Exit brief. This helps its chain use separate physical space.

### More than five arenas

`planArenaConnections()` creates the selected `LargeMapArchetype`. The options are a bounded-degree central hub, a ring with branches, a Start-to-Exit spine with side leaves, two local districts with one bridge, or a dense core with a sparse branch.

Production-size neutral grids have at least 600 cells. Normal radius-8 layouts meet this threshold. The planner adds required cycle edges until the graph has `arenaCount + 1` edges. This gives cycle rank two.

For each required cycle edge, the planner closes an existing connector-contracted path of at least three arena transitions. It selects the edge with the highest value:

```text
score = graphDistance / physicalDistance
```

Selection of a required cycle edge can use Start or Exit. It has no Start or Exit exclusion. It does not keep six loop alternatives.

Required cycle edges route before the required tree edges. This order preserves unassigned route space for both required cycle edges. Failure of either required cycle edge rejects the candidate. One non-primary edge has `MissionEdgePurpose::Shortcut`. The other has `MissionEdgePurpose::Cycle`.

A useful cycle is a required cycle edge whose connector-contracted alternate path has at least three arena transitions when that edge is closed. Each accepted production graph has exactly two useful cycles. Its Shortcut saves at least two arena transitions.

## Stage 6: grow combat arenas

The arena budget targets 36% of buildable cells before the generator adds corridors and entrance spurs.

```text
averageTarget = 0.36 × B / requestedArenaCount
variation = uniform random value in [0.82, 1.18]
targetSize = clamp(integer(averageTarget × featureWeight × variation), 4, limit)
```

Normal arenas use weight `1`. A Landmark arena uses weight `2.1`. A Cluster member uses weight `1.2`.

Feature scales act as normalized budget shares. The generator reduces the shares of non-featured arenas. This redistributes the arena budget before random size variation. The Landmark scale, integer conversion, and clamping do not preserve an exact integer target sum.

A normal arena has a target cap of 60 cells. A Landmark arena has a target cap of 96 cells. A density feature does not remove all negative space.

`growArenaRoom()` starts with the seed. It repeatedly selects one connected frontier cell. Candidate score:

```text
scale = max(2 × seedCellClearance, 1)

score = 1.15 × sameRoomNeighborCount
      + 0.45 × clamp(candidateClearance / scale, 0, 1.5)
      - 0.12 × distanceFromSeed / scale
      + 0.35 × deterministicNoise
```

The score uses these effects:

- Same-room neighbors keep the arena compact.
- Clearance improves local physical use.
- The radial penalty limits long tendrils.
- Deterministic noise changes the shape.

Growth uses these constraints:

- Add only frontier cells so the arena stays connected.
- Do not consume a seed that belongs to another arena.
- Reject a frontier cell that touches an assigned room.
- Keep at least four cells in each arena.

The contact rule usually leaves a buffer for walls and corridors. The first reserved seed is always inserted. Exact separation is not possible when two seeds are already adjacent.

The generator rejects the complete candidate when a requested arena cannot reach four cells. It does not change the start, exit, or mission topology.

## Stage 7: physically route arena connections

`routeBetweenRooms()` embeds required graph edges in sequence. It uses multi-source Dijkstra search. Every source-room cell starts in the search frontier.

Only unassigned buildable cells can become route centerline cells. The route cost includes the final transition into the target room.

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

`connectionCost()` has a center-distance fallback for defensive internal use. A valid `RoomGrid` must provide positive physical data for each neighbor. Accepted inputs therefore use their `CellConnection` values.

### Avoiding unrelated rooms

A route cell gets an extra cost when it is near an unrelated assigned region:

```text
stepCost = connectionCost
    × (1 + 2.5 × foreignAssignedNeighborCount)
```

This cost keeps corridors away from unrelated rooms. It also reduces incidental contacts.

The returned path contains only cells between the source and target. Arena cells stay in their arenas.

## Stage 8: materialize explicit corridors

`addCorridorRoom()` converts a routed centerline into room assignments and required doorway pairs.

### Rooms already touch

The generator checks physical contact when routing returns no intermediate cells. It records a direct planned doorway when the rooms touch. If the rooms do not touch, that mission edge fails. Failure of any required mission edge rejects the current candidate. Failure of an optional mission edge omits only that optional edge.

### Short routes

The generator estimates arena diameter from the average target arena size:

```text
maximumDirectConnectionLength = clamp(sqrt(averageTarget), 2, 8)
```

The generator first keeps one explicit connector for shooter structure. After that, it absorbs a route when its length is not greater than this limit. It records a direct source-to-target doorway. Thus, passages get connector identities only when they are long relative to their arenas.

A one-cell first route is too small for a published room. Each room must have at least two cells. The generator tries to add one safe donor cell from an endpoint arena. It protects the start and exit seeds. It also verifies that the donor arena stays connected and has at least two cells. If no donor is safe, the generator absorbs the bridge into the source arena. It then records a direct doorway between source and target.

### Longer routes

The first suitable route becomes a new `GeneratedRoom`. Each route longer than the direct-link limit also becomes a new `GeneratedRoom`. Each connector gets two required doorway relations:

```text
source arena ↔ connector
connector ↔ target arena
```

### Opportunistic widening

`widenCorridor()` visits each interior centerline cell. It does not visit route endpoints. It can add at most one adjacent side cell for each centerline step.

A widening candidate must meet these rules:

- It is unassigned and buildable.
- It is not already in this corridor.
- It does not touch a room other than the two endpoint arenas.

The generator selects the valid candidate with the greatest clearance. Widening is optional. A constrained route can stay one logical cell wide.

## Stage 9: connect the selected exterior entrances

The generator connects each selected exterior entrance after it builds arenas and mission corridors. `pathToFloor()` minimizes the total clearance-aware and width-aware connection cost. The destination does not have to be the floor cell with the shortest Euclidean distance.

The route can include the boundary entrance as its first cell. Fixed boundary cells are normally not buildable.

Entrance paths use these rules:

- If there is no path, mark the candidate incomplete.
- If the entrance is already on floor, record it without new cells.
- If the route is not longer than the active direct-link limit, absorb it into the destination room.
- If the route is longer, create and widen a connector room when possible.
- For a new connector, require a connector-to-destination doorway.

The route-cell count includes the boundary entrance. Thus, a one-cell route often contains the entrance cell directly next to destination floor.

The top-level candidate is valid only when its connected entrance set is exactly equal to the fixed entrance brief.

## Stage 10: open only planned doorways

Physical contact does not automatically create a circulation edge.

`generatePlannedDoorways()` receives canonical required region pairs from mission construction. For each required pair, it finds all adjacent cell pairs across the boundary. It selects the threshold with the highest score:

```text
doorCandidateScore = sharedBoundaryWidth
    + 0.35 × min(firstCellClearance, secondCellClearance)
    + 0.05 × deterministicNoise
```

The generator rejects the candidate when a required pair has no physical contact.

Doorway quality uses the average physical portal width in the complete neutral grid:

```text
quality = clamp(
    (width + 0.35 × minEndpointClearance)
    / max(1.25 × averageGridPortalWidth, 0.001),
    0,
    1)
```

The generator ignores contacts that are not in the mission graph. A 3D wall builder must create openings only for `RoomLayout::getDoorways()`. This rule also applies when two assigned regions touch at another location.

`RoomLayout` stores cell IDs and physical doorway width. It does not store dual-segment endpoints. Geometry code must keep or rebuild the final `DualGrid`. It needs this grid to find the exact wall-opening segment. `getConnectedEntrances()` also stores boundary cell IDs. It does not store exterior doorway geometry.

## Stage 11: assign roles

Common annotation first calculates room area and a preliminary role. Shooter post-processing then sets the mission roles.

Shooter roles:

- The arena with the start seed is `Start`.
- The arena with the exit seed is `Exit`.
- One planned arena is `Hub`.
- Another planned arena is `Combat`.
- Each corridor or entrance-spur room is `Connector`.

Small recipe maps reserve arena `2` as Hub. They reserve arena `4` as Reward.

Large layouts select exactly one Hub. They prefer the non-Start and non-Exit arena with the highest doorway degree. Large layouts also select exactly one Reward. Reward selection prefers a Combat leaf. Other planned arenas stay Combat.

The output uses dense room IDs. Each ID is equal to the room position in `RoomLayout::getRooms()`.

## Stage 12: produce tactical candidates

Doorway endpoint cells start a multi-source BFS. The search stays inside one room.

A candidate cell must be at least one graph step from each doorway threshold in its room.

### Cover candidates

A cell is a cover candidate when it meets these rules:

- Its doorway distance is at least one.
- At least one graph neighbor has a different room assignment, including `EMPTY_CELL`.

These cells are wall-adjacent placement suggestions. They are not cover objects. They do not include orientation or dimensions.

### Enemy spawn candidates

A cell is an enemy-spawn candidate when it meets these rules:

- Its doorway distance is at least two.
- Its physical clearance is positive.

Cover and spawn sets can contain the same cell. Only published internal doorway endpoints start the BFS. Connected exterior entrance cells are not automatically excluded. Gameplay code must filter around `getConnectedEntrances()` when exterior thresholds need exclusion. It must then apply runtime checks for line of sight, player distance, occupancy, cover density, and enemy footprint size.

## Shooter-specific acceptance rules

An accepted shooter candidate must pass general validation. It must also meet these rules:

- It has at least three non-connector rooms.
- It has at least one explicit connector room.
- It has no connector-to-connector doorway.
- Each connector has doorway degree one or two.
- It has one start and one exit.
- A large layout has exactly one Hub and one Reward.
- Its shortest start-to-exit doorway-graph distance is at least three edges.
- A production-size layout has exactly two useful cycles.
- A production-size Shortcut saves at least two arena transitions.
- It meets the constraints for its selected `LargeMapArchetype`.

Tree connectivity and planned doorways put every published room in one circulation graph.

The application acceptance phase checks more rules for Fortress V1. It simulates progression stages. It checks gate approach and objective reachability.

Expansion, Anchor, and Exit gates must add reachable cells or save route transitions. A gate passes this value rule when either condition is true. If it adds no cells, it must save at least two transitions. Reward does not use the gate-value rule.

Each checked post-gate stage must have 18 statically packed enemy spawn slots in at least two rooms. Deterministic packing prevents adjacent candidates from counting as separate slots. This static result does not prove that the runtime spawn scheduler can place all enemies.

Initial-lock capacity, visibility bands, role compatibility, runtime occupancy recovery, and endurance validation remain.

## Scaling behavior

The topology formulas support the demo radius range of 2–14. Radius controls the available cell count. It does not set gameplay scale. `GeneratedLevelConfig::worldScale` converts source geometry to world units. Normal Fortress V1 generation uses radius 8 and `0.22F`. System fixtures use radius 5 and `0.16F`.

Fortress V1 increases map extent and actor-relative physical dimensions. It does not increase player or enemy radii.

Application validation checks world-space and player-relative measurements. These measurements cover doorways, rooms, objectives, room span, routes, ingress, static spawn capacity, Hub degree, and progression stages. A radius-8 map with `0.16F` cannot pass.

Room span is not a true line-of-sight measurement. Post-gate spawn packing does not cover the initial lock state. It does not model player position, visibility, role compatibility, current occupancy, or runtime placement decisions.

Camera framing was widened separately. The game did not apply the geometry multiplier to all systems. Locomotion, dash, projectile reach, interactions, lighting, detail density, navigation performance, runtime recovery, and endurance still need tests and tuning.

### Compact grids

Small grids use the arena-count formula in Stage 4. `maximumArenaCount` can reduce the request to two or three arenas. The lower clamp value applies only when that maximum permits it.

Small grids also use these limits:

- Use a minimum arena target of four cells.
- Use a minimum published-room size of two cells.
- Use direct arena doorways when a separate corridor does not fit.

The normal candidate budget is six. If all normal shooter candidates fail, generation continues in deterministic order through candidate index 31. Only constrained seeds pay this additional cost.

### Large grids

Large grids can request up to 20 arenas. A normal or Cluster arena has a target cap of 60 cells. A Landmark arena has a target cap of 96 cells. The arena budget stays proportional to the buildable-cell count. Corridors and entrance spurs add more floor.

The room-count limit stays at 64. Current sizing keeps typical arena, tree-corridor, cycle, and entrance-spur counts below this limit.

### Very small or malformed inputs

The graph-first shooter layout needs at least eight buildable cells. Invalid topology, insufficient buildable space, or a failed required route rejects the current candidate. A failed required doorway also rejects the current candidate. The top-level generator continues with the next permitted variant. It returns an empty layout only after it exhausts all variants. The empty layout has one `EMPTY_CELL` assignment for each input cell.

The shooter method does not use a legacy fallback algorithm. Such a fallback would change the gameplay meaning of the requested method.

## Compact pseudocode

```text
function generateShooterCandidate(grid, entranceBrief, candidateSeed):
    start = firstBuildableCellFrom(entranceBrief[0])
    exit = farthestSelectedEntranceInteriorFrom(start)

    arenaCount = scaleArenaCount(buildableCellCount)
    seeds = [start, exit]
    while seeds.size < arenaCount:
        seeds += noiseWeightedFarthestBuildableCell(seeds)

    if seeds.size < 5:
        missionEdges = spatialTree(seeds, forbidDirectStartExit=true)
    else if seeds.size == 5:
        missionEdges = fixedRecipeEdges(smallMapRecipe)
    else:
        missionEdges = selectedArchetypeEdges(largeMapArchetype)
        if grid.cellCount >= 600:
            missionEdges += requiredCycleEdges(missionEdges)

    reserve all seeds
    for each seed:
        arena = growCompactConnectedArena(seed)
        require arena.size >= 4

    routeOrder = requiredCyclesBeforeRequiredTree(missionEdges)
    for edge in routeOrder:
        if routeAndMaterialize(edge) fails:
            require edge is optional

    for entrance in entranceBrief:
        require connectEntranceToNearestFloor(entrance)

    require createEveryPlannedDoorway()
    annotateAreasRolesCoverAndSpawns()
    require genericAndShooterValidation()
    return candidate
```

For a production-size graph, both required cycle edges must route successfully. Validation requires exactly two useful cycles. It also requires Shortcut savings of at least two arena transitions.

## Source map

| Function | Responsibility |
|---|---|
| `interiorCellForEntrance()` | Move a boundary entrance to a usable interior arena anchor |
| `growArenaRoom()` | Grow one connected compact combat arena |
| `planSpatialTreeConnections()` | Build the spatial-tree fallback for fewer than five arenas |
| `planRecipeArenaConnections()` | Build fixed recipe edges for exactly five arenas |
| `planArenaConnections()` | Build selected archetype edges for more than five arenas |
| `routeBetweenRooms()` | Find a physical-cost route between two assigned arenas |
| `connectionCost()` | Penalize distance, low clearance, and narrow portals |
| `widenCorridor()` | Opportunistically add high-clearance lateral corridor cells |
| `roomRemainsConnectedWithout()` | Verify a one-cell donor can safely become corridor floor |
| `addCorridorRoom()` | Materialize direct links, bridge corridors, and normal corridors |
| `generateShooterLayout()` | Select the graph planner and coordinate arena, route, cycle, and entrance stages |
| `generatePlannedDoorways()` | Open only mission-authorized room boundaries |
| `annotateRooms()` | Compute areas, roles, cover candidates, and spawn candidates |
| `shooterCandidateIsValid()` | Enforce shooter-specific structural invariants |
| `candidateScore()` | Score valid layouts for best-of-N selection |
