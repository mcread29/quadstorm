# Layout Quality, Validation, Determinism, and Testing

This document explains how room candidates are seeded, validated, scored, selected, retried, and tested. The room generator is deterministic for a requested seed. The target game adds an application-level match generator that chooses a fresh replayable match seed, derives layout inputs plus a physical-scale and quest profile, and accepts only a complete gameplay-valid result.

- Shooter construction stages: [`shooter-level-generation.md`](shooter-level-generation.md)
- Input/output model: [`room-generation-model.md`](room-generation-model.md)
- Base grid algorithm: [`demo-and-algorithm.md`](demo-and-algorithm.md)
- Planned identity/diversity expansion: [`level-identity-pass.md`](level-identity-pass.md)

## Why generation uses multiple candidates

Procedural room growth and route embedding are constrained optimization problems. A single deterministic random stream can produce:

- An arena seed with too little room to grow.
- A required tree edge with no route through remaining empty cells.
- An entrance that cannot reach floor after corridor placement.
- Narrow doorways or poor coverage despite valid connectivity.
- A short or overly centralized circulation graph.

Instead of forcing malformed geometry through repair steps, the generator creates several deterministic variants, rejects invalid variants, scores valid ones, and publishes the highest-scoring result.

## Stable requested seed versus candidate variant seed

The public room seed identifies the requested layout and is preserved in `RoomLayout::getSeed()` regardless of which candidate wins. In normal gameplay it is deterministically derived from a freshly chosen match seed; replay/tests supply the match seed explicitly, while the room seed remains available for lower-level diagnostics.

### Grid fingerprint

The candidate stream depends on a fingerprint of the neutral input. The fingerprint includes:

- Cell count.
- Exact position, area, and clearance bits.
- Buildability.
- Canonically sorted connection destinations, distances, and widths.
- Canonically sorted entrance candidates.

Changing the grid topology, relaxation result, physical metrics, or entrance policy changes candidate generation.

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

The PRNG seed used inside one candidate is:

```text
generationSeed = low32(mix(gridFingerprint xor variantSeed))
```

`mix()` is SplitMix64-style hashing. `unitNoise(seed,salt)` maps the low 16 mixed bits to `[0,1]` by dividing by `65535`.

Generation is repeatable for the same implementation, standard-library behavior, input bits, method, and seed. C++ standard random distributions do not promise identical mappings across every standard-library implementation, so byte-for-byte cross-toolchain reproducibility should be verified before relying on generated layouts as serialized network protocol data.

## Fixed entrance brief

The entrance set is generated once before candidate variants.

1. Entrance candidates are sorted canonically.
2. Count is selected between `min(3,n)` and `min(6,n)`.
3. Count weights are `C(n,k)`.
4. Candidates are shuffled.
5. The first selected count becomes the fixed brief.

Every candidate must connect exactly this set. Best-of-N scoring cannot choose an easier subset.

Tests verify that:

- All 42 concrete three-to-six subsets appear across 600 representative seeds.
- Three-, four-, five-, and six-side counts are strictly descending, consistent with the expected combination weighting. The test does not apply a statistical tolerance to the exact `20:15:6:1` ratio.
- Candidate-count changes do not change the entrance brief.

## Fixed shooter density mode

On maps large enough to support it, the requested room seed also fixes whether the shooter layout is dispersed, contains one landmark arena, or attempts a two-to-three-arena cluster. This choice does not use the candidate variant seed, so best-of-N scoring cannot compare a requested dense layout against ordinary dispersed variants. Candidate variants may change which non-start/exit arena is featured and the exact cluster placement.

Compact maps always use dispersed placement. A clustered candidate that cannot find a safe local satellite position falls back to ordinary farthest-point placement, preserving mission validity rather than forcing density into unsupported geometry.

## Candidate budget and constrained-grid retries

Public options default to:

```cpp
RoomGenerationOptions {
    .method = RoomGenerationMethod::ShooterLayout,
    .candidateCount = 6
};
```

