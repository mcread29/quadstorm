# Level Generator Evaluation and Improvement Plan

This document evaluates the current shooter-level generator and its application-level match acceptance against the active goals in [`level-generation-lock-in.md`](level-generation-lock-in.md). It focuses on whether accepted levels are likely to produce good top-down horde combat, coherent multi-stage quests, useful traversal choices, and reproducible iteration.

## Executive verdict

The generator has a strong technical foundation, but it does **not yet generate consistently good production levels**.

It reliably produces connected geometry, exact authorized doorways, reproducible same-build candidates, and valid runtime navigation. The problem is that its construction, validation, and score still optimize the previous target: mostly tree-shaped collections of compact arenas with zero or one loop. The active target requires two or three useful cycles, multi-entry combat rooms, distinct room mechanics, stage-safe progression, semantic quest binding, and meaningful shortcuts.

The right approach is not a complete rewrite. Preserve the grid, dual geometry, immutable `GeneratedLevel`, exact doorway contract, mutable `LevelSession`, deterministic retries, and regression fixtures. Replace and extend the shooter planner, room grammar, constraint model, match admission checks, and quest binding.

## Evaluation performed

The review covered:

- Abstract graph planning and physical routing.
- Arena growth and corridor materialization.
- Doorway planning, topology metrics, validation, and scoring.
- Match-level physical acceptance and deterministic retries.
- Gate planning, objective placement, spawning, and navigation.
- Room-, match-, generated-level-, and horde-test coverage.

Verification performed on the current working tree:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

All seven test executables passed. A five-second graphical smoke run also completed without an early failure.

Two read-only seed audits were run against the current build.

### Radius-8 lower-level audit

The audit generated 500 radius-8 layouts: five grid seeds and one hundred room seeds, giving one hundred requests for each large-map archetype.

| Archetype | Empty results | Mean cycles | Multi-entry arenas | Arena leaves |
|---|---:|---:|---:|---:|
| Hub and Spokes | 0/100 | 0.00 | 55.43% | 44.57% |
| Ring and Branches | 8/100 | 1.00 | 50.00% | 50.00% |
| Main Spine | 0/100 | 0.00 | 57.14% | 42.86% |
| Twin Districts | 0/100 | 0.00 | 14.29% | 85.71% |
| Dense Core/Sparse Branch | 0/100 | 0.95 | 61.71% | 38.29% |

No archetype can reach the active requirement of two or three useful cycles. None reaches the required 70% multi-entry ratio on average. Twin Districts is especially leaf-heavy.

### Public match-seed audit

For match seeds 1–100:

- 97 produced a Fortress V1 map.
- 3 used the radius-5 systems fallback.
- Mean whole-map attempts were 2.03.
- Accepted maps averaged 0.32 contracted cycles.
- Accepted maps averaged 49.14% multi-entry substantial rooms.
- Accepted maps averaged 50.86% substantial-room leaves.
- Only 6/97 reached a 70% multi-entry ratio.
- Seeds 71 and 89 admitted maps where the Anchor progression was unreachable after the Expansion purchase.
- Six Anchor gates and fifteen Exit gates added no newly reachable cells.

Some accepted maps only narrowly clear current physical limits. Across this sample, the minimum accepted doorway width was 3.51 player diameters against a 3.5 limit, and the minimum substantial-room area was 271.09 player-diameter squares against a 270 limit.

The results confirm that current tests prove implementation consistency, not production-level quality or progression safety.

## What should be preserved

### Clean artifact boundaries

The chain from `StalbergGrid` through dual geometry, neutral `RoomGrid`, `RoomLayout`, and immutable `GeneratedLevel` is well separated. The room generator does not depend on rendering, and gameplay retains enough geometry to construct exact floors, walls, and thresholds.

### Exact doorway authorization

Only published doorway cell pairs become openings. Incidental physical room contacts remain walls, and navigation uses the same authorized pairs. This is a strong foundation for gates, shortcuts, quests, and deterministic stage simulation.

### Mutable state separated from generated data

`GeneratedLevel` is immutable while `LevelSession` owns current collision and traversal locks. This is the correct boundary for simulating gate stages without regenerating geometry.

### Deterministic bounded generation

Lower-level candidate variants, fixed entrance briefs, application retries, visible fallback, and same-seed tests provide a useful reproducibility foundation. The mechanism needs richer metadata and project-owned randomness, not removal.

### Existing regression fixtures

The radius-5 recipes and representative browser remain useful systems tests and emergency fallback content. They should not define normal production topology.

