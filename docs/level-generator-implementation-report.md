# Level Generator Recommendation Implementation Report

This report uses ASD-STE100 Simplified Technical English. It uses short sentences. It separates historical experiments from current status.

## Purpose

This report records each implementation experiment. It records the tests and measured effect of each experiment. The work follows `level-generator-evaluation.md`.

This report uses `useful cycle` for the measured graph property. A useful cycle is a non-Primary mission edge. Its alternate path has at least three arena transitions with that edge closed after connectors are contracted. This metric does not prove physical route separation. It does not prove a progression unlock. This report uses `Cycle` and `Shortcut` for typed mission-edge purposes.

## Evaluation labels

- **Positive:** The named result improved. For a map-quality judgment, this label does not classify generation cost.
- **Neutral:** The named result did not change.
- **Negative:** The named result decreased. A generation-cost judgment is also Negative when a measured cost exceeds its stated limit.
- **Reverted:** The change was negative. The final implementation does not contain it.

Acceptance results include safety checks, acceptance yield, and accepted-map quality metrics. Measured costs include attempts and test time. Report map quality and generation cost separately when they change in different directions.

## Baseline

Commit: `4b48c2c` (`Checkpoint level generation planning work`)

The baseline used these commands:

```text
cmake --build build -j2
ctest --test-dir build --output-on-failure
timeout 5s xvfb-run -a ./build/stalberg_game
```

The baseline gave these results:

- Build: passed.
- Tests: 7 of 7 passed.
- Test time: 22.30 seconds.
- Virtual-display smoke test: The game started with Mesa llvmpipe. It ran until the five-second timeout.
- Match admission returned only a Boolean result.
- Whole-map retries could change the large-map archetype.
- Match generation accepted the first valid candidate.
- Admission checked gate presence. It did not check the locked progression sequence.

These results are historical. The final result is in the `Final result` section.

## Change record

### 1. Structured match-admission reports

Changes:

- Added a version number to each physical constraint profile.
- Added stable failure codes and names.
- Added expected and actual values to each failure.
- Added the full metric set to the validation report.
- Kept the old Boolean admission function as a report wrapper.

Tests:

- Added checks for named failures, measured ranges, profile versions, and empty reports for accepted maps.
- Build passed.
- Tests: 7 of 7 passed in 22.63 seconds.

Evaluation: **Neutral**.

The set of accepted maps did not change. Rejection reports now use machine-readable data. Each report identifies the failed limit. This information supports safe tuning and seed audits. Test time increased from 22.30 to 22.63 seconds. The measured increase was 0.33 seconds.

### 2. Fixed generation briefs across retries

Changes:

- Added a versioned `GenerationBrief`.
- Selected the large-map archetype and quest recipe one time from the public match seed.
- Passed the fixed brief to each geometry retry.
- Added a stable brief hash to match metadata.
- Kept the known systems fallback independent from the requested production brief.

Tests:

- Added same-seed brief and hash checks.
- Added a direct two-attempt test. The test changes geometry seeds. It keeps map and quest selections fixed.
- Build passed.
- Tests: 7 of 7 passed in 23.12 seconds.

Seed 1-100 audit:

```text
fallback=2
mean attempts=2.02
requested archetypes=16,16,27,19,22
accepted archetypes=16,16,26,18,22
brief mismatches=0
```

The earlier audit had three fallbacks. It had a mean of 2.03 attempts. Its accepted archetype counts were 25, 15, 25, 16, and 16. Retries could change the selection in that audit.

Evaluation: **Positive**.

Retries no longer change level identity or quest identity. In the same 100-seed range, fallback count decreased from three to two. Mean attempts decreased from 2.03 to 2.02.

### 3. Progression-stage admission

Changes:

- Added lock-aware simulation for Expansion, Anchor, Exit, and Reward states.
- Added checks for gate approach, objective reachability, new reachable floor, and route savings.
- Added named stage failures to the structured match report.
- Made the Fortress profile require at least two saved cell transitions when a progression gate adds no floor.
- Added the known seed 71 and seed 89 candidate configurations as regressions.

Tests:

- Both known failing candidates fail at the Anchor stage.
- Public seeds 71 and 89 retry to non-fallback candidates with valid stage reports.
- Build passed without warnings.
- Tests: 7 of 7 passed in 23.37 seconds.

