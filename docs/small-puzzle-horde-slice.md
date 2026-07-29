# Small Puzzle Horde Map Vertical Slice

This document is the interactive and headless acceptance guide for the completed small-map horde **systems tier**. The fixed radius-5, five-substantial-room maps use `worldScale = 0.16F` and select Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing. They remain deterministic regression and fallback fixtures available through `--recipe` and F2.

Normal play now uses the radius-8, `worldScale = 0.22F` Fortress V1 profile with actor-relative physical and capacity gates. `R` preserves the accepted map, `N` generates another, and `--seed` reproduces one within the current build/toolchain. Semantic quest binding and deeper endurance/economy validation remain beyond this fixture guide.

## Run each recipe

```sh
./build/stalberg_game --recipe=hub
./build/stalberg_game --recipe=ring
./build/stalberg_game --recipe=wings
```

F2 opens the complete overview. The first three browser configurations are fixed regression fixtures showing Hub Circuit, Broken Ring, and Twin Wings on the same source grid, making graph differences directly comparable.

## Controls

| Input | Action |
|---|---|
| WASD | Camera-relative movement |
| Mouse / hold LMB | Aim and fire |
| Space | Dash |
| E | Buy a gate within the displayed 2.2-unit range, activate Anchor/Hub/Exit, or buy one missing health at a powered Hub |
| 1 at Hub | Buy the next damage tier |
| 2 at Hub | Buy the next fire-rate tier |
| 3 at Hub | Buy the next dash-recovery tier |
| R | Restart mutable match state on the same accepted map and seed |
| N | Generate and enter a fresh seeded match |
| F1 | Combat regression arena |
| F2 | Full-map overview and recipe browser |
| F3 | Detailed match diagnostics |

Nearby gates and devices publish contextual prompts with their price or prerequisite. Locked thresholds render as a single connected barred gate frame. While the Anchor holdout is active during ordinary combat rounds, the objective explicitly says to stay in the gold ring and that leaving pauses progress; near the optional relays, the prompt explains that the gold target is next and a wrong hit resets the sequence.

## Milestone 1: recipe-driven generation

Interactive check:

1. Open F2.
2. Browse configurations 1–3.
3. Confirm the sidebar reports `HUB CIRCUIT`, `BROKEN RING`, and `TWIN WINGS`.
4. Compare the gold room graph, Exit relationship, Hub position, branch structure, and red gate placement.
5. Confirm each map contains Start, Hub, Combat Anchor, Reward, and Exit rooms.
6. In Hub Circuit, confirm Start, Anchor, Reward, and Exit each have one distinct Hub branch; there is no Start → Anchor shortcut.

The contracted recipe briefs are:

| Recipe | Required semantic edges | Optional edge |
|---|---|---|
| Hub Circuit | Start–Hub, Hub–Exit, Hub–Anchor, Hub–Reward | none |
| Broken Ring | Start–Hub, Hub–Anchor, Anchor–Exit, Hub–Reward | Start–Anchor |
| Twin Wings | Start–Hub, Hub–Exit, Hub–Anchor, Hub–Reward | Anchor–Exit |

Headless coverage: `smallMapRecipesPublishDistinctIntent()` in `stalberg_room_generation_tests` verifies deterministic pre-candidate recipe selection, five substantial rooms, semantic roles, exact required edges, Reward leaf structure, and distinct contracted arena graphs. Hub Circuit rejects every extra semantic edge, and candidate validation rejects layouts whose Start and Anchor transitions leave the Hub less than approximately 65 degrees apart. These checks prevent the adjacent, functionally redundant progression doors found during playtesting.

## Milestone 2: points and permanent gates

Interactive check:

1. Wait for the three-second opening countdown; Round 1 starts automatically.
2. Damage and defeat the six Drifters. Hits award 10 points and each Drifter death awards 60 more.
3. Approach the first orange barred gate. The prompt changes from the remaining amount to its recipe-scaled price when affordable. The displayed prompt and purchase query share the same 2.2-world-unit range.
4. Press E anywhere within that displayed range. Points are deducted and the exact threshold opens permanently.
5. Verify F2 changes that threshold from red to green.

Round 1 guarantees 540 points, enough for the 500/510/520-point Hub Circuit/Broken Ring/Twin Wings first gate. Round 2 guarantees enough for the corresponding 750/765/780-point Anchor route. Reward gates similarly cost 800/816/832. Optional Reward and upgrade spending remains unavailable until the Anchor route is purchased, preventing it from consuming required progression currency.

Headless coverage: `pointsPurchaseGatesAtomically()`, `fortressGateInteractionMatchesTheHudRange()`, and `hordeDamageAwardsPointsOnce()` verify the complete prompt range, exact awards, no duplicate death rewards, affordability, deduction, collision-wall removal, and bidirectional navigation.

## Milestone 3: map-wide rounds

Interactive check:

1. Observe the opening countdown and the automatic five-second timer after every cleanup.
2. Observe `BUILDUP`, `PEAK`, `CLEANUP`, and `INTERMISSION` in the HUD. The current round and next-round countdown replace the old finite `WAVE n/5` display.
3. Confirm no enemies spawn during intermission and that no key press is required to continue.
4. Leave the Anchor and Hub incomplete across multiple rounds; confirm the director continues anyway.
5. Confirm rounds introduce Runners and Casters before the first Elite event on Round 5.

Initial compositions:

