# Level Generation Lock-In

This is the only active product roadmap. All work after the current endless-horde foundation is paused until generated maps and their quests consistently feel good to play. See [`level-generator-evaluation.md`](level-generator-evaluation.md) for the current implementation audit, seed-matrix evidence, and proposed source-level improvement sequence.

The recent topology pass was useful instrumentation, but it did not solve the oatmeal problem. A named archetype is not meaningful identity when the accepted result is still a tree of compact, single-entry arenas. Graph labels, irregular silhouettes, and role colors are diagnostics—not acceptance criteria.

## Product goal

Each new match should produce one memorable fortress that feels designed for top-down horde combat and supports a coherent main quest plus at least one optional discovery path. The map persists for the run and remains reproducible from the displayed match seed.

A good map must create:

- Route choices during combat, not only choices about which dead end to buy first.
- Multiple approaches, escapes, and shortcuts that change as gates and quest devices activate.
- Rooms with mechanically different movement, sightline, cover, ingress, and objective patterns.
- Landmarks and districts that let the player form a mental map.
- Quest dependencies that use the generated space rather than sitting on top of it.
- Meaningful reasons to revisit an area without turning every objective into hub backtracking.

## Work freeze

Do not add bosses, mutation systems, more enemy roles, more upgrades, controller work, meta-progression, additional currencies, or presentation polish unrelated to map readability until this lock-in exits.

Existing combat, endless rounds, economy, gates, Anchor/Hub/relay/Exit interactions, fixed fixtures, and regression arena remain the test harness. Change those systems only when needed to evaluate generated-map circulation, quests, or pacing.

## Acceptance gates

These are starting constraints, not permanent balance constants. Tune them from playtests, but do not remove them merely to increase candidate acceptance.

### 1. Circulation

A normal radius-8 Fortress map must:

- Publish two or three useful contracted-arena cycles.
- Give at least 70% of substantial rooms two or more arena connections.
- Limit ordinary single-entry Combat rooms to two; Start, Reward, and Exit may be intentional leaves.
- Reject routine dead-end chains deeper than two arena transitions.
- Provide at least one shortcut that changes a previously expensive return trip after a gate or quest step.
- Keep alternate routes physically separated enough to create different approaches rather than two neighboring doors into the same choke.

Cycle count alone is insufficient. A useful cycle must shorten a meaningful route, provide a distinct combat approach, or become a readable unlock. Tiny local triangles and parallel adjacent thresholds do not count.

### 2. Arena identity

Every accepted map must contain several published room-shape and combat briefs. Initial briefs:

- Open kiting arena.
- Long-lane or crossfire arena.
- Broken-sightline or L-shaped arena.
- Perimeter-route arena with central negative space.
- Defend-the-center objective arena.
- Dense close-range arena with more than one safe exit.

No accepted map may label every substantial room `BASE COMPACT`. Shape metadata must be generated and validated, not guessed by the renderer. Cover and objective footprints must preserve multiple firing lanes and safe reading space inside entrances.

### 3. Ingress and anti-funneling

- Major Combat rooms require at least two usable player approaches by the time they host late pressure.
- Enemy ingress must come from separated regions and must not reduce every fight to shooting through the doorway the player just used.
- Entrances need immediate readable space or cover before hostile ranged fire can target the player.
- Opening and expanded gate components must each support their scheduled population, circulation, and economy.

### 4. Quest realization

One match seed must deterministically derive geometry inputs, topology/circulation brief, room-shape briefs, semantic anchors, quest recipe, binding, and bounded retry sequence.

Every accepted map must bind:

- A readable main quest with persistent state and voluntary extraction.
- At least one optional sequence with a meaningful reward or shortcut.
- Device anchors selected by role, shape, route order, clearance, visibility, and separation—not fixed room IDs or world coordinates.
- A recovery rule for every failure-capable step.
- Quest feedback that persists across automatic rounds and never authorizes or pauses the director.

The first quest family should use existing physical verbs before adding new systems: hold a zone, kill near a device, activate a discovered order, shoot a target, power a shortcut, and return to a visibly changed landmark.

### 5. Pacing and traversal

Measure and playtest:

- Opening-component survival before the first gate purchase.
- Time from Start to the first expansion choice.
- Time between quest devices and backtracking saved by shortcuts.
- Dash and movement usefulness in each room brief.
- Projectile reach and ranged-enemy visibility across long lanes.
- Navigation cost and spawn distribution under late-round population.

A map that passes geometry metrics but feels slow, funnel-heavy, or administratively repetitive must still be rejected or reworked.

## Implementation order

### Phase A — circulation repair

1. Replace zero/one-loop large-map acceptance with two-to-three useful required cycles.
2. Add hard gates for substantial-room degree, ordinary leaves, dead-end depth, and alternate-route separation.
3. Make loop realization part of required routing; a failed loop rejects the candidate.
4. Score shortcut value and route diversity, not raw edge count.
5. Expose degree histogram, ordinary leaves, useful cycles, dead-end depth, and shortcut savings in F2.

### Phase B — room-shape grammar

1. Publish a shape/combat brief for each substantial room.
2. Grow geometry according to the brief rather than one compact frontier score.
3. Validate elongation, concavity, negative space, doorway spread, local clearance, entrance safety, and objective footprint.
4. Render the published brief in F2 and add cross-seed shape-distribution tests.

### Phase C — semantic anchors and quest compiler

Use [`quest-recipe-design-guide.md`](quest-recipe-design-guide.md) for the presentation rules, declarative recipe model, passageway reward placement, binding constraints, and initial curated recipe library.

1. Publish focal, perimeter, doorway-facing, holdout, target, shortcut, and obstruction anchors.
2. Define one authored quest recipe as semantic requirements and dependencies.
3. Bind it to an accepted generated candidate and reject impossible bindings.
4. Replace hard-coded Round 2/5 checks with recipe-authored requirements.
5. Persist exact quest binding in replay metadata and tests.

### Phase D — component-aware endurance validation

1. Simulate gate stages and measure each reachable component.
2. Validate circulation, ingress, spawn capacity, economy order, and quest solvability at every stage.
3. Add cached navigation fields if profiling shows the larger connected populations need them.
4. Tune movement, camera, interactions, visibility, and lighting only against accepted room briefs.

### Phase E — feel gate

Use a fixed seed matrix plus fresh seeds. For every map, review F2 and play through the opening, expansion, quest, shortcut unlock, and several late rounds.

Record:

- Memorable landmarks and rooms.
- Actual route choices made under pressure.
- Repeated funnel or doorway tactics.
- Backtracking time before and after shortcuts.
- Whether the quest teaches the map.
- Whether two seeds demand meaningfully different movement and planning.

Capture bad seeds as deterministic regressions. Do not resume the broader roadmap until the representative matrix passes and consecutive fresh matches are consistently distinguishable in both overview and play.

## Exit criteria

The lock-in is complete only when:

- Fresh accepted maps consistently contain useful loops, multi-entry combat spaces, and limited intentional leaves.
- Room briefs produce mechanically distinct fights, not only different silhouettes.
- Main and optional quests bind reproducibly to semantic anchors and remain solvable during endless rounds.
- Gate and quest progression opens shortcuts and changes traversal meaningfully.
- The same seed reproduces geometry, graph, room briefs, anchors, quest binding, and retry metadata.
- Manual playtests no longer describe the normal result as isolated compact arenas connected by passages.

Until then, level generation and quest realization are the game.