## Critical findings

## 1. The current topology cannot satisfy the active circulation goal

`shooterCandidateIsValid()` explicitly requires zero cycles for Hub and Spokes, Main Spine, and Twin Districts; Ring and Branches requires one; Dense Core permits at most one. The room-generation test suite also asserts that shooter layouts contain at most one loop.

This directly conflicts with the production requirement for two or three useful cycles. It also explains the observed leaf ratios and doorway funnels.

Topology labels are therefore descriptive metadata, not evidence of quality.

### Required change

Move from tree-first planning with an optional loop to **cycle-first mission planning**:

1. Construct a Start-to-Exit backbone.
2. Add two or three required “ears” connecting separated backbone or branch nodes.
3. Attach no more than a bounded number of intentional leaves.
4. Assign at least one cycle edge as a progression shortcut.
5. Route every required cycle; reject the candidate if realization fails.

Each archetype should express this differently:

- Hub and Spokes: connect selected spokes into two branch-to-branch routes.
- Ring and Branches: retain the primary ring and add a separated cross-route or secondary ear.
- Main Spine: add asymmetrical cross-links between side branches and distant spine nodes.
- Twin Districts: create internal circulation in each district and a meaningful second inter-district route.
- Dense Core/Sparse Branch: preserve a dense two-cycle core while making the sparse branch short and intentional.

## 2. “Useful cycle” is not currently measured

`TopologySignature` measures cycle rank, but not whether a cycle changes play. A tiny triangle, adjacent parallel doors, or an alternate route running beside the primary route can satisfy cycle rank without producing a tactical choice.

### Required change

Measure each cycle or shortcut using the contracted arena graph and physical routes:

- Route distance with the candidate edge closed versus open.
- Percentage and world-time savings for relevant origin/destination pairs.
- Edge-disjoint or room-disjoint alternate-path length.
- Physical separation between route centerlines away from shared endpoints.
- Distinct doorway direction at the rooms where paths diverge.
- Whether the alternate path provides another combat approach, escape route, or progression unlock.

Only cycles passing configured value and separation thresholds count as useful.

## 3. Every arena uses the same compact growth grammar

`growArenaRoom()` applies the same frontier score to every arena. Landmark and cluster modes change size and seed placement, not local combat mechanics. Growth rewards same-room neighbors, clearance, radial compactness, and noise.

This produces size variation but not reliable long lanes, L-shapes, perimeter routes, defended centers, or dense multi-exit rooms.

### Required change

Publish a `RoomShapeBrief` and `RoomCombatBrief` before growth. Each brief should have a dedicated growth objective and hard postconditions.

Initial briefs:

- Open kiting arena.
- Long-lane or crossfire arena.
- L-shaped or broken-sightline arena.
- Perimeter route around central negative space.
- Defend-the-center arena.
- Dense close-range arena with multiple safe exits.

Measure physical rather than cell-count properties:

- Usable area after actor-radius erosion.
- Elongation and principal axis.
- Concavity and lobe/neck structure.
- Central negative-space area.
- Minimum cross-section.
- Doorway angular spread.
- Entrance reading space.
- Objective footprint.
- Sightline bands and cover exposure.
- Internal kiting-loop length.

## 4. Arena growth and corridor routing are greedily separated

Arenas are grown sequentially, then graph edges are routed one at a time through the remaining cells. Earlier rooms and routes consume space needed by later requirements. There is no seed relocation, route backtracking, or rip-up/reroute.

Ring and Branches even attempts its optional closure before mandatory tree edges so the closure has a chance to fit. This can let an optional route block required connectivity.

### Required change

Co-plan geometry and circulation:

1. Place arena seeds.
2. Assign shape and doorway-sector briefs.
3. Generate several candidate route envelopes per mission edge.
4. Reserve required corridor space before full arena growth.
5. Grow arenas jointly or in rounds rather than to completion sequentially.
6. Select routes jointly, with deterministic rip-up/reroute for conflicts.
7. Materialize rooms, corridors, and exact doorway contacts.

Routing results should retain intended endpoint contacts. Current doorway generation later scans the entire shared boundary and can move a doorway away from the contact that justified the route.

## 5. Edge semantics are inferred from vector order

The abstract mission graph is stored as `vector<pair<int,int>>`. The first `arenaCount - 1` edges are treated as required and later edges as optional. Ring routing adds a special ordering exception.

This is too fragile for evolving constraints, shortcuts, quest gates, or route-separation requirements.

