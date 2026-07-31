# Layout Quality, Validation, Determinism, and Testing

This document uses ASD-STE100 Simplified Technical English where practical. Code, formulas, identifiers, commands, and required technical terms keep their exact forms.

This document explains candidate seeds, validation, scores, selection, retries, and tests. The room generator gives the same result for the same requested seed and inputs.

The game adds a match generator. This generator accepts a `MatchGenerationRequest`. The request supplies the physical profile. The match seed derives the layout inputs, topology archetype, and current recipe identity. The match seed does not select the physical profile. The generator accepts only a complete result that passes gameplay validation.

- Shooter construction stages: [`shooter-level-generation.md`](shooter-level-generation.md)
- Input/output model: [`room-generation-model.md`](room-generation-model.md)
- Base grid algorithm: [`demo-and-algorithm.md`](demo-and-algorithm.md)
- Planned identity/diversity expansion: [`level-identity-pass.md`](level-identity-pass.md)

## Why generation uses multiple candidates

Procedural room growth and route embedding are constrained optimization problems. One deterministic random stream can cause these problems:

- An arena seed has too little space for growth.
- A required mission edge has no route through the remaining empty cells.
- An entrance cannot reach floor after corridor placement.
- Doorways are narrow although the graph is valid.
- Floor coverage is outside the score target although the graph is valid.
- The circulation graph is short or too centralized.

The generator does not force malformed geometry through repair steps. It creates deterministic variants. It rejects invalid variants. It scores valid variants. It publishes the valid variant with the highest score.

## Stable requested seed versus candidate variant seed

The public room seed identifies the requested layout. `RoomLayout::getSeed()` keeps this seed for all candidate results. Normal gameplay derives the room seed from a new match seed. Replay and tests supply the match seed directly. Lower-level diagnostics can still use the room seed.

### Grid fingerprint

The candidate stream uses a fingerprint of the neutral input. The fingerprint includes these values:

- Cell count.
- Exact position, area, and clearance bits.
- Buildability.
- Canonically sorted connection destinations, distances, and widths.
- Canonically sorted entrance candidates.

A change to grid topology changes candidate generation. A change to relaxation, physical metrics, or entrance policy has the same effect.

### Candidate seeds

Candidate `0` uses the requested seed directly:

```text
variantSeed(0) = requestedSeed
```

For candidate index `i > 0`:

```text
variantSeed(i) = low32(
    mix(
        (requestedSeed << 32)
        xor mix(0x53484f4f544552 + i)))
```

The PRNG seed inside one candidate is:

```text
generationSeed = low32(mix(gridFingerprint xor variantSeed))
```

`mix()` uses SplitMix64-style hashing. `unitNoise(seed,salt)` maps the low 16 mixed bits to `[0,1]`. It divides these bits by `65535`.

Generation is repeatable for the same implementation, standard-library behavior, input bits, method, and seed. Seeded replay is supported only in the same build and toolchain. It is not portable across standard-library implementations. C++ standard random distributions do not guarantee the same value mapping. Do not use the seed alone as portable serialized layout data.

## Fixed entrance brief

The generator creates the entrance set once before candidate variants.

1. Sort the entrance candidates in canonical order.
2. Select a count between `min(3,n)` and `min(6,n)`.
3. Use `C(n,k)` as the count weights.
4. Shuffle the candidates.
5. Use the first selected count as the fixed brief.

Every candidate must connect exactly this set. Best-of-N scoring cannot select an easier set.

Tests verify these properties:

- All 42 concrete three-to-six subsets occur in 600 representative seeds.
- Counts for three, four, five, and six sides decrease strictly.
- This order agrees with the expected combination weights.
- The test does not apply a statistical tolerance to the exact `20:15:6:1` ratio.
- A change to candidate count does not change the entrance brief.

## Fixed shooter density mode

On a sufficiently large map, the requested room seed fixes the density mode. The mode is dispersed, one landmark arena, or a cluster of two or three arenas. The candidate variant seed does not select this mode. Thus, best-of-N scoring cannot compare one requested density mode with another. Variants can change the featured non-start/exit arena. They can also change the exact cluster location.

Compact maps always use dispersed placement. A cluster candidate can fail to find a safe satellite location. In this case, it uses farthest-point placement. This fallback protects mission validity.

