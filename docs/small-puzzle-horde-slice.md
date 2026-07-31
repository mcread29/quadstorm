# Small Puzzle Horde Map Vertical Slice

This guide uses practical ASD-STE100 Simplified Technical English. It gives short acceptance instructions and uses consistent terms.

This document is the interactive and headless acceptance guide for the completed small-map horde **systems tier**.

Fixture properties:

- Radius 5.
- Five substantial rooms.
- `worldScale = 0.16F`.
- A fixed Hub Circuit, Broken Ring, or Twin Wings recipe supplied before candidate placement and routing.
- Deterministic regression and fallback use.
- Access through `--recipe` or F2.

Normal play uses the radius-8 Fortress V1 profile. This profile uses `worldScale = 0.22F`. It uses actor-relative physical and capacity gates.

Current production status:

- Production maps have exactly two useful cycles.
- Each Shortcut saves at least two arena transitions.
- Progression-stage checks are complete.
- The implemented stage-spawn validation is a static post-gate spawn-packing check.
- The static post-gate spawn-packing check does not prove runtime spawn behavior.

A useful cycle is a non-Primary mission edge whose removal leaves an alternate path of at least three arena transitions in the connector-contracted arena graph.

Current audit:

- The audit used 100 requested match seeds.
- Six requests used the radius-5 fallback.
- The audit calculated production-map metrics from 94 accepted radius-8 maps.
- The mean multi-entry substantial-room ratio was 66.7%.

`R` keeps the accepted map. `N` generates another map. `--seed` reproduces a map in the current build and toolchain.

This fixture guide does not validate these current production gaps:

- Bounded Combat leaves and dead ends.
- Physical route separation.
- Room and combat grammar.
- Typed semantic anchors and the full quest compiler.
- Initial-lock, visibility, and role spawn checks.
- Opening-component circulation and economy checks.
- Runtime recovery, endurance, and navigation checks.

## Run each recipe

```sh
./build/stalberg_game --recipe=hub
./build/stalberg_game --recipe=ring
./build/stalberg_game --recipe=wings
```

Press F2 to open the full overview. Configuration 1 shows the active map. Configurations 2–4 are fixed regression fixtures. They show Hub Circuit, Broken Ring, and Twin Wings on the same source grid. This permits direct graph comparison.

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

Nearby gates and devices show context prompts. The prompts show the price or prerequisite. Locked thresholds have one connected barred gate frame.

Objective prompts:

- The Anchor holdout runs during normal combat rounds.
- The Anchor objective tells the player to stay in the gold ring.
- It tells the player that progress pauses outside the ring.
- The relay prompt shows the next gold target.
- It says that a wrong hit resets the sequence.

## Milestone 1: recipe-driven generation

Interactive check:

1. Open F2.
2. Confirm that configuration 1 shows the active map.
3. Browse configurations 2–4.
4. Confirm that the sidebar shows `HUB CIRCUIT`, `BROKEN RING`, and `TWIN WINGS` for configurations 2–4.
5. Compare the gold room graph and the Exit relationship.
6. Compare the Hub position, branch structure, and red gate placement.
7. Confirm that each fixed recipe map has Start, Hub, Combat Anchor, Reward, and Exit rooms.
8. In Hub Circuit, confirm that Start, Anchor, Reward, and Exit have separate Hub branches.
9. Confirm that Hub Circuit has no Start → Anchor Shortcut.

The contracted recipe briefs are:

| Recipe | Required semantic edges | Optional edge |
|---|---|---|
| Hub Circuit | Start–Hub, Hub–Exit, Hub–Anchor, Hub–Reward | none |
| Broken Ring | Start–Hub, Hub–Anchor, Anchor–Exit, Hub–Reward | Start–Anchor |
| Twin Wings | Start–Hub, Hub–Exit, Hub–Anchor, Hub–Reward | Anchor–Exit |

