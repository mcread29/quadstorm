# Level Generation Lock-In

This document uses ASD-STE100 Simplified Technical English. It uses short sentences. It uses direct instructions.

This document is the only active product roadmap. Pause all work after the current endless-horde foundation. Resume that work only when generated maps meet the active quality metrics. The representative seed matrix must also pass the playtest acceptance checks. See [`level-generator-evaluation.md`](level-generator-evaluation.md) for the implementation audit. It contains seed-matrix evidence and the source-level improvement sequence.

The recent topology work added useful instruments. It did not meet the circulation and room-diversity targets. A named archetype does not give a map a distinct play pattern when the result is a tree of compact, single-entry arenas. Graph labels help diagnosis. Irregular silhouettes help diagnosis. Role colors help diagnosis. These items are not acceptance criteria.

## Product goal

Each new match must produce one memorable fortress. The fortress must support top-down horde combat. It must support one coherent main quest. It must also include at least one optional discovery path. The map stays fixed for the run. The displayed match seed must reproduce the map.

A good map must provide:

- Route choices during combat. A choice of which dead end to buy first is not sufficient.
- Multiple approaches, escapes, and Shortcuts. Gates and quest devices must change these routes.
- Rooms with different movement, sightline, cover, ingress, and objective patterns.
- Landmarks and districts that help the player learn the map.
- Quest dependencies that use the generated space.
- Reasons to revisit an area. Objectives must not always cause hub backtracking.

## Work freeze

Do not add bosses, mutation systems, enemy roles, upgrades, controller work, meta-progression, currencies, or unrelated presentation polish before this lock-in ends.

Use the existing combat, endless rounds, economy, gates, Anchor/Hub/relay/Exit interactions, fixed fixtures, and regression arena as the test harness. Change these systems only when a test of generated-map circulation, quests, or pacing needs a change.

## Acceptance gates

These values are starting constraints. They are not permanent balance constants. Use playtests to tune them. Do not remove them only to accept more candidates.

Current production layouts have two hard circulation requirements. Production-size room validation applies when `grid.getCellCount() >= 600`. This validation requires exactly two useful cycles. It also requires the planned Shortcut to save at least two arena transitions.

This roadmap uses `useful cycle` for the measured graph property. A useful cycle is a non-Primary mission edge. Its alternate path has at least three arena transitions with that edge closed after connectors are contracted. This definition does not prove a separate physical approach. It does not prove a progression unlock. This roadmap uses `Shortcut` for the typed mission-edge purpose.

Other gates in this section are active acceptance targets. The current code does not enforce all targets. The roadmap keeps each target until the implementation meets it.

### 1. Circulation

A normal radius-8 Fortress map must:

- Publish exactly two useful cycles. Production-size room validation enforces this rule.
- Give at least 70% of substantial rooms two or more arena connections. Current validation does not enforce this hard gate.
- Limit ordinary single-entry Combat rooms to two. Start, Reward, and Exit can be intentional leaves. Current validation does not enforce this hard gate.
- Reject routine dead-end chains deeper than two arena transitions. Current validation does not enforce this hard gate.
- Provide one planned Shortcut that saves at least two arena transitions. Production-size room validation enforces this rule.
- Keep alternate routes physically separate. The routes must give different approaches. Two adjacent doors into one choke do not meet this target. Current validation does not enforce this hard gate.

A raw cycle count is not sufficient. A useful cycle must meet the contracted-graph definition above. Physical route separation is a different target. Progression unlock behavior is also a different target.

### 2. Arena identity

Every accepted map must contain several published room-shape and combat briefs. Use these initial briefs:

- Open kiting arena.
- Long-lane or crossfire arena.
- Broken-sightline or L-shaped arena.
- Perimeter-route arena with central negative space.
- Defend-the-center objective arena.
- Dense close-range arena with more than one safe exit.

Do not accept a map that labels every substantial room `BASE COMPACT`. The generator must create and validate shape metadata. The renderer must not guess this metadata. Cover and objective footprints must keep multiple firing lanes open. They must also keep safe reading space at entrances.

This room and combat grammar is an active target. The generator does not implement it.

### 3. Ingress and anti-funneling

- Give each major Combat room at least two usable player approaches before late pressure starts.
- Place enemy ingress in separate regions. Do not make every fight use the doorway that the player used.
- Give each entrance clear space or cover before hostile ranged fire can reach the player.
- Make each opening and expanded gate stage pass its configured population-slot limit.
- Make each stage pass its configured useful-cycle-count limit.
- Make each stage pass the gate-cost-order checks.
- Make each stage pass the route-reachability checks.

The production profile enforces static spawn-packing limits for checked post-gate states. Each checked state requires 18 packed slots in at least two rooms. This check does not prove runtime spawn placement. Runtime also checks player distance. Runtime checks active walls. Runtime checks occupancy. Runtime uses a larger separation limit. The profile does not check the initial-lock state. It does not check visibility bands. It does not check role compatibility.

### 4. Quest realization

One match seed must derive all inputs in a deterministic way. These inputs include geometry, topology, circulation, room shapes, semantic anchors, the quest recipe, binding, and the bounded retry sequence.

Every accepted map must bind:

- A clear main quest with persistent state and voluntary extraction.
- At least one optional sequence with a useful reward or Shortcut.
- Device anchors that use role, shape, route order, clearance, visibility, and separation. Do not use fixed room IDs or world coordinates.
- A recovery rule for each step that can fail.
- Quest feedback that continues through automatic rounds. Quest logic must not authorize or pause the director.

The first quest family must use existing physical verbs before it adds new systems. These verbs are: hold a zone, kill near a device, activate a discovered order, shoot a target, power a Shortcut, and return to a changed landmark.

