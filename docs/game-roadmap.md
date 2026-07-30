# Game Roadmap — Level Generation Lock-In

The previous post-foundation milestone plan is superseded. The only active roadmap is [`level-generation-lock-in.md`](level-generation-lock-in.md). The game will not advance to bosses, more enemies, more economy systems, controller work, meta-progression, or broad presentation polish until procedural levels and their quests consistently feel authored and good to play.

For implemented runtime contracts and controls, see [`game-handoff.md`](game-handoff.md). For the diagnosis of the current oatmeal problem and the detailed acceptance gates, see [`level-generation-lock-in.md`](level-generation-lock-in.md).

## Product focus

Build one fresh, replayable-seed fortress per match that:

- Supports top-down horde movement through useful loops and alternate approaches.
- Avoids a sequence of compact, single-entry combat rooms.
- Changes traversal through gates, powered shortcuts, and quest progress.
- Contains mechanically distinct room briefs, not merely irregular silhouettes.
- Binds a coherent main quest and optional discovery path to generated semantic anchors.
- Remains readable enough for the player to learn and remember during one run.

The generated map persists for the run. `R` resets mutable state on the same map; `N` requests a new match seed; `--seed` reproduces the accepted result within the current build/toolchain.

## Current foundation — frozen except where generation needs it

The project already provides:

- Deterministic fixed-step movement, dash, aiming, firing, collision, damage, and interpolation.
- A preserved F1 combat regression arena.
- Immutable generated geometry, exact doorway thresholds, door-aware navigation, and mutable synchronized locks.
- Automatic endless rounds with bounded deterministic pressure.
- Map-wide Drifter, Runner, Caster, and Elite pressure.
- Currency, permanent gates, upgrades, Hub repair, Anchor holdout, relay sequence, Hub activation, and voluntary extraction.
- Replayable radius-8 Fortress generation with bounded retries and visible fallback.
- F2 whole-map inspection with topology and structural metrics.
- Five large-map topology labels and planners.

The latest topology pass improved diagnostics but did not solve oatmeal. A Hub-and-Spokes result with zero cycles, many single-entry arenas, and uniformly `BASE COMPACT` room briefs is not acceptable merely because it satisfies its named graph plan.

## Active phases

### Phase A — circulation repair

- Require two or three useful cycles on normal Fortress maps.
- Require most substantial rooms to have multiple arena connections.
- Limit ordinary Combat leaves and reject deep routine dead ends.
- Make useful loops required routing constraints rather than optional attempts.
- Measure alternate-route separation and shortcut savings.
- Expose and test ordinary leaves, dead-end depth, useful cycles, and multi-entry ratio.

### Phase B — room-shape and combat grammar

- Generate and publish distinct shape/combat briefs.
- Support kiting, long-lane, broken-sightline, perimeter, center-defense, and dense multi-exit rooms.
- Validate doorway spread, entrance safety, cover lanes, negative space, objective footprint, and enemy ingress.
- Reject maps whose substantial rooms collapse back to one compact grammar.

### Phase C — semantic anchors and quests

- Generate focal, perimeter, doorway-facing, holdout, target, shortcut, and obstruction anchors.
- Bind one authored main-quest family and one optional sequence without fixed coordinates or room IDs.
- Replace hard-coded round checks with recipe-authored requirements.
- Make quest progress persist through automatic rounds and visibly change routes or landmarks.
- Reproduce exact quest binding from the match seed.

### Phase D — component-aware endurance and pacing

- Validate every gate stage for circulation, ingress, population, economy, and quest solvability.
- Tune traversal, dash, projectile suitability, visibility, interaction ranges, camera, and navigation against generated room briefs.
- Ensure shortcuts reduce real backtracking and major encounters cannot all be solved by doorway funneling.

### Phase E — feel gate

- Review a fixed seed matrix and consecutive fresh seeds in F2 and normal play.
- Capture bad seeds as deterministic regressions.
- Score actual route choice, funneling, landmark memory, room distinction, quest-map integration, and shortcut value.
- Resume broader game planning only after the representative matrix consistently feels good.

## Non-negotiable initial gates

For normal radius-8 Fortress candidates:

- Two or three useful contracted-arena cycles.
- At least 70% of substantial rooms have degree two or greater.
- At most two ordinary single-entry Combat rooms.
- No routine dead-end chain deeper than two arena transitions.
- At least one progression-driven shortcut that removes a meaningful return trip.
- Several distinct published room briefs; not every arena may be `BASE COMPACT`.
- A reproducible main quest and optional discovery path bound to semantic anchors.

These thresholds may be tuned from playtests, but candidate acceptance must not be weakened simply to preserve yield.

## Deferred until lock-in exits

- Bosses, Wardens, mutation events, and additional enemy roles.
- More currencies, upgrades, perks, traps, services, or ammunition systems.
- Meta-progression and score expansion.
- Controller and accessibility expansion beyond changes needed for generation playtests.
- Additional finale families.
- Cooperative or competitive multiplayer.
- Broad audiovisual polish unrelated to map readability or quest feedback.

## Engineering gates

After code changes:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Graphical smoke test:

```sh
timeout 3s xvfb-run -a ./build/stalberg_game
```

Interactive playtest:

```sh
DISPLAY=:0 ./build/stalberg_game
```