Headless coverage for `smallMapRecipesPublishDistinctIntent()` in `stalberg_room_generation_tests`:

- Supplies Hub Circuit, Broken Ring, and Twin Wings directly through `RoomGenerationOptions`.
- Checks that each generated layout reports the supplied recipe.
- Counts five substantial rooms.
- Counts one Hub and one Reward room.
- Checks the presence of all required edges.
- Checks the Reward leaf structure.
- Checks different contracted arena graphs.
- Checks that the Hub Circuit contracted graph has no additional edge.

Coverage limits for the named test:

- It does not test seed-driven recipe selection.
- It does not assert every semantic role.
- It does not construct a candidate that fails the branch-angle rule.
- Five-arena small-map candidate validation separately requires a normalized direction dot product of `0.42F` or less between the Start and Anchor transitions from the Hub.
- The `0.42F` rule does not apply to large-map candidates.

## Milestone 2: points and permanent gates

Interactive check:

1. Wait for the three-second opening countdown.
2. Confirm that Round 1 starts automatically.
3. Damage and defeat the six Drifters.
4. Confirm that each hit gives 10 points.
5. Confirm that each Drifter death gives 60 more points.
6. Go near the first orange barred gate.
7. Confirm that the prompt first shows the missing amount.
8. Confirm that the prompt shows the recipe-scaled price when you can buy the gate.
9. Confirm that the prompt and purchase query use the same 2.2-world-unit range.
10. Press E in that displayed range.
11. Confirm that the game deducts the points.
12. Confirm that the exact threshold opens permanently.
13. Open F2.
14. Confirm that the threshold changes from red to green.

Gate economy values:

- Round 1 guarantees 540 points.
- Hub Circuit, Broken Ring, and Twin Wings first gates cost 500, 510, and 520 points.
- Round 2 guarantees the points required for the related Anchor route.
- Hub Circuit, Broken Ring, and Twin Wings Anchor routes cost 750, 765, and 780 points.
- Reward gates cost 800, 816, and 832 points.
- Optional Reward and upgrade spending stays disabled until the player buys the Anchor route.

Headless coverage:

- `pointsPurchaseGatesAtomically()` checks affordability and point deduction.
- It checks the doorway lock state.
- It checks `canTraverse()` in both directions.
- It does not assert active-wall removal or collision behavior.
- `hordeDamageAwardsPointsOnce()` checks hit awards and duplicate death-reward suppression.

Coverage for `fortressGateInteractionMatchesTheHudRange()`:

- Uses one interaction point.
- Places the point `0.1F` inside the configured 2.2-unit limit.
- Does not check the exact boundary.
- Does not check a point outside the limit.

## Milestone 3: map-wide rounds

Interactive check:

1. Observe the opening countdown.
2. Observe the automatic five-second timer after each cleanup.
3. Observe `BUILDUP`, `PEAK`, `CLEANUP`, and `INTERMISSION` in the HUD.
4. Confirm that the current round and next-round countdown replace the old finite `WAVE n/5` display.
5. Confirm that no enemy spawns during intermission.
6. Confirm that no key press is necessary to continue.
7. Keep the Anchor and Hub incomplete for several rounds.
8. Confirm that the director continues.
9. Confirm that rounds introduce Runners and Casters before the first Elite event on Round 5.

Initial compositions:

| Round | Composition |
|---:|---|
| 1 | 6 Drifters |
| 2 | 8 Drifters, 2 Runners |
| 3 | 10 Drifters, 2 Runners, 1 Caster |
| 4 | 12 Drifters, 3 Runners, 1 Caster |
| 5 | 16 Drifters, 3 Runners, 1 Caster, 1 Elite |

Pressure after Round 5:

- The schedule remains deterministic and depends on the recipe.
- The spawn budget increases to 48.
- The living-enemy limit increases to 18.
- Each fifth round has an Elite event.
- Later compositions replace more Drifters with Runners, Casters, and Elites.

Final escalation limits:

