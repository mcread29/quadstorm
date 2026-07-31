# Level Generator Evaluation and Improvement Plan

This document uses ASD-STE100 Simplified Technical English. It separates historical evaluation data from current implementation status.

This document evaluates the shooter-level generator. It also evaluates match admission against [`level-generation-lock-in.md`](level-generation-lock-in.md). The evaluation checks the top-down horde-combat metrics. It checks multi-stage quest requirements. It checks route-choice metrics and reproducibility.

This document uses `useful cycle` for the measured graph property. A useful cycle is a non-Primary mission edge. Its alternate path has at least three arena transitions with that edge closed after connectors are contracted. This metric does not prove physical route separation. It does not prove a progression unlock. This document uses `Cycle` and `Shortcut` for typed mission-edge purposes.

## Executive verdict

The generator has a strong technical foundation. It does **not yet meet all production acceptance targets**.

At the evaluation baseline, the generator made connected geometry and exact authorized doorways. It made reproducible candidates in the same build. It also made valid runtime navigation. However, construction, validation, and scoring used the old target. That target gave mostly tree-shaped groups of compact arenas with zero or one loop.

The current implementation has moved beyond that baseline. Production-size room validation applies when `grid.getCellCount() >= 600`. This validation now requires exactly two useful cycles. It also requires the planned Shortcut to save at least two arena transitions. The current code validates stage progression. It also validates static post-gate spawn packing.

The current audit sent 100 map requests. Six requests used the fallback. The mean attempt count was 3.29 across all 100 requests. The other audit metrics cover the 94 accepted Fortress maps. Their mean score was 58.751. Their mean contracted-cycle count was 2.00. Their mean useful-cycle count was 2.00. Their mean multi-entry ratio was 66.7%. All 94 maps passed progression admission. All 94 maps passed static post-gate spawn packing.

The static packing result does not prove runtime spawn placement. Runtime also checks player distance. Runtime checks active walls. Runtime checks occupancy. Runtime uses a larger separation limit.

The active target still includes the incomplete work:

- A 70% multi-entry hard gate.
- A maximum of two Combat leaves.
- Dead-end-depth limits.
- Physical route separation.
- Joint routing.
- Room and combat grammar.
- Typed semantic anchors.
- The full quest compiler.
- Initial-lock spawn checks.
- Visibility spawn checks.
- Role-compatible spawn checks.
- Bounded runtime recovery.
- Endurance and navigation work.
- Portable project-owned RNG.

Do not rewrite the complete generator. Keep the grid and dual geometry. Keep the immutable `GeneratedLevel`. Keep the exact doorway contract. Keep the mutable `LevelSession`. Keep deterministic retries and regression fixtures. Replace or extend the shooter planner. Replace or extend the room grammar. Replace or extend the constraint model, match admission checks, and quest binding.

## Evaluation performed

The historical review covered:

- Abstract graph planning and physical routing.
- Arena growth and corridor materialization.
- Doorway planning, topology metrics, validation, and scoring.
- Match-level physical admission and deterministic retries.
- Gate planning, objective placement, spawning, and navigation.
- Room, match, generated-level, and horde tests.

The historical review used these commands on its working tree:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

All seven historical test executables passed. A five-second graphical smoke run also ended without an early failure.

The review ran two read-only seed audits on that build.

### Radius-8 lower-level audit

This historical audit generated 500 radius-8 layouts. It used five grid seeds and one hundred room seeds. It made one hundred requests for each large-map archetype.

| Archetype | Empty results | Mean cycles | Multi-entry arenas | Arena leaves |
|---|---:|---:|---:|---:|
| Hub and Spokes | 0/100 | 0.00 | 55.43% | 44.57% |
| Ring and Branches | 8/100 | 1.00 | 50.00% | 50.00% |
| Main Spine | 0/100 | 0.00 | 57.14% | 42.86% |
| Twin Districts | 0/100 | 0.00 | 14.29% | 85.71% |
| Dense Core/Sparse Branch | 0/100 | 0.95 | 61.71% | 38.29% |