The requested count is clamped to `[1,32]`.

Normal behavior:

1. Generate every candidate in the requested budget.
2. Skip invalid candidates.
3. Score every valid candidate.
4. Keep the greatest score.
5. Resolve exact score ties in favor of the earlier candidate because replacement uses strict `>`.

Shooter fallback behavior:

- If the complete requested budget contains no valid shooter candidate, continue deterministically through candidate index `31`.
- If any requested-budget candidate was valid, do not pay for the additional retries.
- Legacy methods do not extend their requested budget.

This fallback is intended for compact or unusually constrained grids. `getSelectedCandidate()` may therefore exceed the configured default count for a shooter layout that required fallback retries.

If all permitted candidates fail, the generator returns an empty assignment-aligned layout. It does not substitute a legacy method.

## Neutral input validation

Before candidate generation, `topologyIsValid()` requires:

- Aligned cell, neighbor, and connection vector sizes.
- Finite positions.
- Finite positive areas.
- Finite nonnegative clearances.
- Matching duplicate-free neighbor and physical-connection destination sets.
- No self-connections.
- Finite positive distances and shared-boundary lengths.
- Symmetric reverse connections.
- Reverse metric agreement within `max(value,1) × 0.0001`.
- Unique, in-range entrance candidates.

Invalid input makes each candidate constructor return immediately with empty assignments. The top-level deterministic loop still executes its configured attempts, and shooter mode can still extend to its fallback limit before returning the final empty layout.

## General candidate validation

`candidateIsValid()` applies to all three room-generation methods.

### Assignment invariants

- Assignment count equals input cell count.
- Values are `EMPTY_CELL` or valid room IDs.
- Room count is between 2 and 64.
- Room IDs are dense and equal vector positions.
- Every room contains at least two cells.
- Measured assignment count equals `GeneratedRoom::cellCount`.
- Every room's assigned cells form one connected component.

### Doorway invariants

- At least `roomCount - 1` doorways exist.
- Region IDs are valid.
- Endpoint indices are valid.
- Endpoint assignments match doorway region IDs.
- Doorway endpoint cells are graph neighbors.

### Circulation and role invariants

- Exactly one room has role `Start`.
- Exactly one room has role `Exit`.
- The doorway graph reaches every room.

A geometrically connected floor is not sufficient; accepted circulation must also be connected through published logical doorways.

## Shooter-specific validation

`shooterCandidateIsValid()` adds:

- At least three non-connector rooms.
- At least one connector room.
- No connector-to-connector doorway.
- Every connector has doorway degree one or two.
- Start and exit are present.
- Shortest start-to-exit doorway-graph distance is at least three edges.
- Radius-5 recipe layouts have exactly five substantial rooms with stable Start `0`, Exit `1`, Hub `2`, Anchor `3`, and Reward `4` semantics.
- Every recipe materializes its exact required contracted arena edges; only Broken Ring and Twin Wings may add their declared optional edge.
- Hub Circuit has no optional Start → Anchor edge, so Start and Anchor cannot become two adjacent entrances to the same destination room.
- Reward remains a leaf reached without making its locked threshold part of the required Start-to-Exit route.
- Start and Anchor branch doorway directions from the Hub have a normalized dot product no greater than `0.42`, or roughly 65 degrees of angular separation.

This rejects layouts that are topologically valid but do not provide a meaningful shooter route, including the playtest failure where two progression branches collapsed into neighboring doors leading to effectively the same place.

## In-progress random-match, physical-scale, quest, and diversity expansion

The game wraps neutral generation in a bounded eight-attempt application-level pipeline: one public match seed derives candidate grid/room seeds and the Fortress V1 profile, same-seed runs reproduce the accepted layout/profile/retry count, `R` preserves the result, `N` requests another seed, and exhaustion visibly selects a deterministic systems fixture. Fortress V1 uses radius 8 and `worldScale = 0.22`; larger shooter layouts now publish one high-degree Hub and prefer a leaf Combat arena for Reward semantics.