### Required change

Use a typed mission brief:

```text
MissionNodeBrief
    semantic capabilities
    allowed room briefs
    degree and approach requirements

MissionEdgeBrief
    endpoints
    required or optional
    purpose: primary / cycle / shortcut / quest / exterior
    connector policy
    width and route-separation requirements
    unlock stage

MissionGraphBrief
    nodes
    edges
    topology constraints
```

Requiredness and purpose must never depend on insertion order.

## 6. The scalar score rewards old and representation-dependent behavior

The current 100-point score does not use most published topology metrics. It targets one loop, 25% raw-room dead ends, path length including connector segmentation, and area variance that mixes arenas with tiny connectors.

Consequences:

- Tree archetypes receive an unavoidable loop penalty despite being declared valid.
- Tiny connectors can improve area variation.
- A connector-realized edge contributes two doorway samples while a direct edge contributes one.
- Exterior spurs affect raw-room dead-end and route terms.
- Entrance completion is constant across candidates in one run.
- No score rewards useful cycles, room-brief compliance, doorway spread, route separation, objective safety, or stage capacity.

### Required change

Apply hard constraints first, then score separate domains:

1. Contracted circulation quality.
2. Physical route and doorway realization.
3. Room-brief compliance and mechanical diversity.
4. Quest binding and shortcut value.
5. Stage capacity and pacing.
6. Negative-space and landmark distribution.

Score each abstract edge once. Exclude exterior spurs from mission topology. Score substantial-room diversity separately from connector dimensions. Keep the score breakdown in diagnostics instead of publishing only one number.

At application level, do not immediately accept the first candidate that barely clears hard limits. Within a fixed deterministic budget, retain the best valid candidate or stop only when a configured quality margin is reached.

## 7. Constraints are scattered and return only booleans

Thresholds and rules are distributed among generator constants, validation switches, score formulas, application profiles, gate planning, and tests. Candidate failures provide no structured reason. Match generation silently swallows construction exceptions.

This makes the requested generate–playtest–adjust-constraint loop difficult to operate.

### Required change

Introduce a versioned constraint profile and structured reports:

```text
GenerationBrief
    generation version
    fixed topology and quest selections
    shape distribution requirements
    constraint profile ID

ValidationReport
    pass/fail
    measured metrics
    failure codes
    expected range and actual value
    failing room, edge, anchor, or gate stage

CandidateScoreBreakdown
    topology
    geometry
    room mechanics
    quest
    endurance
    diversity
```

Separate:

- **Contract invariants:** malformed data is always rejected.
- **Hard gameplay constraints:** a candidate cannot enter play.
- **Soft quality objectives:** rank otherwise valid candidates.
- **Cross-seed diversity constraints:** evaluated over a seed matrix, not one map.

Expose rejection histograms and near-miss metrics in F2/F3.

## 8. Application retries do not preserve the topology brief

Whole-map retries derive a new room seed, and the large-map archetype is derived from that room seed. A difficult archetype can therefore be replaced by another archetype on the next attempt. The seed audit showed an accepted distribution of 25, 15, 25, 16, and 16 across the five archetypes rather than a balanced fixed selection.

This will be especially harmful when quests are added: retries must not silently switch to an easier recipe.

### Required change

Derive the topology archetype, curated quest recipe, recipe version, symbol family, and major variant choices once from the public match seed. Keep that `GenerationBrief` fixed across all geometry attempts. Attempts may vary grid, placement, growth, and routing seeds only.

Persist the accepted brief and a binding hash in replay metadata.

## 9. Accepted maps can contain progression softlocks or decorative gates

`buildSmallMapPlan()` selects paths independently and places Expansion, Anchor, Reward, and Exit gates without simulating their combined locked state. Match acceptance checks that all gate purposes exist, not that the player can approach and complete them in order.

The seed audit found accepted Anchor softlocks and gates that opened no additional floor.

### Required change

Before accepting a map, simulate at least these states:

1. Initial locks.
2. Expansion open.
3. Anchor route open.
4. Hub powered and Exit route open.
5. Optional Reward open.
6. Quest shortcut open.

At every state require:

- The next gate is approachable from its intended side.
- Required devices and clues are reachable.
- The opening or current component supports the scheduled enemy population.
- Opening a progression gate adds reachable space or meaningful route savings.
- No later gate is required to reach the current objective.
- Extraction remains voluntary and reachable after completion.

Add seeds 71 and 89 as deterministic regressions.