At that time, no archetype could reach the proposed target of two or three useful cycles. No archetype reached a 70% mean multi-entry ratio. Twin Districts had especially many leaves.

### Public match-seed audit

The historical audit gave these results for match seeds 1-100:

- 97 seeds produced a Fortress V1 map.
- 3 seeds used the radius-5 systems fallback.
- Mean whole-map attempts were 2.03.
- Accepted maps averaged 0.32 contracted cycles.
- Accepted maps averaged 49.14% multi-entry substantial rooms.
- Accepted maps averaged 50.86% substantial-room leaves.
- Only 6/97 maps reached a 70% multi-entry ratio.
- Seeds 71 and 89 admitted maps with an unreachable Anchor after the Expansion purchase.
- Six Anchor gates and fifteen Exit gates added no newly reachable cells.

The narrowest historical doorway exceeded its limit by 0.01 player diameters. Its accepted width was 3.51 player diameters. The limit was 3.5. The smallest historical substantial room exceeded its area limit by 1.09 player-diameter squares. Its accepted area was 271.09 player-diameter squares. The limit was 270.

These historical results showed that the tests proved implementation consistency. They did not prove production quality or progression safety.

## What to preserve

### Clean artifact boundaries

The chain starts at `StalbergGrid`. It continues through dual geometry, neutral `RoomGrid`, `RoomLayout`, and immutable `GeneratedLevel`. These parts have clear boundaries. The room generator does not depend on rendering. Gameplay keeps enough geometry to make exact floors, walls, and thresholds.

### Exact doorway authorization

Only published doorway cell pairs make openings. Unplanned physical contacts stay as walls. Navigation uses the same authorized pairs. This design supports gates, Shortcuts, quests, and deterministic stage simulation.

### Separate mutable state

`GeneratedLevel` is immutable. `LevelSession` owns current collision locks and traversal locks. Keep this boundary for gate-stage simulation. Do not regenerate geometry for a stage change.

### Deterministic bounded generation

The baseline had lower-level candidate variants. It had fixed entrance briefs and application retries. It had a visible fallback and same-seed tests. These parts gave a useful reproducibility base.

The current implementation fixes the generation brief across retries. It records the brief hash. Portable long-term replay still needs project-owned randomness.

### Existing regression fixtures

Keep the radius-5 recipes and representative browser. They are useful systems tests and emergency fallback content. Do not use them as the normal production topology.

## Critical findings

## 1. The baseline topology could not meet the circulation goal

At the evaluation baseline, `shooterCandidateIsValid()` required zero cycles for Hub and Spokes, Main Spine, and Twin Districts. Ring and Branches required one cycle. Dense Core permitted no more than one cycle. The room-generation tests also required shooter layouts to have no more than one loop.

These rules conflicted with the proposed production target of two or three useful cycles. They also caused high leaf ratios and doorway funnels.

At that time, topology labels described data. They did not prove quality.

### Required change

Use cycle-first mission planning instead of tree-first planning with an optional loop:

1. Construct a Start-to-Exit backbone.
2. Add required ears between separate backbone or branch nodes.
3. Add only a limited number of intentional leaves.
4. Make at least one cycle edge a progression Shortcut.
5. Route each required cycle. Reject the candidate if a required route fails.

Give each archetype a different implementation:

- Hub and Spokes: Connect selected spokes with two branch-to-branch routes.
- Ring and Branches: Keep the primary ring. Add a separate cross-route or secondary ear.
- Main Spine: Add asymmetrical cross-links between side branches and distant spine nodes.
- Twin Districts: Add circulation inside each district. Add a useful second route between districts.
- Dense Core/Sparse Branch: Keep a dense two-cycle core. Keep the sparse branch short and intentional.

Current status: Production graphs now have exactly two required useful cycles. Required route failure rejects a candidate. The graph planner does not bound leaves.

## 2. The baseline did not measure a useful cycle