| Round | Composition |
|---:|---|
| 1 | 6 Drifters |
| 2 | 8 Drifters, 2 Runners |
| 3 | 10 Drifters, 2 Runners, 1 Caster |
| 4 | 12 Drifters, 3 Runners, 1 Caster |
| 5 | 16 Drifters, 3 Runners, 1 Caster, 1 Elite |

After Round 5, pressure is recipe-aware and deterministic. Spawn budget rises to 48, living enemies cap at 18, every fifth round has an Elite event, and composition progressively substitutes Runners, Casters, and Elites for Drifters. A tier-0 baseline plus ten escalation tiers bound health at 1.8×, movement at 1.25×, hostile projectile speed at 1.4×, firing interval at 0.65×, damage at two, and rewards at 1.5×.

Headless coverage: `automaticDirectorStartsWithoutInputOrPuzzleState()` verifies countdown/intermission ownership. `deterministicDifficultyScalesAndStaysBounded()` snapshots rounds 1, 5, 10, 25, and 100, verifies recipe-aware deterministic schedules, checks monotonic pressure, and exercises the maximum `std::uint64_t` round.

## Milestone 4: horde roles and door-aware navigation

Interactive check:

1. Open a gate during intermission or combat and let the next round begin automatically.
2. Move into the newly opened branch.
3. Confirm enemies route through the exact green threshold rather than crossing walls.
4. Observe slow red Drifters, smaller orange Runners, purple ranged Casters, and large periodic Elites.
5. Train enemies through a doorway and verify local separation prevents complete overlap.

Headless coverage: `enemyNavigationUsesDoorState()` verifies that closed thresholds have no legal edge and become immediately traversable by the same player/enemy navigation contract after purchase.

## Milestone 5: Anchor holdout

Interactive check:

1. Complete two rounds and earn enough for the Anchor route.
2. Buy the route at its threshold, or approach the gold Anchor monument and press E to atomically fund the still-closed route and activate the device.
3. Round timing remains unchanged by activation; the holdout runs concurrently with the director.
4. Remain inside the gold marked radius during active combat. The HUD states that leaving pauses progress; accumulated progress persists when re-entering and across round transitions.
5. Reach eight seconds of accumulated hold time. The Anchor turns green and awards 250 points.

Headless coverage: `anchorInteractionFundsAndStartsHoldout()`, `anchorHoldoutRunsAlongsideAutomaticDirector()`, and `anchorHubRelayProgressesWithoutOwningRounds()` check one-press affordable funding/activation, partial persistent progress, puzzle-independent cleanup/intermission transitions, concurrent completion, and one-time reward.

## Milestone 6: Hub and Exit progression

Interactive check:

1. After powering the Anchor, return to the tall Hub machine.
2. Press E during any phase. The Hub turns cyan and the objective-locked Exit route opens.
3. Survive through Round 5; rounds continue automatically afterward.
4. Reach the Exit monument and press E to extract, or ignore it and continue endless rounds.
5. Press R and confirm the entire economy, gates, rounds, objectives, enemies, projectiles, and upgrades reset.

Headless coverage verifies Anchor → Hub → Exit ordering, objective-gate opening, and whole-match reset.

## Milestone 7: optional relay puzzle and Reward branch

Interactive check:

1. Purchase the recipe-scaled Reward branch.
2. Power the Hub.
3. Shoot the three numbered relay orbs in order: 1, 2, 3.
4. A wrong hit resets sequence progress to zero.
5. Correct completion turns all relays green, awards 400 points, and grants the next fire-rate tier if one remains.

Relay state persists across rounds and resets only with the complete match.

Headless coverage verifies wrong-order reset, ordered completion, persistent reward, and one-time completion.

## Milestone 8: upgrades

After purchasing the Anchor route, the Hub offers three permanent tiers per upgrade:

| Input | Tier costs | Authoritative values after each tier |
|---|---|---|
| `1` damage | 1,200 / 2,400 / 4,200 | 2 / 3 / 4 damage per projectile |
| `2` fire rate | 1,000 / 2,200 / 3,800 | 0.075 / 0.060 / 0.050 second interval |
| `3` dash recovery | 900 / 1,800 / 3,000 | 0.70× / 0.55× / 0.45× cooldown |

Purchases persist across rounds, stop cleanly at tier three, and reset with R. After activation, E at the Hub repeatedly repairs one missing health for `500 + 75 × pressure tier` points, preserving a useful post-cap sink. Existing projectiles retain their pool identity; upgrades and repair alter authoritative fixed-step state rather than only presentation.

Headless coverage: `upgradesChangeAuthoritativeSimulation()` verifies insufficient-funds atomicity, exact escalating deductions and tier values, bounded tiers, and repeatable scaled Hub repair.

## Automated validation

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The dedicated `stalberg_horde_match_tests` target covers recipe binding and costs, automatic countdown/intermission transitions, puzzle-independent advancement, representative scaling snapshots, bounded and overflow-safe schedules, objective sites, progression-currency reservation, atomic Anchor interaction, exact scaled rewards, gate state, Reward-route safety, geometry-safe spawning/separation, navigation, concurrent Anchor/Hub progression, relay ordering, explicit extraction, tiered upgrades, repeatable Hub repair, and complete reset. Existing generation, geometry, generated-level, collision, combat, and F1 regression tests remain active.

`stalberg_match_generation_tests` now verifies different-seed variation, exact same-seed accepted-layout/profile replay, same-map restart, bounded deterministic fallback, Fortress V1 actor-relative geometry and ingress targets, and explicit failure of radius-only growth. Successor acceptance work must add larger-map structural diversity, opening-component circulation/economy capacity, and dynamic quest binding to generated semantic anchors.