## 10. Current objective placement is not semantic binding

Large-map planning chooses the first Combat room as Anchor, highest-clearance cells as Hub/Anchor/Exit sites, and sorted Reward-room cover candidates as relay targets. This does not validate route order, visibility, holdout footprint, firing lanes, device separation, or clue relationships.

### Required change

Publish semantic anchors from generated room geometry, then bind the fixed curated quest recipe with deterministic constraint solving. Mandatory binding happens before ordinary loot placement. Failed bindings reject the candidate.

The runtime should consume an immutable bound quest plan rather than reconstructing semantics from roles and room order.

## 11. Spawn metrics measure the all-open map, not actual progression states

Current acceptance counts globally reachable, all-open spawn candidates and uses the maximum Euclidean separation of one cross-room pair. Runtime spawning filters the currently unlocked component, player distance, walls, and occupancy.

A high global count does not prove that the opening area or a quest holdout supports six to eighteen living enemies.

### Required change

Use one shared spawn-usability contract in validation and runtime. For each gate stage measure:

- Mutually compatible spawn-slot packing.
- Number of separated ingress regions and rooms.
- Path reachability to representative player/objective cells.
- Distance and visibility bands.
- Local crowd capacity around doors and objectives.
- Capacity for the stage's scheduled maximum living population.

Add a bounded runtime recovery rule so a temporarily impossible spawn cannot prevent a round from ever reaching cleanup.

## 12. Tactical metadata is too shallow

A cover candidate is currently a boundary cell away from an internal doorway. A spawn candidate is two cell hops from a doorway with positive clearance. Neither captures orientation, wall extent, actor footprint, line of sight, role suitability, or current route state.

### Required change

Publish typed anchors with measured capabilities:

- Cover position, normal, protected arc, and standing footprint.
- Spawn slot, role compatibility, ingress cluster, and visibility bands.
- Holdout footprint and approach regions.
- Doorway-facing device anchor.
- Shootable target and firing-lane anchors.
- Shortcut controls.
- Passageway loot and secret alcoves.
- Presentation attachments for symbols, cables, lights, and landmarks.

## Recommended implementation order

## Phase 0 — Make admission safe and observable

1. Add structured validation reports and centralized, versioned profiles.
2. Keep topology and quest selections fixed across retries.
3. Simulate gate states and reject progression softlocks.
4. Record candidate rejection reasons and score breakdowns.
5. Add the current bad seeds as regressions.
6. Add a command-line seed-matrix audit target that emits CSV or JSON.

**Exit criterion:** Every accepted map has a valid progression sequence, and every rejection can be explained.

## Phase 1 — Replace tree-first circulation

1. Add typed mission nodes and edges.
2. Redesign all large archetypes around two or three required useful cycles.
3. Measure useful-cycle value, leaves, dead-end depth, approach separation, and shortcut savings.
4. Add multi-route planning with deterministic rerouting.
5. Promote the active circulation limits to hard gates.
6. Replace the old test asserting at most one loop.

**Exit criterion:** Every accepted Fortress map has two or three useful cycles, at least 70% multi-entry substantial rooms, no more than two ordinary Combat leaves, shallow routine dead ends, and one valuable unlockable shortcut.

## Phase 2 — Add room grammar and physical portals

1. Publish shape/combat briefs.
2. Reserve doorway sectors and route envelopes before arena growth.
3. Use physical area and actor-relative dimensions instead of cell counts where possible.
4. Implement dedicated growth strategies per brief.
5. Support connector kinds such as mission passage, exterior spur, and deliberate junction.
6. Validate room mechanics, doorway spread, entrance safety, sightlines, and objective footprints.
7. Publish semantic and loot anchors.

**Exit criterion:** Substantial rooms produce mechanically different combat and advertise verified anchor capabilities.

## Phase 3 — Bind curated quest recipes and passageway rewards

1. Select one curated recipe and variants once per public match seed.
2. Compile recipe nodes into semantic and route requirements.
3. Bind all mandatory and optional stages to generated anchors.
4. Reject impossible or poorly ordered bindings.
5. Place quest gates and shortcuts on exact typed mission edges.
6. Replace hard-coded Round 2/5 checks with recipe requirements.
7. Place ordinary passageway weapons, upgrades, supplies, and secrets after mandatory binding.
8. Persist recipe, binding, rewards, and recovery rules.

**Exit criterion:** The same seed reproduces a complete, stage-solvable multi-stage puzzle without fixed room IDs or coordinates.