At the evaluation baseline, `TopologySignature` measured cycle rank. It did not measure a cycle's effect on play. A tiny triangle could pass. Adjacent parallel doors could pass. A route beside the primary route could also pass. These forms did not always create a tactical choice.

### Required change

Define a useful cycle on the connector-contracted arena graph. Close the measured non-Primary mission edge. Require its alternate path to have at least three arena transitions.

Measure these separate physical and gameplay targets:

- Route distance with the candidate edge closed and open.
- Percentage savings and world-time savings for applicable origin and destination pairs.
- Edge-disjoint or arena-disjoint alternate-path length.
- Physical separation between route centerlines away from shared endpoints.
- Different doorway directions at rooms where paths separate.
- Recorded combat approaches, escape routes, or progression unlocks.

Do not infer these physical and gameplay results from the useful-cycle count. Validate them with separate limits.

Current status: Production-size room validation requires exactly two useful cycles. It requires the planned Shortcut to save at least two arena transitions. The validator does not measure physical centerline separation. It does not measure doorway direction.

## 3. Every arena still uses one compact growth grammar

`growArenaRoom()` applies one frontier score to each arena. Landmark and cluster modes change size and seed placement. They do not change local combat mechanics. Growth rewards same-room neighbors, clearance, radial compactness, and noise.

This process changes room size. It does not reliably make long lanes, L-shapes, perimeter routes, defended centers, or dense multi-exit rooms.

### Required change

Publish a `RoomShapeBrief` and `RoomCombatBrief` before growth. Give each brief its own growth objective and hard postconditions.

Use these initial briefs:

- Open kiting arena.
- Long-lane or crossfire arena.
- L-shaped or broken-sightline arena.
- Perimeter route around central negative space.
- Defend-the-center arena.
- Dense close-range arena with multiple safe exits.

Measure physical properties instead of only cell counts:

- Usable area after actor-radius erosion.
- Elongation and principal axis.
- Concavity and lobe or neck structure.
- Central negative-space area.
- Minimum cross-section.
- Doorway angular spread.
- Entrance reading space.
- Objective footprint.
- Sightline bands and cover exposure.
- Internal kiting-loop length.

Current status: The generator does not implement room-shape grammar. It does not implement combat grammar.

## 4. Arena growth and corridor routing are still separate

The generator grows arenas in sequence. It then routes graph edges one at a time through unused cells. Early rooms and routes use space that later requirements can need. The generator does not relocate seeds. It does not backtrack routes. It does not use rip-up and reroute.

At the baseline, Ring and Branches tried its optional closure before mandatory tree edges. This order gave the closure space. It could also let an optional route block required connectivity.

### Required change

Plan geometry and circulation together:

1. Place arena seeds.
2. Assign shape briefs and doorway-sector briefs.
3. Make several candidate route envelopes for each mission edge.
4. Reserve required corridor space before full arena growth.
5. Grow arenas together or in rounds. Do not complete each arena in sequence.
6. Select routes together. Use deterministic rip-up and reroute for conflicts.
7. Materialize rooms, corridors, and exact doorway contacts.

Keep the planned endpoint contacts in routing results. Current doorway generation scans the full shared boundary later. This scan can move a doorway away from the contact that justified the route.

Current status: The generator does not construct rooms and routes together. Typed required edges now control route-failure rejection.

## 5. The baseline inferred edge semantics from vector order

The baseline abstract mission graph used `vector<pair<int,int>>`. It treated the first `arenaCount - 1` edges as required. It treated later edges as optional. Ring routing added a special order rule.

This data model was too fragile for Shortcut edges. It was also too fragile for quest gates, route separation, and new constraints.

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

Do not derive required status or purpose from insertion order.

Current status: The graph planner uses typed mission nodes. It also uses typed mission edges. The current planner emits Primary, Cycle, and Shortcut purposes. Quest and Exterior are reserved enum values. The current planner does not emit them. The graph planner does not implement the complete semantic capability model.

## 6. The baseline scalar score rewarded old behavior

