# Level Generator Recommendation Implementation Report

## Purpose

This report records each implementation experiment, its tests, and its measured effect. The work follows `level-generator-evaluation.md`.

## Evaluation labels

- **Positive:** The change improves safety, quality, repeatability, or observability without a larger measured cost.
- **Neutral:** The change adds required infrastructure, but does not directly change accepted level quality.
- **Negative:** The change reduces measured quality or causes an unacceptable cost or regression.
- **Reverted:** The change was negative and is not in the final implementation.

## Baseline

Commit: `4b48c2c` (`Checkpoint level generation planning work`)

Baseline commands:

```text
cmake --build build -j2
ctest --test-dir build --output-on-failure
timeout 5s xvfb-run -a ./build/stalberg_game
```

Baseline results:

- Build: passed.
- Tests: 7 of 7 passed.
- Test time: 22.30 seconds.
- Virtual-display smoke test: the game initialized with Mesa llvmpipe and ran until the five-second timeout.
- Match admission returned only a Boolean result.
- Whole-map retries could change the large-map archetype.
- Match generation accepted the first valid candidate.
- Gate presence was checked, but the locked progression sequence was not checked.

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

Evaluation: **Positive**.

The accepted map set did not change. Rejections are now machine-readable and explain which limits failed. This is required for safe tuning and seed audits. The measured test-time change was small and within normal run variation.

### 2. Fixed generation briefs across retries

Changes:

- Added a versioned `GenerationBrief`.
- Selected the large-map archetype and quest recipe once from the public match seed.
- Passed the fixed brief to every geometry retry.
- Added a stable brief hash to match metadata.
- Kept the known systems fallback independent from the requested production brief.

Tests:

- Added same-seed brief and hash checks.
- Added a direct two-attempt test that changes geometry seeds and keeps map and quest selections fixed.
- Build passed.
- Tests: 7 of 7 passed in 23.12 seconds.

Seed 1–100 audit:

```text
fallback=2
mean attempts=2.02
requested archetypes=16,16,27,19,22
accepted archetypes=16,16,26,18,22
brief mismatches=0
```

The earlier audit had three fallbacks and a mean of 2.03 attempts. Its accepted archetype counts were 25, 15, 25, 16, and 16 because retries could change the selection.

Evaluation: **Positive**.

Retries no longer change level identity or quest identity. The fallback count decreased from three to two in the same 100-seed range. Mean generation work stayed effectively unchanged.

### 3. Progression-stage admission

Changes:

- Added lock-aware simulation for Expansion, Anchor, Exit, and Reward states.
- Added checks for gate approach, objective reachability, newly reachable floor, and route savings.
- Added named stage failures to the structured match report.
- Made the Fortress profile require at least two saved cell transitions when a progression gate adds no floor.
- Added the known seed 71 and seed 89 candidate configurations as regressions.

Tests:

- Both known bad candidates fail at the Anchor stage.
- Public seeds 71 and 89 retry to non-fallback candidates with valid stage reports.
- Build passed without warnings.
- Tests: 7 of 7 passed in 23.37 seconds.

Seed 1–100 audit:

```text
fallback=11
mean attempts=2.84
accepted archetypes=16,7,26,18,22
```

The previous result had two fallbacks and 2.02 mean attempts. Most new failures were Ring and Branches maps. The stage checks exposed an existing gate-binding problem. The checks did not create that problem.

Evaluation: **Neutral**.

The safety result is positive because accepted Fortress maps now pass the complete gate sequence. The production yield result is negative because fallback use increased to 11 percent. The safety gate remains enabled. The next change must improve gate binding instead of weakening the safety check.

### 4. Deterministic semantic gate and Anchor binding

Changes:

- Tested ordered doorway choices for all four gate purposes.
- Selected the first binding that passes the complete stage simulation.
- Used deterministic failure count as the fallback binding rank.
- Tested alternate Combat rooms as Anchor rooms when the first role-based choice failed.
- Kept Reward gates optional and isolated from the Start-to-Exit route.

