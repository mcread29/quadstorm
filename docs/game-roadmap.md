# Game Roadmap — Level Generation Lock-In

This file uses ASD-STE100 Simplified Technical English for its prose.

The previous post-foundation milestone plan is not active.
[`level-generation-lock-in.md`](level-generation-lock-in.md) is the only active roadmap.
This file is a summary and redirect for that roadmap.
Do not add bosses, more enemies, more economy systems, controller work, meta-progression, or broad presentation polish before this lock-in ends.
Procedural levels and quests must first pass the circulation gates.
They must pass the room-shape and combat grammar gates.
They must also pass the quest-binding, endurance, and navigation gates.

See [`game-handoff.md`](game-handoff.md) for implemented runtime contracts and controls.
See [`level-generation-lock-in.md`](level-generation-lock-in.md) for the detailed acceptance gates.

## Product focus

Request one Fortress V1 map for each match.
Use the validated Systems fallback after attempt exhaustion.
Make its seed replayable.
The fortress must meet these goals:

- Support top-down horde movement through useful cycles and alternate approaches.
- Avoid long sequences of compact, single-entry Combat rooms.
- Change traversal through gates, powered Shortcuts, and quest progress.
- Contain distinct room-shape and combat briefs.
- Do not use only irregular room outlines as identity.
- Bind one main quest with defined state and one optional discovery path to generated semantic anchors.
- Publish landmarks and district differences that reviews can identify.

The generated map stays active for the complete run.
`R` resets mutable state on the same map.
`N` requests a new match seed.
`--seed` reproduces the accepted result in the current build and toolchain.
A portable project RNG is not implemented.

## Current foundation — frozen except where generation needs it

The project has these implemented systems:

- Deterministic fixed-step movement, dash, aim, fire, collision, damage, and interpolation.
- A preserved F1 combat regression arena.
- Immutable generated geometry and exact doorway thresholds.
- Door-aware navigation and synchronized mutable locks.
- Automatic endless rounds with bounded deterministic pressure.
- Map-wide Drifter, Runner, Caster, and Elite pressure.
- Currency, permanent gates, upgrades, Hub repair, Anchor holdout, relay sequence, Hub activation, and voluntary extraction.
- Replayable radius-8 Fortress generation.
- A default `MatchGenerationRequest::attemptBudget` value of eight.
- A validated Systems fallback after attempt exhaustion.
- A `MatchGenerationRequest` that selects the physical profile and defaults to `FortressV1`.
- F2 whole-map inspection with topology and structural metrics.
- Five large-map topology planners and labels.
- Exactly two useful cycles on each accepted production map.
- Each production Shortcut saves at least two arena transitions.
- Progression-stage checks for required route access and useful gate changes.
- The static post-gate spawn-packing check applies to each checked post-gate state.
- Best-valid candidate selection and structured rejection data.

The latest audit sent 100 production requests to the match generator.
The fallback and attempt values cover all 100 requests.
The other values cover the 94 accepted Fortress maps.

| Measure | Result | Scope |
|---|---:|---|
| Fallback | 6 | 100 requests |
| Mean generator attempts | 3.29 | 100 requests |
| Mean score | 58.751 | 94 accepted Fortress maps |
| Mean useful cycles | 2.00 | 94 accepted Fortress maps |
| Multi-entry substantial rooms | 66.7% | 94 accepted Fortress maps |
| Progression safe | 100% | 94 accepted Fortress maps |
| Static post-gate spawn-packing check passed | 100% | 94 accepted Fortress maps |

The circulation pass improved production maps.
It did not complete the lock-in.
The 70% multi-entry hard-gate experiment failed and was removed.
Some archetypes still have too many ordinary Combat leaves.
The room-shape and combat grammar still uses one main growth rule.

## Active phases

### Phase A — circulation repair

Completed work:

- Require exactly two useful cycles on production radius-8 Fortress maps.
- Require each production Shortcut to save at least two arena transitions.
- Make required Cycle and Shortcut routing failures reject a candidate.
- Measure useful cycles, Shortcut arena-transition savings, ordinary Combat leaves, and multi-entry ratio.
- Validate gate progression stages.
- Apply the static post-gate spawn-packing check to each checked post-gate state.