The baseline 100-point score did not use most topology metrics. It targeted one loop and 25% raw-room dead ends. Its path length included connector segmentation. Its area variance mixed arenas with small connectors.

This design had these effects:

- Tree archetypes received an unavoidable loop penalty even when validation accepted them.
- Small connectors could improve area variation.
- A connector edge added two doorway samples. A direct edge added one.
- Exterior spurs changed raw-room dead-end and route terms.
- Entrance completion was constant for all candidates in one run.
- The score did not reward useful cycles, room-brief compliance, doorway spread, route separation, objective safety, or stage capacity.

### Required change

Apply hard constraints first. Then score separate domains:

1. Contracted circulation quality.
2. Physical route and doorway realization.
3. Room-brief compliance and mechanical diversity.
4. Quest binding and Shortcut value.
5. Stage capacity and pacing.
6. Negative-space and landmark distribution.

Score each abstract edge one time. Exclude exterior spurs from mission topology. Score substantial-room diversity separately from connector dimensions. Show the score parts in diagnostics. Do not show only one number.

Do not accept the first candidate when another candidate in the fixed budget has a higher score. Keep the best valid candidate in that budget. Stop early only when a configured quality margin is met.

Current status: Match generation ranks valid candidates in separate score domains. It uses a deterministic budget and a quality-margin stop. Room mechanics, full quest value, and endurance scoring are incomplete.

## 7. Baseline constraints were scattered and Boolean

The baseline put thresholds and rules in generator constants, validation switches, score formulas, application profiles, gate planning, and tests. Candidate failures had no structured reason. Match generation ignored construction exceptions without a report.

This design made the generate, playtest, and constraint-adjustment loop difficult.

### Required change

Use a versioned constraint profile and structured reports:

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

Keep these rule types separate:

- **Contract invariants:** Always reject malformed data.
- **Hard gameplay constraints:** Do not let a failed candidate enter play.
- **Soft quality objectives:** Rank candidates that pass hard constraints.
- **Cross-seed diversity constraints:** Evaluate these on a seed matrix, not one map.

Show rejection histograms and near-miss metrics in F2/F3.

Current status: Match admission uses versioned profiles. It uses named failure codes. It records measured values. It records rejection reasons. It uses separate score domains. Room reports remain incomplete. Quest reports remain incomplete. Endurance reports remain incomplete.

## 8. Baseline retries did not keep the topology brief

At the baseline, each whole-map retry made a new room seed. The large-map archetype came from that room seed. Thus, a retry could replace a difficult archetype with a different archetype. The historical audit accepted 25, 15, 25, 16, and 16 maps for the five archetypes. This distribution was not balanced or fixed.

This behavior would also let retries change to an easier quest recipe.

### Required change

Derive the topology archetype, curated quest recipe, recipe version, symbol family, and major variants one time from the public match seed. Keep this `GenerationBrief` fixed for all geometry attempts. Only grid, placement, growth, and routing seeds can change between attempts.

Store the accepted brief and binding hash in replay metadata.

Current status: A versioned `GenerationBrief` fixes the archetype and quest recipe across retries. Match metadata stores a stable brief hash. The full quest binding hash is not complete because the full quest compiler is not implemented.

## 9. Baseline maps could contain progression softlocks or decorative gates

At the baseline, `buildSmallMapPlan()` selected paths independently. It placed Expansion, Anchor, Reward, and Exit gates without simulation of the combined locked state. Match admission checked that each gate purpose existed. It did not check the required progression order.

The historical seed audit found Anchor softlocks. It also found gates that added no floor.

### Required change

Simulate at least these states before map admission:

1. Initial locks.
2. Expansion open.
3. Anchor route open.
4. Hub powered and Exit route open.
5. Optional Reward open.
6. Quest Shortcut open.

Require these conditions in each state:

- The player can approach the next gate from its intended side.
- The player can reach required devices and clues.
- The opening or current component supports the planned enemy population.
- A progression gate adds reachable space or useful route savings.
- A later gate is not necessary for the current objective.
- Extraction stays voluntary and reachable after completion.