- Health: 1.8×.
- Movement: 1.25×.
- Hostile projectile speed: 1.4×.
- Firing interval: 0.65× minimum scale.
- Damage: two.
- Rewards: 1.5×.

Headless coverage:

- `automaticDirectorStartsWithoutInputOrPuzzleState()` checks countdown and intermission ownership.
- `deterministicDifficultyScalesAndStaysBounded()` records Hub Circuit rounds 1, 5, 10, 25, and 100.
- It checks monotonic Hub Circuit pressure through Round 100.
- It compares the Round 25 Hub Circuit and Twin Wings schedules.
- It tests the maximum `std::uint64_t` Hub Circuit round.
- It does not compare all three recipes.

## Milestone 4: horde roles and door-aware navigation

Interactive check:

1. Open a gate during intermission or combat.
2. Let the next round start automatically.
3. Move into the new branch.
4. Confirm that enemies use the exact green threshold.
5. Confirm that enemies do not cross walls.
6. Observe slow red Drifters.
7. Observe smaller orange Runners and purple ranged Casters.
8. Observe large periodic Elites.
9. Train enemies through a doorway.
10. Confirm that local separation prevents full overlap.

Headless coverage for `enemyNavigationUsesDoorState()`:

- Checks that a closed threshold has no legal edge.
- Checks that the navigation contract makes the threshold traversable immediately after purchase.

## Milestone 5: Anchor holdout

Interactive check:

1. Complete two rounds.
2. Confirm that your points equal or exceed the displayed Anchor-route cost.
3. Select step 4 or step 5.
4. Buy the route at its threshold. Then go to the gold Anchor monument and press E to activate it.
5. As an alternative to step 4, go to the gold Anchor monument and press E. Confirm that this interaction buys the closed route and activates the device.
6. Confirm that activation does not change round timing.
7. Confirm that the holdout runs at the same time as the director.
8. Stay in the gold marked radius during active combat.
9. Confirm that the HUD says that progress pauses when you leave.
10. Leave and enter the radius again.
11. Confirm that progress continues from its stored value.
12. Confirm that progress persists across round transitions.
13. Accumulate eight seconds of hold time.
14. Confirm that the Anchor turns green.
15. Confirm that the player gets 250 points.

Headless coverage:

- `anchorInteractionFundsAndStartsHoldout()` checks one-press funding and activation.
- `anchorHoldoutRunsAlongsideAutomaticDirector()` checks phase changes and concurrent completion.
- `anchorHubRelayProgressesWithoutOwningRounds()` checks partial progress and one 250-point completion award.
- These tests do not attempt a second Anchor completion.
- These tests do not prove duplicate suppression for the Anchor reward.

## Milestone 6: Hub and Exit progression

Interactive check:

1. Power the Anchor.
2. Return to the tall Hub machine.
3. Press E during any phase.
4. Confirm that the Hub turns cyan.
5. Confirm that the objective-locked Exit route opens.
6. Continue through Round 5.
7. Confirm that rounds continue automatically after Round 5.
8. Go to the Exit monument and press E to extract.
9. As an alternative to step 8, ignore the Exit and continue the endless rounds.
10. Press R.
11. Confirm that the economy, gates, rounds, objectives, enemies, projectiles, and upgrades reset.

Headless coverage:

- Checks Anchor → Hub → Exit order.
- Checks objective-gate opening.
- Checks full-match reset.

## Milestone 7: optional relay puzzle and Reward branch

Interactive check:

1. Buy the recipe-scaled Reward branch.
2. Power the Hub.
3. Shoot relay orb 1.
4. Continue through one round transition.
5. Confirm that relay progress remains at one.
6. Shoot relay orb 3.
7. Confirm that sequence progress resets to zero.
8. Shoot the three numbered relay orbs in this order: 1, 2, 3.
9. Confirm that all relays turn green.
10. Confirm that the player gets 400 points.
11. Confirm that the player gets the next fire-rate tier if a tier remains.
12. Continue through one more round transition.
13. Confirm that the completed relay state remains.
14. Shoot a relay again.
15. Confirm that the player does not get more relay points or another relay upgrade.