The current code simulates stage progression. It finds a stage-safe gate and Anchor binding. It does not implement the full quest compiler. It does not bind typed semantic anchors.

### 5. Pacing and traversal

Measure and playtest these items:

- Opening-component survival before the first gate purchase.
- Time from Start to the first expansion choice.
- Time between quest devices.
- Backtracking saved by Shortcuts.
- Dash and movement value in each room brief.
- Projectile reach and ranged-enemy visibility in long lanes.
- Navigation cost and spawn distribution with a late-round population.

Reject or change a map when traversal time exceeds its target. Reject or change a map when playtests record repeated funnel tactics. Reject or change a map when playtests record repetitive objective routes.

## Implementation order

### Phase A - circulation repair

Current status: Production-size room validation requires exactly two useful cycles. It requires Shortcut savings of at least two arena transitions. The other steps are incomplete.

1. Keep exactly two useful required cycles on production layouts.
2. Add hard gates for substantial-room degree, ordinary leaves, dead-end depth, and alternate-route separation.
3. Keep useful-cycle realization in required routing. Reject the candidate if a required useful-cycle route fails.
4. Score Shortcut value and route diversity. Do not use raw edge count as the main value.
5. Show the degree histogram, ordinary leaves, useful cycles, dead-end depth, and Shortcut savings in F2.

### Phase B - room-shape grammar

Current status: The generator does not implement this phase.

1. Publish a shape and combat brief for each substantial room.
2. Grow geometry from the brief. Do not use only one compact frontier score.
3. Validate elongation, concavity, negative space, doorway spread, local clearance, entrance safety, and the objective footprint.
4. Show the published brief in F2. Add cross-seed shape-distribution tests.

### Phase C - semantic anchors and quest compiler

Use [`quest-recipe-design-guide.md`](quest-recipe-design-guide.md) for presentation rules, the declarative recipe model, passageway reward placement, binding constraints, and the initial curated recipe library.

Current status: The current code finds a stage-safe gate and Anchor binding. It does not publish typed anchors. It does not implement the full quest compiler.

1. Publish focal, perimeter, doorway-facing, holdout, target, Shortcut, and obstruction anchors.
2. Define one authored quest recipe with semantic requirements and dependencies.
3. Bind the recipe to an accepted generated candidate. Reject a candidate if binding is not possible.
4. Replace hard-coded Round 2/5 checks with recipe requirements.
5. Store the exact quest binding in replay metadata and tests.

### Phase D - component-aware endurance validation

Current status: The current code validates stage progression. It validates static post-gate spawn packing. It does not prove runtime spawn placement. It does not validate full endurance. It does not implement the planned navigation work.

1. Simulate gate stages. Measure each reachable component.
2. Validate circulation, ingress, spawn capacity, economy order, and quest solvability at each stage.
3. Add cached navigation fields if profiling shows that larger connected populations need them.
4. Tune movement, camera, interactions, visibility, and lighting only with accepted room briefs.

### Phase E - playtest acceptance gate

Use a fixed seed matrix and fresh seeds. For each map, review F2. Play the opening, expansion, quest, Shortcut unlock, and several late rounds.

Record:

- Landmarks and rooms that players recall after the run.
- Route choices made under pressure.
- Repeated funnel or doorway tactics.
- Backtracking time before and after Shortcuts.
- Areas and routes that quest steps reveal.
- Differences in movement and route choices between seeds.

Keep seeds with recorded acceptance failures as deterministic regressions. Do not resume the broader roadmap until the representative matrix passes. Consecutive fresh matches must differ in recorded route choices or room-mechanic results.

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

The audit sent 100 map requests. Six requests used the fallback. The fallback count uses all 100 requests. The mean attempt count of 3.29 uses all 100 requests. The other audit metrics use the 94 accepted Fortress maps. The mean score for those maps is 58.751. Their mean contracted-cycle count is 2.00. Their mean useful-cycle count is 2.00. Their mean multi-entry ratio is 66.7%. All 94 maps passed progression admission. All 94 maps passed static post-gate spawn packing.

The static packing result does not prove runtime spawn placement. Runtime also checks player distance. Runtime checks active walls. Runtime checks occupancy. Runtime uses a larger separation limit.

Production-size room validation requires exactly two useful cycles. It also requires at least two arena transitions of Shortcut savings. The current code validates stage progression. It also validates static post-gate spawn packing.

The current implementation does not complete these items:

- The 70% multi-entry hard gate.
- A maximum of two Combat leaves.
- The dead-end-depth hard gate.
- Physical route separation.
- Joint room and route construction.
- Room-shape and combat grammar.
- Typed semantic anchors.
- The full quest compiler.
- Initial-lock, visibility, and role-compatible spawn checks.
- Bounded runtime spawn recovery.
- Endurance and navigation work.
- A portable project-owned random generator.

## Exit criteria

The lock-in ends only when all these statements are true:

- Fresh accepted maps contain exactly two useful cycles.
- Fresh accepted maps contain multi-entry combat spaces.
- Fresh accepted maps have no more than two intentional ordinary Combat leaves.
- At least 70% of substantial rooms have two or more arena connections.
- Routine dead ends have a maximum depth of two arena transitions.
- Alternate routes have enough physical separation to create different approaches.
- Room briefs produce different combat mechanics. Different silhouettes alone are not sufficient.
- Main and optional quests bind to semantic anchors in a reproducible way. The quests stay solvable during endless rounds.
- Gate and quest progression opens Shortcuts and changes traversal.
- The same seed reproduces geometry, graph, room briefs, anchors, quest binding, and retry metadata.
- Manual playtest records contain no repeated finding of isolated compact arenas connected by passages.

Until then, level generation and quest realization are the game.