Keep seeds 71 and 89 as deterministic regressions.

Current status: Match admission validates stage progression. Gate and Anchor binding use deterministic stage simulation. A progression gate that adds no floor must save at least two cell transitions. The known seed configurations now receive valid bindings or fail admission.

## 10. Current objective placement is not full semantic binding

The original large-map planner selected the first Combat room as the Anchor. It selected high-clearance cells for Hub, Anchor, and Exit sites. It selected sorted Reward-room cover candidates as relay targets. These rules did not validate route order, visibility, holdout area, firing lanes, device separation, or clue relationships.

Current gate and Anchor binding tests alternatives against stage safety. This work is not a full quest compiler.

### Required change

Publish semantic anchors from generated room geometry. Bind the fixed curated quest recipe with deterministic constraint solving. Bind mandatory content before normal loot. Reject a candidate if mandatory binding fails.

Give runtime an immutable bound quest plan. Do not reconstruct semantics from roles and room order.

Current status: The generator does not implement the full quest compiler. It does not publish typed semantic anchors.

## 11. Baseline spawn metrics used only the all-open map

Baseline admission counted globally reachable spawn candidates on the all-open map. It used the maximum Euclidean separation of one cross-room pair.

Current runtime spawning checks the unlocked component. It checks player distance. It checks active walls. It checks occupancy. It also uses a larger separation limit than static packing.

A high global count did not prove that the opening area or a quest holdout could support six to eighteen living enemies.

### Required change

Use one spawn-usability contract in validation and runtime. Measure these items for each gate stage:

- Packing of mutually compatible spawn slots.
- Number of separate ingress regions and rooms.
- Path reachability to representative player or objective cells.
- Distance and visibility bands.
- Local crowd capacity near doors and objectives.
- Capacity for the planned maximum living population in that stage.

Add a bounded runtime recovery rule. A temporary spawn failure must not stop a round from reaching cleanup.

Current status: Checked post-gate Fortress states require 18 statically packed slots in at least two rooms. All 94 accepted Fortress maps passed this static check in the final audit. The check does not prove runtime spawn placement. Match admission does not check initial-lock spawn capacity. It does not check visibility bands. It does not check role compatibility. Runtime does not have bounded spawn recovery.

## 12. Tactical metadata is still too shallow

A cover candidate is a boundary cell away from an internal doorway. A spawn candidate is two cell hops from a doorway and has positive clearance. These candidates do not describe orientation, wall extent, actor footprint, line of sight, role suitability, or current route state.

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

Current status: The generator does not publish typed tactical anchors. It does not publish typed semantic anchors.

## Recommended implementation order

## Phase 0 - Make admission safe and observable

1. Add structured validation reports and centralized versioned profiles.
2. Keep topology and quest selections fixed across retries.
3. Simulate gate states. Reject progression softlocks.
4. Record candidate rejection reasons and score parts.
5. Add seeds with recorded acceptance failures as regressions.
6. Add a command-line seed-matrix audit target that writes CSV or JSON.

**Exit criterion:** Each accepted map has a valid progression sequence. Each rejection has an explanation.

Current status: The current code implements this phase for the present gate sequence and reports.

## Phase 1 - Replace tree-first circulation

1. Add typed mission nodes and edges.
2. Design all large archetypes around exactly two required useful cycles.
3. Measure useful-cycle value, leaves, dead-end depth, approach separation, and Shortcut savings.
4. Add multi-route planning with deterministic rerouting.
5. Make the active circulation limits hard gates.
6. Remove the old test that required no more than one loop.

**Exit criterion:** Each accepted Fortress map has exactly two useful cycles. At least 70% of its substantial rooms have multiple entries. It has no more than two ordinary Combat leaves. Routine dead ends are shallow. Its unlockable Shortcut meets the configured savings target.