Required behavior:

- Correct sequence progress persists across rounds.
- A wrong relay input resets sequence progress to zero.
- Relay completion persists across rounds.
- Only a full match reset clears relay completion.

Headless coverage:

- Checks wrong-order reset and ordered completion.
- Checks the grant of a fire-rate upgrade.
- Does not inspect the 400-point relay award.
- Does not attempt a second completed sequence.
- Does not move relay progress or completion across a round transition.

## Milestone 8: upgrades

After the player buys the Anchor route, the Hub offers three permanent tiers for each upgrade:

| Input | Tier costs | Authoritative values after each tier |
|---|---|---|
| `1` damage | 1,200 / 2,400 / 4,200 | 2 / 3 / 4 damage per projectile |
| `2` fire rate | 1,000 / 2,200 / 3,800 | 0.075 / 0.060 / 0.050 second interval |
| `3` dash recovery | 900 / 1,800 / 3,000 | 0.70× / 0.55× / 0.45× cooldown |

Upgrade state:

- Purchases persist across rounds.
- Purchases stop at tier three.
- `R` resets all upgrade tiers.
- After Hub activation, E repairs one missing health.
- Each repair costs `500 + 75 × pressure tier` points.
- Repair remains available after the upgrade tier limit.

Existing projectiles keep their pool identity. Upgrades and repair change authoritative fixed-step state. They do not change only the presentation.

Headless coverage for `upgradesChangeAuthoritativeSimulation()`:

- Checks insufficient-funds atomicity.
- Performs all three purchases for each upgrade line.
- Checks total deductions after the purchases.
- Checks the final value for each upgrade line.
- Does not check each intermediate tier value.
- Checks the tier limit.
- Checks repeatable scaled Hub repair.

## Automated validation

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The `stalberg_horde_match_tests` target checks:

- Recipe binding and costs.
- Automatic countdown and intermission transitions.
- Puzzle-independent advancement.
- Scaling snapshots, bounded schedules, and overflow safety.
- Objective sites and progression-currency reservation.
- Atomic Anchor interaction and configured rewards.
- Gate state and Reward-route safety.
- Geometry-safe spawning, separation, and navigation.
- Concurrent Anchor and Hub progression.
- Relay order and explicit extraction.
- Upgrades, Hub repair, and full reset.

The generation, geometry, generated-level, collision, combat, and F1 regression tests remain active.

`stalberg_match_generation_tests` checks:

- Different-seed variation.
- Exact same-seed replay of the accepted layout, profile, and retry.
- Same-map restart.
- Bounded deterministic fallback.
- Fortress V1 actor-relative geometry and ingress targets.
- Failure of radius-only growth.

Production admission status:

- Includes progression-stage checks.
- Includes the static post-gate spawn-packing check.
- The static post-gate spawn-packing check requires 18 slots in at least two rooms for each checked post-gate state.
- The static post-gate spawn-packing check does not run the runtime spawn system.
- The static post-gate spawn-packing check does not prove runtime occupancy recovery, visibility, or role suitability.

Current production audit:

- Production maps require exactly two useful cycles.
- Each Shortcut must save at least two arena transitions.
- The audit used 100 requested match seeds.
- Six requests used the fallback.
- The other 94 accepted radius-8 maps had a mean multi-entry substantial-room ratio of 66.7%.

Next acceptance work:

- Bound Combat leaves and dead ends.
- Check physical route separation.
- Add room and combat grammar.
- Publish typed semantic anchors.
- Compile the full quest recipes.
- Add initial-lock, visibility, and role spawn checks.
- Add opening-component circulation and economy checks.
- Add bounded runtime recovery.
- Add endurance and navigation validation.