Application acceptance requires the current semantic rooms, three relay targets, all four gate purposes, a Hub with at least three published doorway edges, and actor-relative minimums for doorway width, substantial/Anchor area, objective clearance, Anchor room span, Start-to-Exit route distance, statically usable cross-room ingress separation, usable spawn candidates, and spawn-bearing rooms. Static spawn filtering requires all-open reachability, enemy-sized source clearance, and distance from immutable walls; dynamic player separation, occupied slots, and the currently locked component remain runtime concerns. A radius-8 map left at `0.16` fails explicitly. This proves a first physically larger, all-open capacity boundary, not full endurance or quest validity: opening-component circulation/economy, connector-specific measurements, true line-of-sight bands, broader topology profiles, semantic anchors, and authored quest binding remain.

Implemented graph measurements include:

- Published large-map topology archetype and archetype-specific degree/cycle constraints.
- Contracted and direct arena-edge counts plus direct arena-to-arena edge ratio.
- Longest alternating arena/connector chain.
- Connector and substantial-room counts.
- Multi-door substantial-room and meaningful-junction counts.
- Degree histogram, maximum degree, cycle rank, branch depth, and Start-to-Exit distance.

Graph-community separation and score penalties based on these measurements remain pending.

Planned room-geometry measurements include compactness, elongation, concavity, lobe/neck structure, doorway count, doorway angular spread, and local clearance. Implemented gameplay-scale measurements cover doorway width, room footprint, objective clearance, Anchor room-center span, route travel, statically usable cross-room ingress separation, and all-open spawn capacity in world/player-relative units. The room span is a size proxy, not a line-of-sight proof. Connector-specific dimensions, true sightline bands, traversal-time checks, and opening-component capacity remain. Quest validation must check semantic-role availability, required route ordering/separation, objective capacity, and optional-branch viability without fixed coordinates.

Accepted cross-seed sets should contain multiple topology, room-shape, and quest-placement signatures without introducing nondeterminism: repeated identical match inputs must still publish identical layouts, metadata, scores, and bindings.

The physical checks are application-level acceptance gates and do not alter the neutral 100-point candidate score below. Archetype-specific graph validity is now a neutral hard gate, while the published alternation/direct-link metrics do not yet alter the score. Shape/diversity scoring still needs an explicit rebaseline.

## Candidate quality score

Every valid candidate is scored on a 100-point scale.

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

The target is 48% physical-area coverage. Scores reach zero at or beyond 32% and 64% coverage.

The numerator includes every assigned cell, including selected fixed boundary entrances. The denominator includes only buildable input cells, so coverage can be slightly influenced by entrance floor.

## Mean doorway width: 20 points

Average portal width is calculated from undirected buildable-to-buildable input connections.

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

Widths above `1.5 × averagePortalWidth` receive no additional reward.

## Minimum doorway width: 16 points

```text
minimumWidthScore = min over doorways(min(ratio, 1))
points = 16 × minimumWidthScore
```

This component prevents a high mean from hiding one severe bottleneck.

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

The score favors a coefficient of variation near `0.55`, encouraging meaningful size contrast between arenas and connectors.

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

When the target is nonzero:

```text
loopScore = min(1, loopCount / desiredLoops)
```

When `desiredLoops == 0`, `loopScore` is explicitly `1`, awarding the full loop component without requiring a loop.

```text
points = 10 × loopScore
```

Shooter construction bounds loops to at most one. Shooter validation does not independently check loop count, and the score itself does not penalize excess loops.

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

This favors a start-to-exit route using roughly 65% of the maximum possible simple graph length.

### Dead-end ratio

```text
deadEndRatio = degreeOneRoomCount / roomCount
branchScore = clamp(
    1 - abs(deadEndRatio - 0.25) / 0.35,
    0,
    1)
```