Current status: Production-size room validation requires exactly two useful cycles. It requires minimum Shortcut savings of two arena transitions. The graph planner uses typed edges. The 70% multi-entry gate is incomplete. The maximum of two Combat leaves is incomplete. Dead-end-depth validation is incomplete. Physical approach separation is incomplete. Joint rerouting is incomplete.

## Phase 2 - Add room grammar and physical portals

1. Publish shape and combat briefs.
2. Reserve doorway sectors and route envelopes before arena growth.
3. Use physical area and actor-relative dimensions where possible. Do not use only cell counts.
4. Add a growth strategy for each brief.
5. Support connector types such as mission passage, exterior spur, and deliberate junction.
6. Validate room mechanics, doorway spread, entrance safety, sightlines, and objective footprints.
7. Publish semantic and loot anchors.

**Exit criterion:** Substantial rooms produce different combat mechanics. They publish verified anchor capabilities.

Current status: The generator does not implement this phase.

## Phase 3 - Bind curated quest recipes and passageway rewards

1. Select one curated recipe and its variants one time for each public match seed.
2. Compile recipe nodes into semantic and route requirements.
3. Bind all mandatory and optional stages to generated anchors.
4. Reject bindings that fail or have a poor order.
5. Put quest gates and Shortcuts on exact typed mission edges.
6. Replace hard-coded Round 2/5 checks with recipe requirements.
7. Place normal passageway weapons, upgrades, supplies, and secrets after mandatory binding.
8. Store the recipe, binding, rewards, and recovery rules.

**Exit criterion:** The same seed reproduces a complete and stage-solvable multi-stage puzzle. The puzzle does not use fixed room IDs or coordinates.

Current status: Recipe identity stays fixed across retries. The current code finds a stage-safe gate and Anchor binding. Typed semantic anchors are incomplete. The full quest compiler is incomplete.

## Phase 4 - Validate endurance and runtime navigation

1. Validate spawn packing, ingress, line of sight, objective safety, and economy at each stage.
2. Add maximum and minimum traversal-time limits.
3. Cache reverse navigation fields by target cell and lock revision.
4. Distribute enemies on equal-cost routes and ingress lanes.
5. Make ranged positioning use navigation and line of sight.
6. Run rounds 1, 5, 25, and maximum pressure without graphics on accepted seed matrices.

**Exit criterion:** Planned populations always enter, navigate, and clear. Play does not reduce to one doorway. A round does not hang.

Current status: Match admission validates static post-gate spawn packing. This check does not prove runtime spawn placement. Initial-lock checks are incomplete. Visibility checks are incomplete. Role checks are incomplete. Endurance work is incomplete. Runtime recovery is incomplete. Navigation work is incomplete.

## Phase 5 - Establish the playtest acceptance gate

1. Keep a fixed regression matrix and use fresh seeds.
2. Record route choices, funnel tactics, Shortcut savings, backtracking, landmark memory, and room-mechanic diversity.
3. Keep each seed with a recorded acceptance failure as a deterministic regression.
4. Adjust construction, hard constraints, and soft weights together.
5. Add project-owned RNG and a generation-version contract before a promise of portable long-term seed replay.

**Exit criterion:** Consecutive accepted matches differ in recorded route choices or room-mechanic results. Manual review records contain no repeated finding of compact arenas connected by passages.

Current status: The repository contains the seed-audit tool. The generator uses generation versions. The full playtest acceptance gate is incomplete. Portable RNG is incomplete.

## Initial constraint changes

These values must be in configurable profiles. Do not scatter them as literals.

This table records the historical evaluation baseline. It also records the proposed production direction at that time.

