# Small Puzzle Horde Map Vertical Slice

This document is the interactive and headless acceptance guide for the first intentional small-map horde match. The active generated level uses radius 5 and five substantial rooms. Generation selects one of three gameplay recipes before candidate placement and routing: **Hub Circuit**, **Broken Ring**, or **Twin Wings**. The required Anchor is a combat holdout objective, not a logic puzzle; the optional relay sequence is the slice's first deliberately simple puzzle.

## Run each recipe

```sh
./build/stalberg_game --recipe=hub
./build/stalberg_game --recipe=ring
./build/stalberg_game --recipe=wings
```

F2 opens the complete overview. The first three browser configurations show Hub Circuit, Broken Ring, and Twin Wings on the same source grid, making graph differences directly comparable.

## Controls

| Input | Action |
|---|---|
| WASD | Camera-relative movement |
| Mouse / hold LMB | Aim and fire |
| Space | Dash |
| N | Start the next round during intermission |
| E | Buy a nearby gate or activate the Anchor, Hub, or Exit; at Anchor, fund-and-start atomically when affordable |
| 1 at Hub | Buy damage upgrade for 1,200 points |
| 2 at Hub | Buy fire-rate upgrade for 1,000 points |
| 3 at Hub | Buy dash-recovery upgrade for 900 points |
| R | Reset the complete match |
| F1 | Combat regression arena |
| F2 | Full-map overview and recipe browser |
| F3 | Detailed match diagnostics |

Nearby gates and devices publish contextual prompts with their price or prerequisite. Locked thresholds render as a single connected barred gate frame. During the Anchor wave, the objective explicitly says to stay in the gold ring and that leaving pauses progress; near the optional relays, the prompt explains that the gold target is next and a wrong hit resets the sequence.

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

1. Press N to start Round 1.
2. Damage and defeat the six Drifters. Hits award 10 points and each Drifter death awards 60 more.
3. Approach the first orange barred gate. The prompt changes from the remaining amount to `E OPEN GATE 500 POINTS` when affordable.
4. Press E. Points are deducted and the exact threshold opens permanently.
5. Verify F2 changes that threshold from red to green.

Round 1 guarantees 540 points, enough for the 500-point first gate. Round 2 guarantees enough for the 750-point Anchor route. Reward and upgrade spending remains unavailable until the Anchor route is purchased, preventing optional spending from consuming required progression currency.

Headless coverage: `pointsPurchaseGatesAtomically()` and `hordeDamageAwardsPointsOnce()` verify exact awards, no duplicate death rewards, affordability, deduction, collision-wall removal, and bidirectional navigation.

## Milestone 3: map-wide rounds

Interactive check:

1. Press N during intermission.
2. Observe `BUILDUP`, `PEAK`, `CLEANUP`, and `INTERMISSION` in the HUD. After Round 2, ordinary progression pauses until the Anchor holdout begins; its objective-wave cleanup waits for the full hold duration before returning to intermission.
3. Confirm no enemies spawn during intermission.
4. Confirm rounds introduce roles in order: Drifter, Runner, Caster, then the finale Elite.

Initial compositions:

| Round | Composition |
|---:|---|
| 1 | 6 Drifters |
| 2 | 8 Drifters, 2 Runners |
| 3 | 10 Drifters, 2 Runners, 1 Caster |
| 4 | 12 Drifters, 3 Runners, 1 Caster |
| 5 | 14 Drifters, 4 Runners, 2 Casters, 1 Elite |

Headless coverage: `roundDirectorPublishesDeterministicRoles()` verifies exact schedules and phase initialization.

## Milestone 4: horde roles and door-aware navigation

Interactive check:

1. Open a gate during intermission and begin another round.
2. Move into the newly opened branch.
3. Confirm enemies route through the exact green threshold rather than crossing walls.
4. Observe slow red Drifters, smaller orange Runners, purple ranged Casters, and the large finale Elite.
5. Train enemies through a doorway and verify local separation prevents complete overlap.

Headless coverage: `enemyNavigationUsesDoorState()` verifies that closed thresholds have no legal edge and become immediately traversable by the same player/enemy navigation contract after purchase.

## Milestone 5: Anchor holdout

Interactive check:

1. Complete two rounds and earn enough for the Anchor route.
2. Buy the route at its threshold, or approach the gold Anchor monument and press E to atomically fund the still-closed route and activate the device.
3. The next round starts automatically.
4. Remain inside the gold marked radius. The HUD states that leaving pauses progress; accumulated progress persists when re-entering.
5. Reach eight seconds of accumulated hold time. The Anchor turns green and awards 250 points.

Headless coverage: `anchorInteractionFundsAndStartsHoldout()`, `cleanupWaitsForActualAnchorHoldout()`, and `anchorHubRelayAndExitFormPuzzleProgression()` check one-press affordable funding/activation, activation prerequisites, partial persistent progress, objective-wave cleanup blocking, exact completion, and one-time reward.

## Milestone 6: Hub and Exit progression

Interactive check:

1. After powering the Anchor, return to the tall Hub machine.
2. Press E during intermission. The Hub turns cyan and the objective-locked Exit route opens.
3. Complete Round 5.
4. Reach the Exit monument and press E to complete the map.
5. Press R and confirm the entire economy, gates, rounds, objectives, enemies, projectiles, and upgrades reset.

Headless coverage verifies Anchor → Hub → Exit ordering, objective-gate opening, and whole-match reset.

## Milestone 7: optional relay puzzle and Reward branch

Interactive check:

1. Purchase the 800-point Reward branch.
2. Power the Hub.
3. Shoot the three numbered relay orbs in order: 1, 2, 3.
4. A wrong hit resets sequence progress to zero.
5. Correct completion turns all relays green, awards 400 points, and grants the fire-rate upgrade.

Relay state persists across rounds and resets only with the complete match.

Headless coverage verifies wrong-order reset, ordered completion, persistent reward, and one-time completion.

## Milestone 8: upgrades

After purchasing the Anchor route, the Hub offers:

- `1`: damage increases from one to two per projectile.
- `2`: fire interval decreases from 0.10 to 0.075 seconds.
- `3`: dash cooldown scales to 70% of its base duration.

Purchases are permanent for the current match, cannot be bought twice, and reset with R. Existing projectiles retain their normal pool identity; upgrades alter authoritative fixed-step behavior rather than only presentation.

Headless coverage: `upgradesChangeAuthoritativeSimulation()` verifies prices, one-time ownership, and the actual combat/player fields.

## Automated validation

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The dedicated `stalberg_horde_match_tests` target covers recipe binding, objective sites, progression-currency reservation, atomic Anchor interaction, exact kill rewards, gate state, optional Reward-route safety, geometry-safe spawning/separation, round schedules and cleanup, navigation, Anchor/Hub/Exit progression, relay ordering, upgrades, and complete reset. Existing generation, geometry, generated-level, collision, combat, and F1 regression tests remain active.