Tests:

- The two known softlocked candidate configurations now receive valid gate and Anchor bindings.
- Every simulated gate is approachable and every stage objective is reachable for those regressions.
- Build passed without warnings.
- Tests: 7 of 7 passed in 24.13 seconds.

Seed 1–100 audit:

```text
fallback=5
mean attempts=2.60
accepted archetypes=16,13,26,18,22
```

The stage-admission-only result had 11 fallbacks and 2.84 mean attempts. Binding reduced fallback use by more than half. The full test run increased by approximately 0.8 seconds because candidate gate bindings now run stage simulation.

Evaluation: **Positive**.

The change repairs known softlocks instead of only rejecting them. It also improves production yield while all hard stage checks stay active. Five percent fallback use remains higher than the two percent result before stage checks, so further construction work is still useful.

### 5. Best-valid candidate selection

Changes:

- Added separate score domains for circulation, physical margin, progression, and generator quality.
- Kept hard validation separate from ranking.
- Retained the best valid candidate within a deterministic ranking budget.
- Added a configurable quality-margin stop.
- Recorded attempts performed, selected attempt, valid candidate count, and score details.
- Limited default ranking work to one attempt after the first valid candidate.

Tests:

- Rebuilt every evaluated candidate for a fixed seed and confirmed that the selected attempt has the best score.
- Confirmed that invalid candidates do not enter ranking.
- Confirmed same-seed score and selection metadata.
- Build passed without warnings.
- Tests: 7 of 7 passed in 25.59 seconds.

Seed 1–100 comparison:

```text
first valid:  fallback=5  mean attempts=2.60  mean score=44.7821
ranked:       fallback=5  mean attempts=3.34  mean score=45.1403
```

Evaluation: **Positive**.

The mean score increased by 0.8 percent, and fallback use did not change. Mean attempts increased by 28 percent. A wider ranking budget produced only a small additional score gain, so the default budget was reduced to one extra attempt.

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

- Confirmed same-seed rejection records are identical.
- Confirmed exhausted fallback metadata contains a rejection or construction reason.
- Build passed without warnings.
- Tests: 8 of 8 passed in 25.73 seconds.
- The audit smoke test took 0.23 seconds.

Evaluation: **Positive**.

The change does not alter map admission or quality. It makes each retry and fallback measurable. Future constraint changes can now use repository-owned CSV data instead of temporary audit programs.

### 7. Cycle-first production mission graphs

Changes:

- Added two required cycle ears to every production-size large-map graph.
- Required physical realization of both cycle edges.
- Rejected a production candidate when either cycle route failed.
- Selected cycle endpoints with at least three graph transitions of separation.
- Routed required cycle edges before tree edges to preserve cycle space.
- Kept compact systems fixtures on their existing zero-or-one-cycle rules.
- Replaced the test that required no more than one loop.

Tests:

- Every tested production-size archetype has exactly two contracted cycles.
- The full room-generation matrix passed.
- Build passed without warnings.
- Tests: 8 of 8 passed in 30.47 seconds.

Seed 1–100 comparison:

```text
before required cycles: fallback=5  mean attempts=3.34  mean score=45.140
                         mean cycles=0–1  historical multi-entry=49.14%
after required cycles:  fallback=6  mean attempts=2.99  mean score=58.159
                         mean cycles=2.00  mean multi-entry=65.4%
```

Evaluation: **Positive**.

Accepted production maps now meet the two-cycle circulation requirement. Mean multi-entry coverage increased substantially. Fallback use increased by one map, but mean attempts decreased because stronger topology also increased the quality score.

#### Reverted experiment: immediate 70 percent multi-entry hard gate