| Constraint | Evaluation baseline | Proposed production direction |
|---|---|---|
| Useful contracted cycles | 0–1 | Require 2–3 |
| Multi-entry substantial rooms | Measured, mostly unused | Require at least 70% |
| Ordinary Combat leaves | Not separated from other leaves | Maximum 2 |
| Routine dead-end depth | Not enforced | Maximum 2 arena transitions |
| Shortcut value | Not measured | Require configured distance/time savings |
| Alternate-route separation | Not measured | Require distinct doorway direction and physical route separation |
| Room shapes | One compact grammar | Require a varied brief distribution |
| Objective footprint | Highest-clearance cell proxy | Validate complete footprint and approaches |
| Stage reachability | All-open aggregate checks | Validate every gate and quest state |
| Spawn capacity | Global candidate count | Validate compatible packing per stage |
| Application selection | First valid attempt | Best valid candidate within a fixed budget or quality-margin stop |
| Rejection reporting | Boolean/silent exception | Structured failure codes and metrics |
| Archetype/quest retries | Can change with attempt seed | Fixed once per public match seed |

The current implementation changed these baseline items:

- Useful cycles: Production-size room validation requires exactly two.
- Shortcut value: Production-size room validation requires savings of at least two arena transitions.
- Stage reachability: Match admission validates stage progression.
- Spawn capacity: Checked post-gate stages use hard static packed-slot limits.
- Application selection: The generator ranks valid candidates in a fixed budget.
- Rejection reporting: Reports use named codes and metrics.
- Archetype and quest retries: The public match seed fixes the generation brief.

The current implementation has not completed the other table targets. Keep them as active acceptance targets.

First, measure the seed matrix. Then choose the initial numeric limit for each constraint. Next, implement and test one constraint at a time. Finally, tune each limit with playtests.

## Source-level implementation map

| Area | Primary files |
|---|---|
| Typed mission brief and graph planning | `src/rooms/room_generation_shooter_planning.inc`, `src/rooms/room_generator.hpp` |
| Shape briefs, connector kinds, anchor metadata | `src/rooms/room_layout.hpp` plus new focused headers |
| Joint arena/route realization | `src/rooms/room_generation_shooter_layout.inc`, `src/rooms/room_generation_shooter_planning.inc` |
| Structured validation | `src/rooms/room_generation_validation.hpp`, `src/game/match_map_metrics.*` |
| Score decomposition | `src/rooms/room_generation_scoring.hpp`, application-level match quality module |
| Fixed match brief and retries | `src/game/match_generator.*`, `src/game/generated_level.*` |
| Quest binding and gates | Replace the role/ID assumptions in `src/game/horde_match_plan.cpp` |
| Stage simulation | `src/game/match_map_metrics.*`, `src/game/level_session.*` |
| Runtime spawn/navigation contract | `src/game/horde_match.*`, `src/game/level_session.*` |
| Diagnostics | `src/game/game_renderer_overview.cpp`, F3 HUD, seed-audit executable |
| Regression and distribution tests | `tests/room_generation_tests.cpp`, `tests/match_generation_tests.cpp`, `tests/horde_match_tests.cpp` |

## Current audit

The final 100-seed production audit reports:

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

Production-size room validation requires exactly two useful cycles. It requires the planned Shortcut to save at least two arena transitions. Match admission validates stage progression. It also validates static post-gate spawn packing.

These active items are incomplete:

- The 70% multi-entry hard gate.
- A maximum of two Combat leaves.
- Dead-end-depth validation.
- Physical route separation.
- Joint room and route construction.
- Room-shape and combat grammar.
- Typed semantic anchors.
- The full quest compiler.
- Initial-lock, visibility, and role-compatible spawn checks.
- Bounded runtime spawn recovery.
- Endurance and navigation work.
- Portable project-owned RNG.

## Final recommendation

Do not add more archetype names first. Do not adjust only the scalar weights. These changes improve labels or ranking without repair of construction.

Keep progression safety and diagnostics active. Keep circulation based on exactly two required useful cycles. Next, add bounded leaves, dead-end limits, and route separation. Then add room-shape briefs and semantic anchors. After that, bind curated quest recipes and passageway loot.

Use this permanent improvement loop:

```text
generate seed matrix
    → inspect rejection and quality metrics
    → play accepted maps
    → capture seeds with acceptance failures
    → change construction and constraints
    → rerun regressions and fresh seeds
```

No single score formula can make the generator meet all production acceptance targets. Use this repeated process to improve measured results and playtest results.