Seed 1-100 audit:

```text
fallback=11
mean attempts=2.84
accepted archetypes=16,7,26,18,22
```

The previous result had two fallbacks. It had 2.02 mean attempts. Most new failures were Ring and Branches maps. The stage checks found an existing gate-binding problem. They did not cause the problem.

Evaluation: **Negative**.

Accepted Fortress maps passed the complete gate sequence. Fallback use increased from 2 to 11 requests. Acceptance yield decreased from 98 to 89 Fortress maps. This decrease makes the experiment Negative under the objective label rules. The safety gate stayed active. The next change had to improve gate binding. It could not weaken the safety check.

### 4. Deterministic semantic gate and Anchor binding

Changes:

- Tested ordered doorway choices for all four gate purposes.
- Selected the first binding that passed the complete stage simulation.
- Used deterministic failure count to rank fallback bindings.
- Tested alternate Combat rooms as Anchor rooms when the first role-based choice failed.
- Kept Reward gates optional and separate from the Start-to-Exit route.

Tests:

- The two known softlocked candidate configurations now receive valid gate and Anchor bindings.
- Each simulated gate is approachable for those regressions.
- Each stage objective is reachable for those regressions.
- Build passed without warnings.
- Tests: 7 of 7 passed in 24.13 seconds.

Seed 1-100 audit:

```text
fallback=5
mean attempts=2.60
accepted archetypes=16,13,26,18,22
```

The stage-admission-only result had 11 fallbacks. It had 2.84 mean attempts. Binding reduced fallback use from 11 to 5 requests. This was a 54.5 percent reduction. Test time increased from 23.37 to 24.13 seconds. The measured increase was 0.76 seconds. Candidate gate bindings now run stage simulation.

This experiment had no stated test-time limit.

Evaluation: **Positive**.

The change repaired known softlocks. It did not only reject them. It also improved production yield. All hard stage checks stayed active. Fallback use was five percent. The result before stage checks was two percent. Construction still had to recover this difference of three fallback requests.

### 5. Best-valid candidate selection

Changes:

- Added separate score domains for circulation, physical margin, progression, and generator quality.
- Kept hard validation separate from ranking.
- Kept the best valid candidate in a deterministic ranking budget.
- Added a configurable quality-margin stop.
- Recorded attempts performed, selected attempt, valid candidate count, and score details.
- Limited default ranking work to one attempt after the first valid candidate.

Tests:

- Rebuilt each evaluated candidate for a fixed seed. Confirmed that the selected attempt has the best score.
- Confirmed that invalid candidates do not enter ranking.
- Confirmed same-seed score and selection metadata.
- Build passed without warnings.
- Tests: 7 of 7 passed in 25.59 seconds.

Seed 1-100 comparison:

```text
first valid:  fallback=5  mean attempts=2.60  mean score=44.7821
ranked:       fallback=5  mean attempts=3.34  mean score=45.1403
```

Evaluation: **Positive**.

The mean score increased by 0.8 percent. Fallback use did not change. Mean attempts increased by 28 percent. A larger ranking budget increased the score again. The experiment did not record the size of that increase. Therefore, the default budget was reduced to one extra attempt.

The stated ranking-work limit is one attempt after the first valid candidate. The default configuration meets this limit.

### 6. Rejection records and seed-matrix audit tool

Changes:

- Recorded named rejection counts and construction-failure counts in match metadata.
- Added `stalberg_level_seed_audit`.
- Added stable CSV columns for the fixed brief, selection, score domains, topology, and rejection counts.
- Added a CTest smoke test for the audit executable.

Example:

```text
./build/stalberg_level_seed_audit 1 100 > level-audit.csv
```

Tests:

- Confirmed that same-seed rejection records are identical.
- Confirmed that exhausted fallback metadata contains a rejection or construction reason.
- Build passed without warnings.
- Tests: 8 of 8 passed in 25.73 seconds.
- The audit smoke test took 0.23 seconds.

Evaluation: **Neutral**.

The change did not change map admission or quality. It made each retry and fallback measurable. Future constraint changes can use repository-owned CSV data. They do not need temporary audit programs.

### 7. Cycle-first production mission graphs

Changes:

- Added two required cycle ears to each production-size large-map graph.
- Required physical realization of both cycle edges.
- Rejected a production candidate when either cycle route failed.
- Selected cycle endpoints with at least three arena transitions of separation.
- Routed required cycle edges before tree edges to keep cycle space available.
- Kept compact systems fixtures on their existing zero-or-one-cycle rules.
- Replaced the test that required no more than one loop.

Tests:

- Each tested production-size archetype has exactly two contracted cycles.
- The full room-generation matrix passed.
- Build passed without warnings.
- Tests: 8 of 8 passed in 30.47 seconds.

Seed 1-100 comparison:

```text
before required cycles: fallback=5  mean attempts=3.34  mean score=45.140
                         mean cycles=0–1  historical multi-entry=49.14%
after required cycles:  fallback=6  mean attempts=2.99  mean score=58.159
                         mean cycles=2.00  mean multi-entry=65.4%
```

Evaluation: **Negative**.

Accepted production maps met the requirement for two contracted cycles. Mean multi-entry coverage increased from the historical 49.14 percent to 65.4 percent. The increase was 16.26 percentage points. Fallback use increased from five to six requests. Acceptance yield decreased from 95 to 94 Fortress maps. This decrease makes the experiment Negative under the objective label rules. Mean attempts decreased from 3.34 to 2.99.

#### Reverted experiment: immediate 70 percent multi-entry hard gate

This experiment added a third cycle when necessary. It rejected layouts below 70 percent multi-entry coverage. Ring and Branches could not reliably make these layouts. Twin Districts could not reliably make them. Several room-generation regressions returned empty layouts. The experiment was **Negative**. The final implementation excludes it.

The retained two-cycle design reached 65.4 percent mean multi-entry coverage at this point. A future bounded-leaf archetype redesign was necessary before the 70 percent rule could become a hard gate. This 70 percent hard gate is still an active acceptance target.

### 8. Typed mission graph edges

Changes:

- Added typed mission nodes and edges.
- Added Primary, Cycle, Shortcut, Quest, and Exterior enum values.
- Emitted Primary, Cycle, and Shortcut purposes from the current planner.
- Reserved Quest and Exterior for future planner output.
- Added explicit required status and unlock stage.
- Classified the large-map spanning tree independently from planner insertion order.
- Published mission-edge metadata on `RoomLayout`.
- Used explicit required status when route realization decides if failure rejects the candidate.
- Marked one cycle as the planned unlockable Shortcut.

Tests:

- Each production archetype publishes required typed edges.
- Each production graph publishes one Shortcut edge and one Cycle edge.
- Published mission-edge count matches the contracted graph edge count.
- Build passed without warnings.
- Tests: 8 of 8 passed in 30.76 seconds.

Seed 1-100 audit:

```text
fallback=6  mean attempts=2.99  mean score=58.159
mean cycles=2.00  mean multi-entry=65.4%
```

Evaluation: **Neutral**.

The set of accepted maps did not change. Measured quality did not change. Required status and purpose no longer depend on data outside the graph model.

#### Reverted experiment: route all edges in semantic-purpose order

This experiment routed Shortcut and Cycle edges in semantic-purpose order. This order changed physical construction. Fallback use increased from six to seven maps. The result was **Negative**. The final implementation keeps the proven physical routing order. It uses typed edge status to control failure behavior.

### 9. Useful-cycle and Shortcut measurement

Changes:

- Measured each typed Cycle and Shortcut edge with that edge closed.
- Counted a non-Primary mission edge as a useful cycle only when its alternate path has at least three arena transitions after connectors are contracted.
- Measured Shortcut savings in arena transitions.
- Counted ordinary Combat leaves separately.
- Made production-size room validation require exactly two useful cycles and at least two arena transitions of Shortcut savings.
- Changed candidate circulation ranking to use useful cycles instead of cycle rank.
- Added the new metrics to the seed-audit CSV.

Tests:

- Each production archetype publishes two useful cycles.
- Each production Shortcut saves at least two arena transitions.
- Build passed without warnings.
- Tests: 8 of 8 passed.

Seed 1-100 comparison:

```text
before useful-cycle gate: fallback=6  mean attempts=2.99  mean score=58.159
                          mean multi-entry=65.4%
after useful-cycle gate:  fallback=6  mean attempts=3.29  mean score=58.751
                          mean multi-entry=66.7%
```

Evaluation: **Positive**.