The experiment added a third cycle when needed and rejected layouts below 70 percent multi-entry coverage. Ring and Branches and Twin Districts could not realize the rule reliably. Several room-generation regressions returned empty layouts. The experiment was **Negative** and was reverted.

The retained two-cycle design reaches 65.4 percent multi-entry coverage on average. A future bounded-leaf archetype redesign is necessary before the 70 percent rule can become a hard gate.

### 8. Typed mission graph edges

Changes:

- Added typed mission nodes and edges.
- Added explicit Primary, Cycle, Shortcut, Quest, and Exterior purposes.
- Added explicit required status and unlock stage.
- Classified the large-map spanning tree independently from planner insertion order.
- Published mission-edge metadata on `RoomLayout`.
- Used explicit required status when route realization decides whether failure rejects the candidate.
- Marked one cycle as the planned unlockable Shortcut.

Tests:

- Every production archetype publishes required typed edges.
- Every production graph publishes one Shortcut edge and one Cycle edge.
- Published mission-edge count matches the contracted graph edge count.
- Build passed without warnings.
- Tests: 8 of 8 passed in 30.76 seconds.

Seed 1–100 audit:

```text
fallback=6  mean attempts=2.99  mean score=58.159
mean cycles=2.00  mean multi-entry=65.4%
```

Evaluation: **Positive**.

The accepted set and measured quality stayed unchanged. Requiredness and purpose are no longer implicit data outside the graph model.

#### Reverted experiment: route all edges in semantic-purpose order

Routing Shortcut and Cycle edges by their new semantic order changed physical construction. Fallback use increased from six to seven maps. The result was **Negative**. The implementation keeps the proven physical routing order, but required failure behavior now uses the typed edge status.

### 9. Useful-cycle and shortcut measurement

Changes:

- Measured each typed Cycle and Shortcut edge with that edge closed.
- Counted a cycle as useful only when its alternate path has at least three room transitions.
- Measured Shortcut transition savings.
- Counted ordinary Combat leaves separately.
- Required two useful cycles and at least two Shortcut transitions of savings.
- Changed candidate circulation ranking to use useful cycles instead of cycle rank.
- Added the new metrics to the seed-audit CSV.

Tests:

- Every production archetype publishes two useful cycles.
- Every production Shortcut saves at least two room transitions.
- Build passed without warnings.
- Tests: 8 of 8 passed.

Seed 1–100 comparison:

```text
before useful-cycle gate: fallback=6  mean attempts=2.99  mean score=58.159
                          mean multi-entry=65.4%
after useful-cycle gate:  fallback=6  mean attempts=3.29  mean score=58.751
                          mean multi-entry=66.7%
```

Evaluation: **Positive**.

Fallback use stayed unchanged. The accepted set now rejects local cycles that save less than two room transitions. Mean attempts increased by 10 percent. The current metric does not yet measure physical centerline separation or doorway direction.

### 10. Spawn packing at progression stages

Changes:

- Measured statically usable spawn cells in each simulated gate state.
- Applied deterministic distance packing so adjacent cells do not count as separate simultaneous slots.
- Counted the rooms that contain packed slots.
- Required 18 packed slots in at least two rooms for each post-gate Fortress state.
- Added a structured stage-spawn failure code.
- Skipped packing work during gate-binding searches that do not request spawn limits.

Tests:

- Accepted Fortress maps meet packed-slot and room-count limits at every simulated stage.
- An impossible spawn profile returns `stage_spawn_capacity` failures.
- Build passed without warnings.
- Tests: 8 of 8 passed in 33.83 seconds.

Seed 1–100 comparison:

```text
before: fallback=6  mean attempts=3.29  mean score=58.751
after:  fallback=6  mean attempts=3.29  mean score=58.751
```

Evaluation: **Positive**.

The new hard contract did not reduce production yield. It proves that each checked stage can place the scheduled maximum living population across more than one room. Initial-lock spawn capacity, visibility bands, and runtime occupancy recovery still need separate work.