The target is about 25% dead ends, supporting optional side rooms without turning every room into a leaf.

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

Exact entrance completion is a hard acceptance rule and every candidate in one best-of-N run shares the same brief. This term is therefore constant among candidates in that run. It shifts absolute scores between runs with different selected entrance counts; it cannot compensate for a missing entrance or decide between candidates sharing one brief.

## Failure behavior

A candidate is rejected when any required invariant fails, including:

- Arena growth below minimum size.
- Required tree-route failure.
- Entrance-route failure.
- Missing planned doorway contact.
- Wrong connected entrance set.
- Disconnected or count-mismatched rooms.
- Disconnected circulation.
- Invalid roles.
- Shooter structure or route too weak.

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

Callers should check `getRoomCount()` before constructing gameplay state.

## Test organization

CTest currently builds six headless test executables: two for generation, one for renderer-cache geometry, one for generated-level runtime packaging/geometry/navigation/collection encounters, one for `stalberg_game` dash/collision/combat modules, and one for the persistent horde match. Stationary-target collision, audio, browser presentation, and full runtime rendering currently rely on graphical smoke or manual runs; target headless tests were explicitly deferred for Milestone 3.

| Test | Coverage |
|---|---|
| `stalberg_grid_tests` | Base mesh topology, repeatability, dual geometry, and relaxation |
| `stalberg_room_generation_tests` | Adapter contract, all room methods, physical metadata, circulation, roles, scoring, and edge cases |
| `stalberg_grid_renderer_cache_tests` | Cached dual-grid and room-overlay geometry, bounds, colors, and draw-command alignment |
| `stalberg_generated_level_tests` | Retained artifact alignment, exact floor area, authorized walls/doors, doorway clearance, dynamic lock collision/navigation, lifecycle/reset, traversal firing and projectile preservation, deterministic multi-spawn filtering/identity, partial/all-enemies clear transitions, hostile cleanup, whole-match identity reset, Start spawn, and reachability |
| `stalberg_game_tests` | Injected encounter walls, swept-circle queries, player wall faces/endpoints/corners/sliding, dash activation/cooldown/wall collision, swept projectile-wall hits, outside-muzzle rejection, profile-separated pool ownership/reuse, muzzle position, fixed fire cadence, deterministic single-enemy and collection movement/patterns, earliest enemy-hit and exact-time identity tie-breaking, simultaneous defeat, enemy closed-wall containment, player damage/invulnerability/death, interpolation freeze, and post-death/post-victory restart |
| `stalberg_horde_match_tests` | Recipe plans and costs, automatic countdown/intermission transitions, puzzle-independent round advancement, deterministic bounded difficulty snapshots and schedules, maximum-round/point overflow safety, geometry-safe spawning, door-aware navigation, economy/gates, concurrent Anchor/Hub/relay progression, explicit extraction, tiered upgrades, repeatable Hub repair, and whole-match reset |

Run:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Grid and dual-geometry tests

`tests/grid_tests.cpp` checks:

- Valid quad indices and edge use counts.
- Fixed boundary detection.
- Symmetric neighbor relationships.
- Repeatable topology.
- Positive finite dual-cell areas and clearances.
- Positive finite dual-portal widths.
- One dual cell per logical grid vertex.
- One dual connection per primal grid edge.
- Boundary preservation during full relaxation.

## Adapter and neutral-model tests

`adapterProducesValidRoomInput()` checks:

- Cell-count preservation.
- Buildability mapping.
- Canonical adjacency.
- Positive physical cell measurements.
- Aligned physical connection metadata.
- Six unique fixed boundary-side entrance candidates.

`connectionOrderingDoesNotAffectGeneration()` reverses:

- Topological neighbor lists.
- Physical connection lists.
- Entrance candidate order.

It then verifies identical assignments and quality score.

`roomInputIsIndependentFromLaterRelaxation()` verifies snapshot ownership.

## General layout tests

`roomLayoutIsValid()` verifies, across representative seeds and radii:

- Method metadata.
- Assignment alignment.
- Room-count bounds.
- Three-to-six connected entrances.
- Boundary-floor policy.
- Intentional negative space.
- Connected floor.
- Bounded doorway loop count.
- Valid physical doorway width and quality.
- Unique room-pair doorways.
- Connected circulation graph.
- Exact room areas and cell counts.
- Connected individual rooms.
- Candidate ownership for cover and spawn metadata.
- Exactly one start and exit.
- Finite positive candidate quality.

## Shooter-specific tests

`shooterLayoutHasExplicitCombatStructure()` checks:

- Multiple arenas.
- Explicit connector regions.
- No connector-to-connector doorway.
- Connector degree of one or two.
- Mean arena area greater than mean connector area.
- At most one deliberate loop.
- Start-to-exit graph distance of at least three.

`largeShooterLayoutUsesDirectArenaLinks()` checks that a representative radius 14 layout uses direct arena links, keeps connector count to at most half its arena count, and publishes at most one structural two-cell connector.

`shooterLayoutsCanCreateDenseAreas()` uses fixed radius 14 seeds to verify both density feature forms: a landmark arena at least 1.75 times the median arena size, and a three-arena cluster whose substantial members are joined locally within nine average cell-edge lengths.

Additional regressions cover:

- Radius 2 compact layouts.
- Constrained radius 2 seeds that previously failed within the normal budget. The regression validates the final layout but does not explicitly assert which candidate index won or that fallback retries were required.
- Radius 3 small layouts.
- Radius 14 large layouts.
- Repeatability of shooter assignments, doorways, physical metadata, roles, tactical candidates, score, and selected candidate.

## Legacy-method tests

The suite preserves behavior checks for branching and organic methods:

- Connected multi-cell rooms.
- Entrance subset distribution.
- Organic six-sector dispersion.
- Room silhouette variation.
- Organic room-size variation.
- Expanded 64-room budget for large branching layouts.
- Empty behavior for degenerate or malformed neutral grids.

## Sanitizer verification

For additional local verification, configure a separate sanitizer build:

```sh
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'

cmake --build build-asan \
  --target stalberg_grid_tests stalberg_room_generation_tests -j

ASAN_OPTIONS=detect_leaks=1 \
  ctest --test-dir build-asan --output-on-failure
```

This checks memory safety and undefined behavior in the headless generation paths. Remove the separate build directory afterward if it should not remain as a local artifact.

## Debugging a poor or empty layout

Use this order:

1. Confirm the source grid was fully relaxed before `makeRoomGrid()`.
2. Verify `RoomGrid` physical metrics are finite and positive.
3. Check `getRoomCount()` before consuming metadata.
4. Record requested seed, grid seed, radius, method, selected candidate, and quality score.
5. Compare connected entrances with the requested brief.
6. Inspect start-to-exit graph distance and connector degrees.
7. For game candidates, record the match seed, derived seeds, world scale, selected recipe, semantic anchors, and whether fallback was used.
8. Verify actor-relative room/route dimensions and quest requirements before admitting the map to normal play.
9. Inspect minimum selected doorway width relative to average input portal width.
10. Try a different room seed before changing grid topology.
11. If compact seeds fail frequently, profile how often fallback candidates above the requested budget are selected.
12. Add a deterministic regression case to `tests/room_generation_tests.cpp` before changing formulas.

The diagnostic HUD already displays room seed, doorway count, quality score, and selected candidate index for quick visual diagnosis.

For a valid but repetitive layout, do not merely cycle seeds until one looks better. Use F2 and the fixed read-only representative browser to inspect the complete silhouette, room graph, thresholds, baseline topology, and generator metadata. As later identity slices publish archetypes, anti-alternation metrics, room-shape histograms, district/landmark anchors, and cross-seed similarity reports, expose those in the same overview and capture poor signatures as deterministic diversity regressions before changing construction or score weights.