Fallback use did not change. Production-size room validation now requires exactly two useful cycles. It rejects a map when the planned Shortcut saves fewer than two arena transitions. Mean attempts increased by 10 percent. The current metric does not measure physical centerline separation. It does not measure doorway direction.

The experiment kept the existing deterministic attempt budget. This report does not state its numeric limit.

Current production status: Production-size room validation applies when `grid.getCellCount() >= 600`. It requires exactly two useful cycles. It requires a planned Shortcut to save at least two arena transitions.

### 10. Spawn packing at progression stages

Changes:

- Measured statically usable spawn cells in each simulated gate state.
- Applied deterministic distance packing. Adjacent cells do not count as separate simultaneous slots.
- Counted the rooms that contain packed slots.
- Required 18 packed slots in at least two rooms for each post-gate Fortress state.
- Added a structured stage-spawn failure code.
- Skipped packing work during gate-binding searches that do not request spawn limits.

Tests:

- Accepted Fortress maps meet packed-slot and room-count limits at each simulated stage.
- An impossible spawn profile returns `stage_spawn_capacity` failures.
- Build passed without warnings.
- Tests: 8 of 8 passed in 33.83 seconds.

Seed 1-100 comparison:

```text
before: fallback=6  mean attempts=3.29  mean score=58.751
after:  fallback=6  mean attempts=3.29  mean score=58.751
```

Evaluation: **Positive**.

The new hard contract did not reduce production yield. The static check found 18 packed slots in at least two rooms for each checked stage. This result does not prove runtime spawn placement. Runtime also checks player distance. Runtime checks active walls. Runtime checks occupancy. Runtime uses a larger separation limit. Initial-lock spawn capacity still needs work. Visibility bands and runtime occupancy recovery also need work.

This experiment had no stated test-time limit.

## Final result