Remaining work:

- Bound ordinary Combat leaves.
- Reject deep routine dead ends.
- Measure physical route separation.
- Improve the multi-entry substantial-room result from 66.7% before a 70% hard gate is restored.
- Expose complete leaf, dead-end-depth, and physical-separation data in F2.

### Phase B — room-shape and combat grammar

Room-shape and combat grammar is the set of generated room-shape briefs, combat briefs, and validation rules.

- Generate and publish distinct room-shape and combat briefs.
- Support kiting, long-lane, broken-sightline, perimeter, center-defense, and dense multi-exit rooms.
- Validate doorway spread, entrance safety, cover lanes, negative space, objective footprint, and enemy ingress.
- Reject maps that use one compact grammar for all substantial rooms.
- Join room growth and route construction where separate passes fail a required route or room brief.

### Phase C — semantic anchors and quests

- Generate focal, perimeter, doorway-facing, holdout, target, Shortcut, and obstruction anchors.
- Compile one authored main-quest family and one optional sequence.
- Do not use fixed coordinates or room IDs for quest binding.
- Replace hard-coded round checks with recipe-authored requirements.
- Keep quest progress through automatic rounds.
- Make quest progress change routes or landmarks.
- Reproduce the exact quest binding from the same request data.

### Phase D — component-aware endurance and pacing

Progression-stage checks are implemented.
The static post-gate spawn-packing check is implemented.
They do not cover all runtime states.
The static post-gate spawn-packing check does not prove runtime placement.
Runtime placement also uses player distance, active walls, occupancy, and a larger separation distance.

- Add initial-lock spawn-capacity checks.
- Add visibility-band and spawn-role compatibility checks.
- Add bounded runtime spawn recovery.
- Validate endurance at rounds 1, 5, 25, and maximum pressure.
- Tune traversal, dash, and projectile suitability for each generated room brief.
- Tune visibility, interaction ranges, and camera for each generated room brief.
- Measure navigation cost for each generated room brief.
- Add cached reverse navigation fields if profiling requires them.
- Make Shortcuts reduce measured backtracking.
- Prevent major encounters from using only doorway funnels.

### Phase E — map-quality gate

- Review a fixed seed matrix in F2 and normal play.
- Review consecutive request seeds in F2 and normal play.
- Save each seed that fails a named acceptance check as a deterministic regression.
- Record route choice, funnel use, and physical route separation.
- Record landmark identification and room-brief distinction.
- Record quest-map integration and Shortcut arena-transition savings.
- Resume broader game planning only when the fixed matrix and consecutive-seed sample pass these checks.

## Non-negotiable initial gates

Production radius-8 Fortress candidates currently meet these gates:

- Exactly two useful contracted-arena cycles.
- Each production Shortcut saves at least two arena transitions.
- Safe progression at each checked gate stage.
- 18 statically packed spawn slots in at least two rooms at each checked post-gate stage.

The following target gates remain:

- At least 70% of substantial rooms have degree two or greater.
- At most two ordinary single-entry Combat rooms.
- No routine dead-end chain is deeper than two arena transitions.
- Alternate routes have useful physical separation.
- Several distinct room briefs exist on each map.
- Not every arena uses `BASE COMPACT`.
- One reproducible main quest binds to semantic anchors.
- One reproducible optional discovery path binds to semantic anchors.
- Initial-lock spawn, visibility-band, and spawn-role compatibility checks pass.
- Runtime spawn recovery is bounded.

Tune thresholds from playtests when evidence supports a change.
Do not weaken candidate acceptance only to increase yield.

## Deferred until lock-in exits

- Bosses, Wardens, mutation events, and more enemy roles.
- More currencies, upgrades, perks, traps, services, and ammunition systems.
- Meta-progression and score expansion.
- Controller and accessibility expansion that is not necessary for generation playtests.
- More finale families.
- Cooperative or competitive multiplayer.
- Broad audiovisual polish that does not improve map readability or quest feedback.

## Engineering gates

Run these checks after code changes:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run this graphical smoke test:

```sh
timeout 3s xvfb-run -a ./build/stalberg_game
```

Run this interactive playtest:

```sh
DISPLAY=:0 ./build/stalberg_game
```