## Phase 4 — Validate endurance and runtime navigation

1. Validate spawn packing, ingress, LOS, objective safety, and economy at every stage.
2. Add maximum as well as minimum traversal-time limits.
3. Cache reverse navigation fields by target cell and lock revision.
4. Distribute enemies across equal-cost routes and ingress lanes.
5. Make ranged positioning depend on navigation and line of sight.
6. Headlessly exercise rounds 1, 5, 25, and maximum pressure on accepted seed matrices.

**Exit criterion:** Scheduled populations always enter, navigate, and clear without reducing play to a single doorway or hanging the round.

## Phase 5 — Establish the feel gate

1. Maintain a fixed regression matrix plus fresh seeds.
2. Record route choices, funnel tactics, shortcut savings, backtracking, landmark memory, and room-mechanic diversity.
3. Preserve every important bad seed as a deterministic regression.
4. Adjust hard constraints, soft weights, and construction together.
5. Add project-owned RNG and a generation-version contract before promising portable long-term seed replay.

**Exit criterion:** Consecutive accepted matches are consistently distinguishable in overview and play, and manual reviews no longer describe ordinary output as compact arenas connected by passages.

## Initial constraint changes

These should be configurable profile values rather than scattered literals.

| Constraint | Current behavior | Production direction |
|---|---|---|
| Useful contracted cycles | 0–1 | Require 2–3 |
| Multi-entry substantial rooms | Measured, mostly unused | Require at least 70% |
| Ordinary Combat leaves | Not separated from other leaves | Maximum 2 |
| Routine dead-end depth | Not enforced | Maximum 2 transitions |
| Shortcut value | Not measured | Require configured distance/time savings |
| Alternate-route separation | Not measured | Require distinct doorway direction and physical route separation |
| Room shapes | One compact grammar | Require a varied brief distribution |
| Objective footprint | Highest-clearance cell proxy | Validate complete footprint and approaches |
| Stage reachability | All-open aggregate checks | Validate every gate and quest state |
| Spawn capacity | Global candidate count | Validate compatible packing per stage |
| Application selection | First valid attempt | Best valid candidate within a fixed budget or quality-margin stop |
| Rejection reporting | Boolean/silent exception | Structured failure codes and metrics |
| Archetype/quest retries | Can change with attempt seed | Fixed once per public match seed |

Exact physical thresholds for route separation, shortcut time, sightline bands, and crowd packing should be baselined from the current seed matrix and then tuned through playtests. The constraint itself should exist before its final number is chosen.

## Source-level implementation map

| Area | Primary files |
|---|---|
| Typed mission brief and graph planning | `src/rooms/room_generation_shooter_planning.inc`, `src/rooms/room_generator.hpp` |
| Shape briefs, connector kinds, anchor metadata | `src/rooms/room_layout.hpp` plus new focused headers |
| Joint arena/route realization | `src/rooms/room_generation_shooter_layout.inc`, `room_generation_shooter_planning.inc` |
| Structured validation | `src/rooms/room_generation_validation.hpp`, `src/game/match_map_metrics.*` |
| Score decomposition | `src/rooms/room_generation_scoring.hpp`, application-level match quality module |
| Fixed match brief and retries | `src/game/match_generator.*`, `src/game/generated_level.*` |
| Quest binding and gates | Replace the role/ID assumptions in `src/game/horde_match_plan.cpp` |
| Stage simulation | `src/game/match_map_metrics.*`, `src/game/level_session.*` |
| Runtime spawn/navigation contract | `src/game/horde_match.*`, `src/game/level_session.*` |
| Diagnostics | `src/game/game_renderer_overview.cpp`, F3 HUD, seed-audit executable |
| Regression and distribution tests | `tests/room_generation_tests.cpp`, `tests/match_generation_tests.cpp`, `tests/horde_match_tests.cpp` |

## Final recommendation

Do not add more archetype names or adjust the existing scalar weights first. That would improve labels without fixing the construction model.

Start with progression safety and diagnostics, then rebuild circulation around required useful cycles. Once graph construction can reliably meet those constraints, add room-shape briefs and semantic anchors. Only then bind the curated quest recipes and passageway loot.

The generator should improve through a permanent loop:

```text
generate seed matrix
    → inspect rejection and quality metrics
    → play accepted maps
    → capture bad seeds
    → change construction and constraints
    → rerun regressions and fresh seeds
```

That process, rather than any single scoring formula, is what can make the generator consistently produce good levels.