## Candidate budget and constrained-grid retries

Public options use these defaults:

```cpp
RoomGenerationOptions {
    .method = RoomGenerationMethod::ShooterLayout,
    .candidateCount = 6
};
```

The requested count is clamped to `[1,32]`.

Normal behavior uses this sequence:

1. Generate all candidates in the requested budget.
2. Skip each invalid candidate.
3. Score each valid candidate.
4. Keep the highest score.
5. Keep the earlier candidate for an exact score tie because replacement uses strict `>`.

Shooter fallback behavior uses these rules:

- If the requested budget has no valid shooter candidate, continue through candidate index `31` in deterministic order.
- If the requested budget has a valid candidate, do not run the additional retries.
- Legacy methods do not extend their requested budgets.

This fallback supports compact or constrained grids. `getSelectedCandidate()` can therefore be greater than the configured default count.

If all allowed candidates fail, the generator returns an empty assignment-aligned layout. It does not use a legacy method.

## Neutral input validation

`RoomGrid` stores cells, connection lists, and entrance candidates. It does not store an independent neighbor vector. `RoomGrid::neighbors()` projects neighbor indices from connection destinations.

Before candidate generation, `topologyIsValid()` requires these properties:

- The connection-list count is equal to the cell count.
- Positions are finite.
- Areas are finite and positive.
- Clearances are finite and not negative.
- Each connection list has no duplicate destination.
- Connections do not reference their source cell.
- Distances and shared-boundary lengths are finite and positive.
- Each connection has a symmetric reverse connection.
- Reverse metrics agree within `max(value,1) × 0.0001`.
- Entrance candidates are unique and in range.

Invalid input makes each candidate constructor return an empty assignment. The top-level deterministic loop still runs its configured attempts. Shooter mode can still continue to its fallback limit. It then returns the final empty layout.

## General candidate validation

`candidateIsValid()` applies to all three room-generation methods.

### Assignment invariants

- Assignment count is equal to input cell count.
- Each value is `EMPTY_CELL` or a valid room ID.
- Room count is from 2 through 64.
- Room IDs are dense and equal to vector positions.
- Each room has at least two cells.
- Measured assignment count is equal to `GeneratedRoom::cellCount`.
- The assigned cells of each room form one connected component.

### Doorway invariants

- Doorway count is at least `roomCount - 1`.
- Region IDs are valid.
- Endpoint indices are valid.
- Endpoint assignments match the doorway region IDs.
- Doorway endpoint cells are graph neighbors.

### Circulation and role invariants

- Exactly one room has role `Start`.
- Exactly one room has role `Exit`.
- The doorway graph reaches every room.

Connected floor geometry is not sufficient. Accepted circulation must use a connected graph of published logical doorways.

## Shooter-specific validation

`shooterCandidateIsValid()` adds these rules:

- The layout has at least three non-connector rooms.
- The layout has at least one connector room.
- The layout has no connector-to-connector doorway.
- Each connector has doorway degree one or two.
- Start and exit are present.
- The shortest start-to-exit doorway-graph distance is at least three edges.
- Radius-5 recipe layouts have exactly five substantial rooms.
- Radius-5 recipe semantics use Start `0`, Exit `1`, Hub `2`, Anchor `3`, and Reward `4`.
- Each recipe materializes its exact required contracted arena edges.
- Only Broken Ring and Twin Wings can add their declared optional edge.
- Hub Circuit has no optional Start → Anchor edge.
- Start and Anchor cannot become adjacent entrances to the same destination room.
- Reward stays a leaf.
- The locked Reward threshold is not on the required Start-to-Exit route.
- Start and Anchor branch directions from Hub have a normalized dot product not greater than `0.42`.
- The `0.42` value is the exact code threshold.
- A large layout has exactly one Hub and one Reward.
- Large-layout Reward selection prefers a Combat leaf.
- A production-size layout has exactly two useful cycles. A production-size neutral grid has at least 600 cells.
- Production validation uses the useful-cycle definition in [`shooter-level-generation.md`](shooter-level-generation.md#more-than-five-arenas).
- A production Shortcut saves at least two arena transitions.
- Each production archetype meets its degree, cycle, branch-depth, and junction rules.

These rules reject a graph that has valid topology but a weak shooter route. They also reject the tested failure in which two progression branches became adjacent doors to almost the same place.

## Production match acceptance

The game wraps neutral generation in an application pipeline. The default attempt budget is eight. `MatchGenerationRequest` supplies the physical profile. One public match seed derives the grid seeds and room seeds. It also derives the topology archetype and current recipe identity. These identity values stay fixed across retries.

Repeated runs with the same request reproduce the accepted layout, profile, and retry count in the same build and toolchain. This replay guarantee does not apply across standard-library implementations. `R` keeps the current result. `N` requests a new seed. Budget exhaustion selects a visible deterministic systems fixture.

Fortress V1 uses radius 8 and `worldScale = 0.22`. Production room validation requires exactly two useful cycles. The Shortcut must save at least two arena transitions.

The current recipe identity is not a full quest plan. It does not assign clues or services. It does not publish typed semantic anchors.

The application derives its current plan from room roles and doorway paths. It uses the first suitable Combat room as Anchor. A stage-safe search can try other Combat rooms. It uses the highest-clearance cells in Hub, Anchor, and Exit as objective sites. It selects three relays from sorted Reward cover candidates. It uses Reward room cells when there are not enough cover candidates.

Application acceptance requires Start, Hub, Anchor, Reward, and Exit rooms. It requires three relay targets and all four gate purposes. It also requires a Hub with at least three published doorway edges.

Actor-relative gates check doorway width and substantial-room area. They check Anchor area, objective clearance, Anchor room span, and Start-to-Exit route distance. They also check usable cross-room ingress separation, usable spawn candidates, and spawn-bearing rooms.

All-open static spawn filtering requires enemy-sized source clearance. It also requires reachability and distance from immutable walls. A radius-8 map with `0.16` fails.

Stage progression validation is implemented. It simulates gate states. It checks that a gate is approachable before it opens. It checks objective reachability at each stage.

Expansion, Anchor, and Exit gates must add reachable cells or save route transitions. A gate passes when either condition is true. If it adds no cells, it must save at least two transitions. Reward does not use this gate-value rule.

Stage-spawn validation is implemented. It measures usable spawn cells in each checked gate state. Deterministic distance packing prevents adjacent cells from counting as separate simultaneous slots. Each checked post-gate Fortress stage must have 18 packed slots in at least two rooms.

This is a static admission check. It does not prove that the runtime spawn scheduler can place the scheduled enemies. It does not model runtime occupancy, player position, visibility, or role compatibility.

These gates prove actor-relative size and all-open static capacity. They prove the specified progression-stage conditions. They also prove the specified post-gate static packing conditions. They do not prove runtime placement or full runtime endurance.

Room grammar remains incomplete. Leaf counts are not yet bounded for all archetypes. Physical route separation is not measured. The full semantic-anchor and quest-compiler system remains.

Opening-component circulation and economy checks remain. Initial-lock capacity and visibility bands remain. Role compatibility, runtime occupancy recovery, and endurance tests also remain.

### Implemented graph measurements

- Published large-map topology archetype.
- Archetype-specific degree and cycle constraints.
- Contracted arena-edge count.
- Direct arena-edge count.
- Direct arena-to-arena edge ratio.
- Longest alternating arena and connector chain.
- Connector count.
- Substantial-room count.
- Multi-door substantial-room count.
- Meaningful-junction count.
- Degree histogram.
- Maximum degree.
- Cycle rank.
- Useful-cycle count.
- Minimum Shortcut savings in arena transitions.
- Ordinary Combat leaf count.
- Branch depth.
- Start-to-Exit distance.

Graph-signature scoring is partly implemented. Application candidate ranking scores the multi-entry substantial-room ratio, useful-cycle count, and ordinary Combat leaf ratio. District scoring remains incomplete. Graph-community separation and other district score terms also remain incomplete. Bounded-leaf construction remains incomplete.

### Physical and gameplay measurements

Implemented measurements cover doorway width and room footprint. They cover objective clearance, Anchor room-center span, and route travel. They also cover usable cross-room ingress separation, all-open spawn capacity, stage progression, and post-gate static spawn packing.

World and player-relative units are available where applicable. Room span is only a size proxy. It is not proof of line of sight. Static spawn packing is not proof of runtime placement.

The remaining room measurements include compactness, elongation, concavity, lobes, and necks. Doorway angular spread and connector dimensions also remain. Physical route separation, true sightline bands, and traversal time remain.

Opening-component capacity and initial-lock spawn capacity remain. Role compatibility, runtime recovery, and endurance also remain.

A full quest compiler must validate semantic roles and route order. It must validate route separation, objective capacity, and optional branches. It must also validate dependency solvability and extraction availability. It must not use fixed coordinates.

Future cross-seed validation must include topology, room-shape, and objective-placement signatures. This variation must not add nondeterminism within one build and toolchain. Identical match requests must publish identical layouts, metadata, scores, and current application plans in that environment.

### Latest 100-seed production audit

The latest audit covers 100 match requests. Of these requests, 94 produce accepted Fortress maps. The other 6 requests use fallback.

```text
requests=100
accepted Fortress maps=94
fallback requests=6%
mean attempts=3.29
accepted-map mean score=58.751
accepted-map mean contracted cycles=2.00
accepted-map mean useful cycles=2.00
accepted-map mean multi-entry substantial rooms=66.7%
accepted-map progression safe=94/94 (100%)
accepted-map static stage-spawn safe=94/94 (100%)
```

Fallback percentage and mean attempts use all 100 requests. Score, cycle, multi-entry, progression-safe, and stage-spawn-safe values use only the 94 accepted Fortress maps.

The 58.751 value is the accepted-map mean application score. A proposed 70% multi-entry hard gate was not retained. The accepted-map mean is 66.7%. Bounded-leaf construction must improve before that hard gate is practical.

## Application candidate score

Application selection gives each valid match candidate a score out of 100. It uses four domains:

```text
40  circulation
25  physical margin
25  progression
10  neutral generator quality
───
100 total
```

Circulation uses the multi-entry substantial-room ratio, useful-cycle count, and leaf ratio. Physical margin uses the accepted margin above nine profile minimums. Progression uses newly reachable cells and route savings across gate stages. Neutral generator quality contributes 10% of the neutral score below.

## Neutral candidate quality score

Every valid room candidate gets a neutral score on a 100-point scale.

```text
20  coverage
20  mean doorway width
16  minimum doorway width
12  room-area variation
10  loop completion
16  circulation quality
 6  entrance completion
───
100 total
```

## Coverage: 20 points

```text
coverage = assignedFloorArea / buildableInputArea
coverageScore = clamp(1 - abs(coverage - 0.48) / 0.16, 0, 1)
points = 20 × coverageScore
```

The target is 48% physical-area coverage. The score is zero at or below 32% coverage. It is also zero at or above 64% coverage.

The numerator includes all assigned cells. It includes selected fixed boundary entrances. The denominator includes only buildable input cells. Thus, entrance floor can change coverage by a small amount.

## Mean doorway width: 20 points

The generator calculates average portal width from undirected buildable-to-buildable input connections.

For each selected doorway:

```text
ratio = clamp(
    doorwayWidth / max(averagePortalWidth, 0.001),
    0,
    1.5)

normalizedDoorway = ratio / 1.5
```

```text
meanDoorwayScore = mean(normalizedDoorway)
points = 20 × meanDoorwayScore
```

A width greater than `1.5 × averagePortalWidth` gets no more points.

## Minimum doorway width: 16 points

```text
minimumWidthScore = min over doorways(min(ratio, 1))
points = 16 × minimumWidthScore
```

This component stops a high mean from hiding one narrow bottleneck.

## Room-area variation: 12 points

Let `A` be room physical area.

```text
meanArea = mean(A)
variation = sqrt(mean((A - meanArea)²)) / meanArea
variationScore = clamp(
    1 - abs(variation - 0.55) / 0.55,
    0,
    1)
points = 12 × variationScore
```

The score prefers a coefficient of variation near `0.55`. This creates size differences between arenas and connectors.

## Loop completion: 10 points

For a connected doorway graph:

```text
loopCount = doorwayCount - (roomCount - 1)
```

Shooter target:

```text
desiredLoops = arenaCount >= 5 ? 1 : 0
```

Legacy target:

```text
desiredLoops =
    roomCount < 4
        ? 0
        : min(4, max(1, roomCount / 6))
```

When the target is not zero:

```text
loopScore = min(1, loopCount / desiredLoops)
```

When `desiredLoops == 0`, `loopScore` is `1`. This gives all loop points without a loop.

```text
points = 10 × loopScore
```

This neutral score reaches its maximum at one shooter loop. It does not penalize more loops. It does not enforce the production contract. Production validation separately requires exactly two useful cycles and minimum Shortcut savings of two arena transitions.

## Circulation quality: 16 points

Circulation combines path length, side branches, and maximum degree.

### Start-to-exit path

```text
normalizedPath = startExitGraphDistance / max(roomCount - 1, 1)
pathScore = clamp(
    1 - abs(normalizedPath - 0.65) / 0.65,
    0,
    1)
```

This score prefers a Start-to-Exit route that uses about 65% of the maximum simple graph length.

### Dead-end ratio

```text
deadEndRatio = degreeOneRoomCount / roomCount
branchScore = clamp(
    1 - abs(deadEndRatio - 0.25) / 0.35,
    0,
    1)
```

The target is about 25% dead ends. This supports optional side rooms. It does not make each room a leaf.

### Maximum degree

```text
degreeScore =
    1                           when maximumDegree <= 4
    1 / (maximumDegree - 3)     otherwise
```

### Combined circulation score

```text
circulationScore =
    0.5 × pathScore
  + 0.3 × branchScore
  + 0.2 × degreeScore

points = 16 × circulationScore
```

## Entrance completion: 6 points

```text
entranceScore = clamp(connectedEntranceCount / 6, 0, 1)
points = 6 × entranceScore
```

Exact entrance completion is a hard acceptance rule. All candidates in one best-of-N run use the same brief. Thus, this term is constant in that run. It changes absolute scores between runs with different entrance counts. It cannot offset a missing entrance. It cannot select between candidates that use the same brief.

## Failure behavior

The generator rejects a candidate when a required invariant fails. Failures include these conditions:

- Arena growth is below the minimum size.
- A required mission route fails.
- A route for a required cycle edge fails.
- An entrance route fails.
- Planned doorway contact is missing.
- The connected entrance set is wrong.
- A room is disconnected.
- A room count does not match its assignments.
- Circulation is disconnected.
- Roles are invalid.
- Shooter structure is too weak.
- Start-to-Exit distance is too short.
- A production layout does not have exactly two useful cycles.
- A production Shortcut saves fewer than two arena transitions.

Each failure rejects only the current candidate. The top-level generator continues with the next permitted variant. It returns the empty result below only after it exhausts all variants.

If no candidate survives:

```text
seed = requested seed
method = requested method
assignments = EMPTY_CELL for every input cell
rooms = []
doorways = []
connectedEntrances = []
qualityScore = 0
selectedCandidate = 0
```

Callers must check `getRoomCount()` before they create gameplay state.

## Test organization

CMake registers 8 CTest tests. Seven tests use dedicated test executables. The eighth test is `stalberg_level_seed_audit_smoke`. It runs the level seed audit executable for one seed.

Headless game tests cover stationary-target projectile damage. Audio, browser presentation, and full runtime rendering still use graphical smoke tests or manual runs.

| Test | Coverage |
|---|---|
| `stalberg_grid_tests` | Base mesh topology, repeatability, dual geometry, and relaxation |
| `stalberg_room_generation_tests` | Adapter contract, all room methods, physical metadata, circulation, roles, scoring, required cycle edges, Shortcut savings, and edge cases |
| `stalberg_grid_renderer_cache_tests` | One cached dual polygon per grid cell, center-point hit-testing, and rejection of a room layout with an assignment count that does not match the cached grid |
| `stalberg_generated_level_tests` | Retained artifact alignment, exact floor area, authorized walls/doors, doorway clearance, dynamic lock collision/navigation, lifecycle/reset, traversal firing and projectile preservation, deterministic multi-spawn filtering/identity, partial/all-enemies clear transitions, hostile cleanup, whole-match identity reset, Start spawn, and reachability |
| `stalberg_game_tests` | Stationary-target projectile damage, injected encounter walls, swept-circle queries, player wall faces/endpoints/corners/sliding, dash activation/cooldown/wall collision, swept projectile-wall hits, outside-muzzle rejection, profile-separated pool ownership/reuse, muzzle position, fixed fire cadence, deterministic single-enemy and collection movement/patterns, earliest enemy-hit and exact-time identity tie-breaking, simultaneous defeat, enemy closed-wall containment, player damage/invulnerability/death, interpolation freeze, and post-death/post-victory restart |
| `stalberg_horde_match_tests` | Recipe plans and costs, automatic countdown/intermission transitions, puzzle-independent round advancement, deterministic bounded difficulty snapshots and schedules, maximum-round/point overflow safety, geometry-safe spawning, door-aware navigation, economy/gates, concurrent Anchor/Hub/relay progression, explicit extraction, tiered upgrades, repeatable Hub repair, and whole-match reset |
| `stalberg_match_generation_tests` | Same-build and same-toolchain match-seed replay, fixed generation briefs, bounded attempts, deterministic fallback, actor-relative Fortress gates, progression-stage validation, stage-spawn packing, rejection records, and best-valid ranking |
| `stalberg_level_seed_audit_smoke` | One-seed CSV audit startup, generation, and completion |

Run all registered tests:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Grid and dual-geometry tests

`tests/grid_tests.cpp` checks these properties:

- Quad indices and edge-use counts are valid.
- Fixed boundary detection is correct.
- Neighbor relations are symmetric.
- Topology is repeatable.
- Dual-cell areas and clearances are finite and positive.
- Dual-portal widths are finite and positive.
- Each logical grid vertex has one dual cell.
- Each primal grid edge has one dual connection.
- Full relaxation keeps the boundary fixed.

## Adapter and neutral-model tests

`adapterProducesValidRoomInput()` checks these properties:

- Cell count is preserved.
- Buildability mapping is correct.
- Adjacency is canonical.
- Physical cell measurements are positive.
- Physical connection data is aligned.
- Six fixed boundary-side entrance candidates are unique.

`connectionOrderingDoesNotAffectGeneration()` reverses these inputs:

- Physical connection lists.
- Entrance candidate order.

The test then requires identical assignments and an identical quality score.

`roomInputIsIndependentFromLaterRelaxation()` verifies snapshot ownership.

## General layout tests

`roomLayoutIsValid()` checks representative seeds and radii. It verifies these properties:

- Method metadata is correct.
- Assignments align with input cells.
- Room count is in range.
- Three to six entrances are connected.
- Boundary floor follows policy.
- The layout keeps negative space.
- Floor is connected.
- Doorway loop count meets the applicable bound.
- Doorway width and quality are valid.
- Room-pair doorways are unique.
- The circulation graph is connected.
- Room areas and cell counts are exact.
- Each room is connected.
- Cover and spawn data belongs to the correct room.
- There is exactly one start and one exit.
- Candidate quality is finite and positive.

## Shooter-specific tests

`shooterLayoutHasExplicitCombatStructure()` checks these properties:

- The layout has multiple arenas.
- The layout has explicit connector regions.
- The layout has no connector-to-connector doorway.
- Connector degree is one or two.
- Mean arena area is greater than mean connector area.
- Layouts with fewer than 10 substantial rooms have at most one doorway-graph loop.
- Layouts with at least 10 substantial rooms have exactly two doorway-graph loops.
- Start-to-Exit graph distance is at least three.

`largeMapArchetypesPublishDistinctSignatures()` tests required cycle edges. It checks every selected archetype. It requires cycle rank two and exactly two useful cycles. It requires Shortcut savings of at least two arena transitions. It also checks one typed Shortcut edge and one typed Cycle edge.

`largeShooterLayoutUsesDirectArenaLinks()` checks one representative radius 14 layout. The layout must use direct arena links. Connector count must not be greater than arena count. The layout can publish at most one structural two-cell connector. It also requires exactly one Hub and one Reward.

`shooterLayoutsCanCreateDenseAreas()` uses fixed radius 14 seeds. It verifies both density feature forms. A landmark arena must be at least 1.75 times the median arena size. A three-arena cluster must join its substantial members within nine average cell-edge lengths.

Other regressions cover these cases:

- Radius 2 compact layouts.
- Constrained radius 2 seeds that failed in the normal budget before the fallback change.
- The constrained regression validates the final layout.
- It does not require a specific winning candidate index.
- Radius 3 small layouts.
- Radius 14 large layouts.
- Repeatable assignments, doorways, physical data, roles, tactical candidates, score, topology signature, and selected candidate.
- The repeatability test does not compare `missionEdges`.

## Match-generation tests

`stalberg_match_generation_tests` checks the application-level contracts. The tests cover these areas:

- The same match seed gives the same accepted map and metadata.
- `R` does not regenerate accepted geometry.
- Exhausted budgets use the visible deterministic fallback.
- Fortress V1 requires radius 8 and `worldScale = 0.22`.
- Actor-relative geometry and all-open spawn capacity meet profile limits.
- Progression stages keep required gates approachable.
- Required objectives stay reachable.
- Expansion, Anchor, and Exit gates add reachable cells or save at least two transitions.
- Reward does not require route value.
- Each checked post-gate stage has 18 packed spawn slots in at least two rooms.
- Stage packing is a static admission check. It does not test runtime placement.
- Impossible stage-spawn profiles return `stage_spawn_capacity`.
- Rejection records are deterministic.
- Candidate ranking keeps the best valid result within its ranking budget.

## Legacy-method tests

The suite keeps these checks for branching and organic methods:

- Rooms are connected and have multiple cells.
- Entrance subsets follow the required distribution.
- Organic rooms use six-sector dispersion.
- Room silhouettes vary.
- Organic room sizes vary.
- Large branching layouts can use the expanded 64-room budget.
- Degenerate or malformed neutral grids produce empty layouts.

## Sanitizer verification

Use a separate sanitizer build for the two headless generation test executables:

```sh
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'

cmake --build build-asan \
  --target stalberg_grid_tests stalberg_room_generation_tests -j

ASAN_OPTIONS=detect_leaks=1 \
  ctest --test-dir build-asan --output-on-failure \
  -R '^(stalberg_grid_tests|stalberg_room_generation_tests)$'
```

The build command and CTest filter select the same two test targets. This run checks memory safety and undefined behavior in the headless generation paths. Remove `build-asan` after the run if you do not need the local artifact.

## Debugging a low-scoring or empty layout

Use this sequence:

1. Confirm that the source grid completed relaxation before `makeRoomGrid()`.
2. Confirm that all `RoomGrid` physical metrics are finite and positive.
3. Check `getRoomCount()` before you read metadata.
4. Record the requested seed, grid seed, radius, method, selected candidate, and quality score.
5. Compare connected entrances with the requested brief.
6. Check Start-to-Exit graph distance and connector degrees.
7. Check `usefulCycleCount` and `minimumShortcutSavingsTransitions` for a production layout.
8. Record the match seed, derived seeds, world scale, fixed brief, score, rejection counts, and fallback state.
9. Inspect each progression-stage failure code and gate state.
10. Inspect packed spawn slots and spawn-room count for each checked stage.
11. Check actor-relative room and route dimensions.
12. Check the minimum selected doorway width against the average input portal width.
13. Try a different room seed before you change grid topology.
14. Profile fallback candidate indices when compact seeds fail often.
15. Add a deterministic regression to `tests/room_generation_tests.cpp` or `tests/match_generation_tests.cpp` before you change formulas.

The diagnostic HUD shows the room seed, doorway count, quality score, and selected candidate index. Use this data for quick visual diagnosis.

F2 shows the runtime floor, room graph, room roles, thresholds, and selected topology summary. It does not show typed mission edges. It does not show useful-cycle count, Shortcut savings, or rejection records. Use the audit CSV for aggregate cycle rank, useful-cycle count, Shortcut savings, and rejection counts.

Do not select a seed by visual preference alone. Use F2 and the fixed read-only representative browser. Record arena count, connector count, cycle rank, junction count, branch depth, direct-edge ratio, alternating-chain length, Start-to-Exit distance, and locked-threshold count.

The audit CSV does not contain typed mission-edge endpoints or purposes. It contains aggregate cycle rank, useful-cycle count, Shortcut savings, Combat leaf count, multi-entry ratio, score domains, and rejection counts. Add a deterministic regression for a named metric violation before you change construction or score weights.

Future inspection data must include room grammar, bounded leaves, and physical route separation. It must include semantic anchors, initial-lock spawn checks, and visibility checks. It must also include role compatibility, runtime recovery, and endurance results.