The final verification used these commands:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
timeout 8s xvfb-run -a ./build/stalberg_game
```

It gave these results:

- Build: passed without warnings.
- Tests: 8 of 8 passed in 33.83 seconds.
- Virtual display: raylib, OpenGL, shaders, fonts, framebuffers, and the game loop started correctly.
- The virtual-display run stayed active until the expected eight-second timeout.

The final 100-seed production audit gave these values:

```text
fallback=6
mean attempts=3.29
mean score=58.751
mean contracted cycles=2.00
mean useful cycles=2.00
mean multi-entry substantial rooms=66.7%
progression-safe accepted maps=100%
stage-spawn-safe accepted maps=100%
```

The fallback count covers all 100 requests. The mean attempt count of 3.29 also covers all 100 requests. The remaining metrics cover the 94 accepted Fortress maps. The mean score for those maps is 58.751. Their mean contracted-cycle count is 2.00. Their mean useful-cycle count is 2.00. Their mean multi-entry ratio is 66.7%. All 94 maps passed progression admission. All 94 maps passed static post-gate spawn packing.

The static packing result does not prove runtime spawn placement. Runtime also checks player distance. Runtime checks active walls. Runtime checks occupancy. Runtime uses a larger separation limit.

This table compares the historical evaluation baseline with the final result:

The baseline map-quality values cover 97 accepted Fortress maps. The final map-quality values cover 94 accepted Fortress maps. Fallback use and mean attempts cover all 100 requests in each audit.

| Measure | Evaluation baseline | Final result | Judgment |
|---|---:|---:|---|
| Contracted cycles | 0.32 | 2.00 | Positive |
| Useful cycles | Not measured | 2.00 required | Positive |
| Multi-entry substantial rooms | 49.14% | 66.7% | Positive |
| Known Anchor softlocks | Present | Repaired or rejected | Positive |
| Decorative progression gates | Present | Rejected below value limit | Positive |
| Static post-gate spawn packing | Not measured | 18 slots in 2 rooms | Positive |
| Fallback use | 3% | 6% | Negative |
| Mean attempts | 2.03 | 3.29 | Negative |
| Rejection explanations | Boolean or silent | Named metrics and counts | Positive |

Map-quality judgment: **Positive**.

Generation-cost judgment: **Negative**.

The original report called the overall result Positive. The map-quality judgment preserves that historical conclusion. Mean contracted cycles increased from 0.32 to 2.00. Useful cycles changed from not measured to 2.00 required. Mean multi-entry coverage increased from 49.14% to 66.7%. The implementation repaired or rejected known Anchor softlocks. All 94 accepted Fortress maps passed progression admission. The implementation also rejected progression gates below the value limit. Static post-gate spawn packing changed from not measured to 18 slots in 2 rooms. All 94 accepted Fortress maps passed this static check.

Generation cost increased. Mean attempts increased from 2.03 to 3.29 across 100 requests. Fallback use increased from 3 to 6 requests. Acceptance yield decreased from 97 to 94 Fortress maps.

## Commit record

| Commit | Change |
|---|---|
| `557d4c2` | Structured match-admission reports |
| `c501bb1` | Fixed generation briefs across retries |
| `adeec0d` | Progression-stage admission |
| `892d45d` | Stage-safe gate and Anchor binding |
| `076dcfc` | Best-valid candidate ranking |
| `1fd1e60` | Seed-audit executable and rejection records |
| `0eeca59` | Two required production cycles |
| `96cfdfb` | Typed mission graph edges |
| `6e00da9` | Useful-cycle and Shortcut measurement |
| `327043d` | Stage-specific spawn packing |

## Current status

The current implementation contains these items:

- Production-size room validation requires exactly two useful cycles on production layouts.
- Production-size room validation requires a planned Shortcut to save at least two arena transitions.
- Match admission validates stage progression.
- Gate binding uses stage-safety checks.
- Checked post-gate states have a hard static spawn-packing gate.
- The final audit reports `fallback=6` across 100 requests.
- The final audit reports `attempts=3.29` across 100 requests.
- The final audit reports `score=58.751` across 94 accepted Fortress maps.
- The final audit reports `contracted cycles=2.00` across 94 accepted Fortress maps.
- The final audit reports `useful cycles=2.00` across 94 accepted Fortress maps.
- The final audit reports `multi-entry=66.7%` across 94 accepted Fortress maps.
- The final audit reports `progression safe=100%` across 94 accepted Fortress maps.
- The final audit reports `spawn safe=100%` across 94 accepted Fortress maps. This result means static post-gate packing passed.
- Static post-gate packing does not prove runtime spawn placement.
- Runtime also checks player distance.
- Runtime checks active walls.
- Runtime checks occupancy.
- Runtime uses a larger separation limit.

## Recommendations that remain incomplete

The following work needs construction or runtime changes. The listed limits remain active acceptance targets. No weaker limit replaces them.

1. **70 percent multi-entry hard gate:** The retained result is 66.7 percent. The direct hard-gate experiment was negative. The final implementation excludes it.
2. **Maximum two Combat leaves:** Twin Districts can exceed this limit. The archetype needs bounded branch construction.
3. **Dead-end depth:** Routine dead-end chains must have a maximum depth of two arena transitions. Current validation does not enforce this hard gate.
4. **Physical route separation:** Useful-cycle checks measure graph savings. They do not measure centerline separation or doorway-angle separation.
5. **Joint room and route construction:** Arena growth and route construction are separate. The generator has no full rip-up and reroute system.
6. **Room shape and combat briefs:** Rooms use one main growth grammar. Dedicated lane, perimeter, defended-center, and broken-sightline builders are necessary.
7. **Typed tactical and semantic anchors:** The generator does not publish cover orientation. It does not publish firing lanes. It does not publish holdout footprints. It does not publish presentation attachments.
8. **Full curated quest binding:** Gate and Anchor binding uses constraints. The generator does not compile the complete recipe set to immutable semantic anchors.
9. **Initial-lock, visibility, and role spawn checks:** Static post-gate packing is a hard gate. The initial lock state is not checked. Visibility bands are not checked. Role compatibility is not checked.
10. **Bounded runtime spawn recovery:** A temporary spawn failure can still prevent normal progress. Runtime needs a bounded recovery rule.
11. **Endurance and navigation work:** Reverse-field caching remains incomplete. Equal-cost route distribution remains incomplete. Automated rounds 1, 5, 25, and maximum-pressure runs also remain incomplete.
12. **Portable project-owned random generator:** Generation versions and brief hashes exist. Portable long-term replay still depends on standard-library random behavior.

First, implement room-shape and combat briefs. Then implement bounded branch construction for each archetype. Enable the 70 percent hard gate after both changes pass their tests. Keep all current hard safety and circulation gates active during this work.
