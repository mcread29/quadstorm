# Layout Quality, Validation, Determinism, and Testing

This document explains how room candidates are seeded, validated, scored, selected, retried, and tested.

- Shooter construction stages: [`shooter-level-generation.md`](shooter-level-generation.md)
- Input/output model: [`room-generation-model.md`](room-generation-model.md)
- Base grid algorithm: [`demo-and-algorithm.md`](demo-and-algorithm.md)

## Why generation uses multiple candidates

Procedural room growth and route embedding are constrained optimization problems. A single deterministic random stream can produce:

- An arena seed with too little room to grow.
- A required tree edge with no route through remaining empty cells.
- An entrance that cannot reach floor after corridor placement.
- Narrow doorways or poor coverage despite valid connectivity.
- A short or overly centralized circulation graph.

Instead of forcing malformed geometry through repair steps, the generator creates several deterministic variants, rejects invalid variants, scores valid ones, and publishes the highest-scoring result.

## Stable requested seed versus candidate variant seed

The public seed identifies the requested layout. It is preserved in `RoomLayout::getSeed()` regardless of which candidate wins.

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

This rejects layouts that are topologically valid but do not provide a meaningful shooter route.

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

CTest currently builds three headless test executables: two for generation and one for the separate `stalberg_game` arena/projectile/weapon modules. Target collision and rendering currently rely on graphical smoke runs; target headless tests were explicitly deferred for Milestone 3.

| Test | Coverage |
|---|---|
| `stalberg_grid_tests` | Base mesh topology, repeatability, dual geometry, and relaxation |
| `stalberg_room_generation_tests` | Adapter contract, all room methods, physical metadata, circulation, roles, scoring, and edge cases |
| `stalberg_game_tests` | Player wall faces/endpoints/corners/sliding, swept projectile-wall hits, outside-muzzle rejection, projectile movement/interpolation, pool reuse, muzzle position, and fixed fire cadence |

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
5. Compare connected entrances with the fixed brief.
6. Inspect start-to-exit graph distance and connector degrees.
7. Inspect minimum selected doorway width relative to average input portal width.
8. Try a different room seed before changing grid topology.
9. If compact seeds fail frequently, profile how often fallback candidates above the requested budget are selected.
10. Add a deterministic regression case to `tests/room_generation_tests.cpp` before changing formulas.

The HUD already displays room seed, doorway count, quality score, and selected candidate index for quick visual diagnosis.
