# Game Handoff

This file uses ASD-STE100 Simplified Technical English for its prose.

This file is the continuation guide for the `stalberg_game` runtime.
The previous milestone sequence is not active.
Read [`level-generation-lock-in.md`](level-generation-lock-in.md) for the active roadmap and acceptance gates.
Use the generator documents for current procedural-level contracts.
[`level-identity-pass.md`](level-identity-pass.md) gives historical information about the first identity pass.

## Planning reset — generation and quests only

The first topology-label pass did not meet the circulation targets.
It did not implement room-shape and combat grammar.
The later circulation pass improved the result.
Each accepted production radius-8 map now has exactly two useful cycles.
A useful cycle is a non-Primary mission edge whose alternate path has at least 3 arena transitions in the connector-contracted arena graph.
Each production Shortcut now saves at least two arena transitions.
Progression-stage checks are implemented.
The static post-gate spawn-packing check is implemented.

The lock-in is not complete.
Some maps still have too many ordinary Combat leaves.
Routine dead-end depth does not have a hard limit.
Useful-cycle checks do not measure physical route separation.
The room-shape and combat grammar still uses one main growth rule.

Do not add bosses, more enemy roles, more economy systems, meta-progression, controller work, or unrelated polish before the lock-in ends.
Use this work order:

1. Bound ordinary Combat leaves and routine dead-end depth.
2. Measure physical route separation.
3. Generate distinct briefs for the room-shape and combat grammar.
4. Publish semantic anchors and compile one main quest and one optional discovery path.
5. Add initial-lock spawn, visibility-band, and spawn-role compatibility checks.
6. Add bounded runtime spawn recovery.
7. Complete endurance and navigation tests.
8. Replace standard-library generation randomness with a portable project RNG.
9. Review seeds that fail a named acceptance check and consecutive request seeds in F2 and normal play.

The five large-map archetypes and `TopologySignature` are construction inputs and diagnostics.
They do not prove the remaining circulation requirements.
They do not prove the room-shape and combat grammar requirements.
They do not prove the quest and endurance requirements.
Candidate acceptance must use the gates in [`level-generation-lock-in.md`](level-generation-lock-in.md).

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

## Product destination

The small-map horde **systems** vertical slice is complete.
The automatic endless-round rework is also complete.
Radius-5 shooter generation selects Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing.
It then publishes Start, Hub, Anchor-capable Combat, Reward, and Exit structure.

Round 1 starts after a three-second countdown.
Each cleared round starts a five-second intermission.
The next round starts without player input.
The round index uses overflow-safe 64-bit state.

The match has point income and permanent exact-threshold gates.
It has three tiers for damage, fire-rate, and dash upgrades.
It has repeatable repair at a powered Hub.
It has map-wide Drifter, Runner, Caster, and Elite pressure.
It has an Anchor holdout, Hub activation, an optional ordered relay puzzle, and voluntary Exit extraction.
These systems keep their state during the match.

Hub Circuit has one branch for each semantic room.
Small-map validation requires a normalized direction dot product of `0.42F` or less between the Start and Anchor transitions from the Hub.
The F2 overview and command-line recipe option show each fixed slice.

The product is an **automatically advancing, endless round-based horde shooter**.
Each new match uses one fresh procedural map.
The accepted map stays active for the run.
The displayed match seed reproduces the map in the same build and toolchain.
`R` keeps the seed.
`N` requests a different seed.
Difficulty is deterministic for the accepted map and round index.
Puzzle, gate, Anchor, and Hub state do not pause or authorize the director.
Extraction is voluntary.
Successful extraction sets victory.
Victory stops later director updates.

The procedural map is not a disposable floor in a multi-floor run.
Fixed curated layouts are not the normal content model.
Each new match creates a `MatchGenerationRequest`.
The request selects the physical profile and generator attempt limits.
The physical profile defaults to `FortressV1`.
Each generator attempt derives only its grid and room seeds from `matchSeed`.
The selected archetype and quest-recipe identity stay fixed across geometry retries.
`MatchGenerationRequest::attemptBudget` sets the generator-attempt limit.
Its default value is eight.
The match generator returns only a validated generated result or the validated Systems fallback.
Curated seeds remain regression fixtures and an emergency fallback.
Authored recipes must describe semantic relationships and requirements.
They must not use fixed world coordinates or room IDs.

## Current behavior

Run:

```sh
cmake -S . -B build
cmake --build build -j
./build/stalberg_game
./build/stalberg_game --seed=123456789  # replay a generated match
./build/stalberg_game --recipe=hub      # fixed regression fixture
```

Controls:

| Input | Action |
|---|---|
| WASD | Camera-relative movement |
| Space | Dash in the movement direction, or facing direction while stationary |
| Mouse | Aim on the XZ ground plane |
| Hold left mouse button | Fire anywhere on the generated map or in the regression arena |
| E | Buy a gate, activate Anchor/Hub/Exit, or repair one missing health at the powered Hub; at Anchor, fund-and-start atomically when affordable |
| 1/2/3 at Hub | Buy the next damage, fire-rate, or dash tier |
| R | Restart mutable match state on the same finalized generated map, or restart regression combat |
| N | Generate and enter a fresh match with a new seed |
| F1 | Toggle generated horde match / combat regression arena |
| F2 | Toggle the generated full-level developer overview |
| F3 | Toggle rendering/gameplay diagnostics |
| Left/Right in overview | Browse the fixed representative configurations read-only |
| Home in overview | Return to the active generated layout |
| Escape/window close | Exit |

The runtime creates a `MatchGenerationRequest` at startup.
It puts a fresh public match seed in the request.
The request uses its default `FortressV1` physical profile.
Each generator attempt derives radius-8 grid and room seeds from `matchSeed`.
One versioned `GenerationBrief` and stable brief hash apply to the request.
The brief keeps the archetype and quest-recipe identity constant across retries.

The application-level generator evaluates up to `MatchGenerationRequest::attemptBudget` attempts.
The default `attemptBudget` is eight.
It rejects candidates that fail the systems plan, Fortress V1 physical gates, or progression stages.
It also applies the static post-gate spawn-packing check.
It ranks valid candidates by circulation, physical margin, progression, and generator quality.
After the first valid candidate, the default `rankingAttemptBudget` permits one more generator attempt.
The quality margin and valid-candidate limit can stop generation earlier.
The generator keeps the valid candidate with the highest score from the attempts that it evaluates.
It shows the known radius-5 Hub Circuit Systems fallback after attempt exhaustion.

`--seed=<unsigned decimal>` sets `matchSeed`.
The same request profile and generator settings reproduce the seeds, brief data, attempt data, and geometry in the same build and toolchain.
`N` requests another seed.
`R` resets only mutable state.
`--recipe=hub|ring|wings` starts a fixed diagnostic fixture.
Do not use `--recipe` with `--seed`.

`GeneratedLevel` first contains candidate geometry and candidate generation metadata.
It keeps the relaxed grid and exact dual geometry.
It keeps the neutral room graph and shooter layout.
It keeps the exact floor, walls, and doorway thresholds.
It also keeps immutable navigation.
After candidate selection, `MatchGenerator` calls `finalizeMatchGeneration()`.
This call replaces only the generation metadata with final selection metadata.
This call occurs before `MatchGenerator` returns the level to gameplay.
After finalization, gameplay treats the level data as immutable.
Gameplay then uses `GeneratedLevel` as read-only data.
Large layouts select Hub and Spokes, Ring and Branches, Main Spine, Twin Districts, or Dense Core/Sparse Branch.
Each production graph publishes one typed Cycle edge and one typed Shortcut edge.
The physical layout realizes exactly two useful cycles.
The production Shortcut saves at least two arena transitions.
Large layouts publish one high-degree Hub.
They prefer a leaf arena for Reward semantics.

`LevelSession` owns the authoritative player and current room.
It owns dynamic doorway collision and matching traversal state.
`HordeMatch` owns points, permanent gate purchases, upgrades, and persistent player attack state.
It owns the deterministic round schedule, map-wide enemies, and hostile projectiles.
It also owns Anchor/Hub/relay/Exit state and whole-match reset.

Progression-stage simulation checks Expansion, Anchor, Exit, and Reward states.
It checks gate approach, objective reachability, newly reachable floor, and route savings.
Deterministic gate and Anchor binding tests alternate doorway and Combat-room choices.
The binding search keeps the first binding that passes the complete stage simulation.
The static post-gate spawn-packing check evaluates each simulated post-gate Fortress state.
Each checked state must have 18 packed spawn slots in at least two rooms.
This validation does not prove runtime spawn placement.
Runtime placement also uses player distance and active walls.
It also uses occupancy and a larger separation distance.

Room-shape and combat grammar is not implemented.
Typed semantic anchors and the quest compiler are not implemented.
Initial-lock spawn checks are not implemented.
Visibility-band and spawn-role compatibility checks are not implemented.
Runtime spawn recovery and full large-map endurance tests are not implemented.

Round 1 gives sufficient points for the recipe-scaled first gate.
Round 2 gives sufficient points for the Anchor route.
Optional spending stays disabled until the player can fund the required Anchor route.
Drifters and Runners use the open exact cell graph.
Casters and Elites use difficulty-scaled ranged fan patterns.
Local separation prevents complete crowd overlap.

E resolves gate and device interactions.
The gate HUD prompt and purchase query use the same 2.2-world-unit range.
At the Anchor, E can fund a closed Anchor gate and start the holdout in one operation.
Rounds continue during incomplete Anchor and Hub work.

The Hub sells three tiers for each authoritative upgrade.
Use 1/2/3 to buy these upgrades.
The Hub repairs one missing health for each E interaction after activation.
Repair cost scales with pressure.
The relay grants the next fire-rate tier.
Locked gates use one connected barred frame.

The F2 overview draws exact floor triangles and the recipe graph.
It shows live lock state, objective sites, and relay order.
It shows match, grid, and room seeds.
It shows generator attempts, profile, candidate index, and quality score.
It shows the published topology summary.
It does not show rejection records or typed mission-edge data.
Preview browsing does not change the active match.

`F1` opens the hard-coded 20-by-20 combat regression arena.
This path has the stationary target and deterministic hostile orb.
It has separate projectile pools and swept wall and damage collision.
It has defeat and victory freeze, restart, effects, HUD, and procedural tones.

## Runtime architecture

```text
main.cpp
    ├── requests finalized read-only GeneratedLevel data through MatchGenerator
    ├── reads PlayerInput through game_input
    ├── advances generated room progression or combat-regression fixed state
    ├── interpolates simulation state for rendering
    └── asks GameRenderer to draw

GeneratedLevel
    ├── retains StalbergGrid + DualGrid + RoomGrid + RoomLayout
    ├── triangulates every assigned exact dual polygon
    ├── retains walls plus exact published doorway threshold segments
    └── publishes Start spawn + immutable door-aware navigation

LevelSession
    ├── owns player + current room + lifecycle states
    ├── locks/unlocks doorway collision and traversal together
    ├── publishes active walls for simulation and rendering
    └── performs generated traversal fixed updates and reset

HordeMatch
    ├── binds objective sites and economy gates from the selected recipe
    ├── owns points, upgrades, persistent attacks, and hostile projectiles
    ├── owns the automatic endless director and bounded difficulty profile
    ├── routes Drifter/Runner/Caster/Elite enemies through opened cells
    ├── drives Anchor → Hub → Exit plus the optional ordered relay reward
    └── resets the complete persistent match deterministically

PlayerInput + active wall span ──→ updateCombat() ──→ caller-owned Player ──→ wall resolution
      │                 │                └── health/invulnerability/death
      │                 ├──→ Weapon + player ProjectilePool ──→ Target + Enemy
      │                 ├──→ Enemy movement + fan pattern
      │                 ├──→ enemy ProjectilePool ──→ Player damage
      │                 └──→ deterministic terminal-state freeze
      └── populated only by game_input

Encounter ── wraps CombatState + regression-owned Player + Target + restart

Player ──→ updateGameCamera() ──→ Camera3D
Camera3D + keyboard/mouse ──→ readPlayerInput()

Camera3D + GeneratedLevel + interpolated player ──→ GameRenderer
Camera3D + Arena + interpolated combat state ──→ GameRenderer
CombatStepResult / EncounterStepResult ──→ CombatAudio
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, generated/combat view switching, 120 Hz accumulator, interpolation, audio-event forwarding, and composition |
| `src/game/match_generator.hpp/.cpp` | Request-selected profile, match-seed inputs, brief selection, generator attempts, admission, ranking, metadata, and fallback |
| `src/game/match_map_metrics.hpp/.cpp` | Physical, progression-stage, route-savings, and static post-gate spawn-packing check measurements |
| `src/game/generated_level.hpp/.cpp` | Candidate artifacts, metadata finalization, exact floor triangles, walls, doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.hpp/.cpp` | Mutable traversal, authoritative player, room state, doorway locks, active walls, effects, and reset |
| `src/game/horde_match.hpp/.cpp` | Recipe binding, endless director, difficulty, economy, horde combat, objectives, extraction, and reset |
| `src/game/generated_encounter.hpp/.cpp` | Preserved generated-room regression coordinator and spawn-selection coverage; not the active generated runtime path |
| `src/game/enemy_collection.hpp/.cpp` | Stable enemy identities and order, movement, fire, earliest swept-hit selection, and all-defeated queries |
| `src/game/combat.hpp/.cpp` | Player attack state, regression update order, and injected wall geometry |
| `src/game/encounter.hpp/.cpp` | Hard-coded regression wrapper, optional target integration, and deterministic reset |
| `src/game/enemy.hpp/.cpp` | Deterministic orbit movement, fan cadence, health, damage, and hostile projectile profile |
| `src/game/combat_audio.hpp/.cpp` | Audio-device ownership and generated combat tones |
| `src/game/arena.hpp/.cpp` | Immutable segments, circle collision and sliding, and projectile-wall resolution |
| `src/game/collision_2d.hpp/.cpp` | Closest-point, swept-circle, segment, earliest-hit, and closed-loop containment queries |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, dash, facing, health, damage, invulnerability, and interpolation |
| `src/game/weapon.hpp/.cpp` | Fire cadence and muzzle spawning |
| `src/game/projectile_pool.hpp/.cpp` | Profile-based projectile slots, movement, lifetime, reuse, and interpolation |
| `src/game/target.hpp/.cpp` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.hpp/.cpp` | Camera creation, follow, movement basis, ground projection, and interpolation |
| `src/game/game_input.hpp/.cpp` | raylib keyboard and mouse polling |
| `src/game/game_renderer*` | Runtime scene, role-distinct entities, telegraphs, HUD, landmarks, and full-level overview |
| `src/game/render_resources.hpp/.cpp` | Model and mesh ownership, floor detail, projectile glow, and UI-font loading |
| `src/game/render_style.hpp/.cpp` | World/HUD palette, 1280-by-800 UI canvas, role colors, and HUD panels |
| `src/game/directional_shader.hpp` | Embedded GLSL, floor and wall treatment, and directional-light vector |
| `src/game/post_process_*` | Ambient grounding, edge treatment, bloom, tone mapping, FXAA, and screen effects |
| `tests/generated_level_tests.cpp` | Artifact, geometry, doorway, traversal, spawn, combat-transition, cleanup, reset, and reachability coverage |
| `tests/game_tests.cpp` | Dash, collision, combat, target, projectile, enemy, damage, containment, interpolation, victory, and restart coverage |
| `tests/horde_match_tests.cpp` | Director, difficulty, recipes, economy, gates, spawning, objectives, extraction, upgrades, and reset coverage |
| `tests/match_generation_tests.cpp` | Replay, variation, rejection, fallback, physical gates, progression stages, static post-gate spawn-packing check, and radius-only failure |
| `CMakeLists.txt` | Runtime/test sources, raylib linkage, warnings, and Debug runtime optimization |

The renderer destructor unloads models before it unloads the shared lighting shader.
`CombatAudio` unloads sounds before it closes the audio device.
Both presentation owners must end before `CloseWindow()`.
For this reason, `main.cpp` puts them in an inner scope.

## Technical decisions and invariants

### Coordinate model

```text
world X/Z = authoritative gameplay plane
world Y   = visual height
```

The sphere center stays at `PLAYER_RADIUS` above `Y = 0`.
Aim points are ray intersections with the infinite `Y = 0` plane.

### Camera and controls

The camera uses orthographic projection.
It has a 45-degree elevation and a diagonal heading.
The first Fortress V1 framing pass uses a 25-world-unit base orthographic size.
It uses a 19.5-unit height, 14-unit diagonal offset, and 2.5-unit facing look-ahead.
Viewports wider than 1.9:1 reduce the vertical orthographic size.
This limits the additional world area on ultrawide windows.

Movement uses the planar forward and right vectors of the camera.
Do not restore a fixed isometric input matrix.
The camera was tuned separately from the 1.375× geometry-scale increase.
Locomotion and combat range still need playtests.

### Timing

Simulation uses fixed `1/120` second steps.
Rendering interpolates player, enemy, camera, and projectile state.
Put new gameplay behavior in the fixed update loop.
Do not put dash, cooldown, projectile, collision, or enemy logic in the render path.

The player dash moves at 18 world units/second for 0.18 seconds.
The activation cooldown is 0.8 seconds.
Dash uses camera-relative movement direction.
It uses aim-facing direction when the player is stationary.
Dash does not give invulnerability.
It uses standard circle-versus-wall collision and sliding.
Space is an edge input.
The game keeps it latched until a fixed step uses it.

The game limits frame time to 50 ms before accumulator input.
This prevents an unbounded catch-up after a pause or debugger stop.

### Projectile contract

Projectile state uses `Vector2` X/Z coordinates.
Only the renderer converts this state to `Vector3`.
Each `ProjectilePool` owns one read-only `ProjectileProfile`.
Player and enemy pools can use different profiles.
They do not need ownership flags or per-projectile configuration.

Spawning requires finite positive speed and lifetime.
It requires a finite nonnegative radius.
Each pool has 192 stable slots.
The pool scans from the start for the first inactive slot.
A full pool drops the shot.
The weapon still uses its cooldown.
This rule prevents a burst when a slot becomes available.

The base projectile profiles are:

| Constant | Player | Enemy |
|---|---:|---:|
| Fire interval | 0.1 seconds | 1.15 seconds |
| Projectile speed | 22 world units/second | 6.5 world units/second |
| Projectile lifetime | 1.8 seconds | 3.4 seconds |
| Projectile radius | 0.16 world units | 0.22 world units |

The player muzzle is 1.3 world units from the player center.
The enemy fires in three directions.
The directions are 0 and ±14 degrees from the current player-facing direction.
The F1 regression uses the base enemy values.
`HordeMatch` rebuilds its hostile pool at round start.
It scales hostile projectile speed from 6.5 to 9.1 units/second.
It scales ranged-enemy fire interval from 1.15 to 0.7475 seconds.
Projectile lifetime and radius do not change.

Each fixed projectile update copies `position` to `previousPosition` before movement.
`updateCombat()` keeps the regression update order.
The generated runtime uses `HordeMatch`.
`LevelSession` moves the player first.
Persistent player attacks then move against active walls.
Relay and enemy swept hits then resolve.
Horde movement and ranged patterns then run.
Hostile shots then resolve.
Contact and projectile damage use the same invulnerability state.
Player attacks continue across rounds.
Hostile shots remain active during `Cleanup`.
They clear when `Cleanup` changes to `Intermission`.
Whole-match reset also clears them.

Wall-aware weapon and enemy-pattern overloads reject blocked muzzle paths.
They still use the cooldown.
A new projectile has a valid muzzle-to-first-step segment.
Keep this segment.
Resolve wall collision before enemy, target, or player collision.
Do not replace swept collision with a current-position overlap test.

### Target contract

The target position is X/Z `(-5, -5)`.
Its radius is `0.85`.
It has five health, a `0.16` second hit flash, and a one-second reset delay.
`updateTarget()` uses the shared swept circle-versus-circle query for each active projectile.
The query uses the projectile radius and target radius.
It supports zero-length sweeps and initial overlaps.

A hit deactivates the projectile immediately.
It removes one target health.
Processing stops when a hit defeats the target.
Later slots stay active in that fixed step.
A defeated target does not collide.
It resets at the same position with full health after the delay.
`tests/game_tests.cpp` has headless target-collision coverage.
It verifies a target hit during the final valid projectile lifetime step.
It also verifies target reset during encounter restart.

### Enemy and player-combat contract

The regression enemy starts at X/Z `(5, 5)`.
It has 20 health and moves at 2.4 units/second.
It circles counter-clockwise relative to the player.
It applies a limited radial correction toward a five-unit preferred distance.
Its state has no random source.
Identical fixed-step inputs must give identical movement and shots.

Player-hit tests use projectile movement relative to previous and current enemy positions.
They include both radii.
A hit uses the projectile and removes one health.
The regression encounter freezes at victory when enemy health reaches zero.

The preserved generated-encounter regression keeps source-cell IDs and earliest-hit tie breaking.
Active horde enemies get increasing match spawn IDs.
They get role-specific health, speed, and reward values.
They use exact door-aware pursuit and deterministic local separation.
A player projectile damages a maximum of one enemy.
Hit and death transitions award points one time.

The player has five health.
Hostile collision uses projectile movement relative to previous and current player positions.
It includes both radii.
Contact deactivates a hostile shot during invulnerability.
A valid hit applies the current bounded horde-damage value.
It then starts 0.8 seconds of invulnerability.
Horde damage is one through pressure tier 7.
It is two after pressure tier 7.
The F1 regression damage stays at one.
The encounter freezes at defeat when player health reaches zero.
A post-victory or post-defeat `restartPressed` restores the owner.
It also clears the projectile pools.

### Player ownership boundary

Two runtime views own separate players.
`LevelSession` owns the generated-match player.
The regression `Encounter` owns the arena player.
These views cannot run at the same time.

`HordeMatch` passes `LevelSession::player()` through movement, economy, puzzle, and horde combat.
It owns the separate persistent attack state.
Regression `CombatState` stays in the caller-owned single-enemy F1 path.
Do not copy or synchronize parallel generated players.

### Arena contract

`ARENA_WALLS` has four ordered X/Z segments.
They make a square from `-10` to `10` on each axis.
Reusable `updateCombat()` requires an injected wall span.
`updateEncounter()` keeps its injected-wall overload.
Its three-argument regression overload uses `ARENA_WALLS`.

Player collision treats the player as a `PLAYER_RADIUS` circle.
It resolves contacts with four fixed passes.
It removes only velocity into each contact normal.
Tangential movement stays active.
The position before movement selects the stable side of wall-face contacts.

Projectile collision uses shared `collision_2d` queries.
Each projectile is a moving circle.
The query checks its full previous-to-current path against segment faces and endpoint circles.
It uses the pool profile radius.
It selects the earliest wall hit.
The arena resolver clips the position to the contact and deactivates the projectile.
It also deactivates projectile centers outside the closed convex wall loop.
Wall meshes extend outward from the ordered segments.
The inner mesh face aligns with the simulation segment.

### Input boundary

`updatePlayer()`, `updateCombat()`, and `updateEncounter()` must not call input APIs.
These APIs include `IsKeyDown()` and `GetMousePosition()`.
Extend `PlayerInput` for new input state.
Then populate it in `readPlayerInput()`.
Restart and dash are edge inputs.
`main.cpp` keeps each input latched until a fixed step uses it.
This design keeps simulation code testable.
It also permits future controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model`, `Shader`, `Font`, or `Sound` handles.
`GameRenderer` and its rendering modules own graphics resources.
`CombatAudio` owns the audio device and generated sounds.
Projectiles publish stable simulation state to presentation modules.
They do not issue draw or audio calls.

HUD and overview drawing use a centered 1280-by-800 virtual canvas.
The renderer scales it uniformly to the framebuffer.
Position HUD items against `UI_CANVAS_WIDTH` and `UI_CANVAS_HEIGHT`.
Do not use native window dimensions for HUD placement.
Text uses ComicShannsMono Nerd Font Mono from `RenderResources`.
CMake copies the font beside native builds.
It preloads the font in the web virtual file system.
The Boost bar uses a full cyan fill to show readiness.
It does not use a second text label.

The web build fixes the raylib framebuffer at 1280 by 800.
`web/shell.html` scales the 16:10 canvas uniformly in the viewport.
Do not enable `FLAG_WINDOW_RESIZABLE` on the web build.
That flag lets raylib use the browser aspect ratio for the framebuffer.
CSS then letterboxes the canvas and stretches the image.
It also makes GLFW mouse coordinates differ from `GetScreenWidth()` and `GetScreenHeight()`.

### Procedural generation boundary

The game target links generator libraries only through `GeneratedLevel`.
The package keeps `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together.
`RoomLayout` alone does not contain all exact floor polygons and doorway segments.
The package also keeps one `DoorwayThreshold` segment for each published doorway.
Geometry and navigation are read-only after construction.
`finalizeMatchGeneration()` can replace generation metadata after candidate selection.
`MatchGenerator` calls it before gameplay receives the selected level.
Gameplay does not mutate `GeneratedLevel`.

Neighboring assigned cells in one room are traversable.
Cross-room navigation uses only cell pairs from `RoomLayout::getDoorways()`.
Physical contact between regions does not create traversal.
`LevelSession` owns mutable lock state.
It adds locked threshold segments to collision walls and traversal checks.
Connected exterior entrance cells stay enclosed.

The powered-Exit requirement is hard-coded to Round 5.
The extraction interaction is explicit and voluntary.
Successful extraction sets `victory`.
`updateHordeMatch()` then returns before it updates the round director.
Later updates also return before director work while `victory` stays set.
Replace this requirement with recipe-authored quest metadata.

`MatchGenerationRequest` selects the physical profile.
The default profile is `FortressV1`.
This profile uses radius 8 and `GeneratedLevelConfig::worldScale = 0.22F`.
Each generator attempt derives its grid and room seeds from the 64-bit `matchSeed`.
`MatchGenerator` evaluates up to the configured `attemptBudget`.
The default `attemptBudget` is eight.
It records the selected profile and generation brief.
It records attempts performed and the selected attempt.
It records the valid-candidate count and score.
It also records rejection data.
It uses the radius-5 `0.16F` Hub Circuit fixture only after production generation fails.

Systems-plan validation requires Start, Hub, Anchor-capable Combat, Reward, and Exit rooms.
It requires three relay targets.
It requires Expansion, Anchor, Reward, and Exit gates.
Physical validation checks player-relative geometry and map-wide capacity.
Progression validation checks each simulated gate state.
The static post-gate spawn-packing check evaluates each simulated post-gate state.

A radius-8 layout with `worldScale = 0.16F` fails.
`--seed` supplies the public match seed.
The fixed Hub Circuit, Broken Ring, and Twin Wings presets use room seeds 7, 2, and 3.
`--recipe=hub|ring|wings` and the six F2 previews are deterministic regression tools.

Replay determinism applies to the same game build and toolchain.
Lower-level grid and room generation use standard-library shuffle and distribution implementations.
A different C++ standard library can produce a different result.
Portable replay requires project-owned random algorithms and a generation-version contract.

## Completed implementation slice: automatic endless rounds and scaling

The automatic director and bounded pressure profile are implemented.
Fixed-step simulation and generated-level ownership did not change.

### 1. Decouple and automate the round director — complete

- The implementation removed `HORDE_FINAL_ROUND`.
- It removed `PlayerInput::startRoundPressed`, queued round-start input, and the deploy-next-wave prompt.
- `KEY_N` no longer starts a round.
- `KEY_N` still requests a fresh match.
- Round 1 starts after a three-second countdown.
- Cleanup starts a five-second intermission.
- The next round starts automatically.
- `Intermission`, `Buildup`, `Peak`, and `Cleanup` stay as director-owned phases.
- Puzzle, gate, Hub, and Anchor state cannot block these phases.
- The round index uses `std::uint64_t`.
- Schedule and profile arithmetic clamps before multiplication.
- This rule also applies at the maximum round value.
- The HUD shows the round and intermission countdown.
- Hostile projectiles remain active during `Cleanup`.
- They clear at the `Cleanup`-to-`Intermission` transition.
- Whole-match reset also clears them.
- Player attacks, purchases, upgrades, and puzzle state stay active.

### 2. Add deterministic difficulty scaling — complete

One reproducible profile uses the round index and map recipe:

1. Spawn budget grows from 6 to 48.
2. Simultaneous living pressure grows from 6 to 18.
3. Spawn pacing becomes shorter within fixed limits.
4. Higher pressure replaces Drifters with Runners, Casters, and a maximum of three Elites.
5. Deterministic role order and increasing spawn IDs vary role and ingress order by round and recipe.
6. Health, movement, hostile projectile speed, fire cadence, damage, and rewards use ten bounded tiers.
7. Each fifth round schedules an Elite event.
8. An Elite event does not make a terminal round.

Health has a maximum scale of 1.8×.
Movement has a maximum scale of 1.25×.
Hostile projectile speed has a maximum scale of 1.4×.
Fire interval has a minimum scale of 0.65×.
Damage has a maximum value of two.
Reward income has a maximum scale of 1.5×.
Spawn budget and simultaneous population reach their limits at Round 25.
Attribute and role changes reach the final pressure tier at Round 51.
Five-round Elite events continue after Round 51.

Recipe-scaled gate costs protect the opening economy.
Three upgrade tiers follow the bounded threat curve.
Powered-Hub repair is a repeatable resource sink after the pressure cap.
Bosses and mutation events are future work.

## Generation foundation status and superseded plan notes

The new-match boundary and Fortress V1 scale are complete.
The cycle requirements and progression stages are complete.
The static post-gate spawn-packing check is complete.
Next, bound circulation and implement room-shape and combat grammar.
Then implement semantic anchors and quests.
After that, complete the remaining spawn checks, runtime recovery, endurance tests, and map-quality gate.

### 3. Add the new-match generation boundary — complete

- Normal play creates one 64-bit match seed.
- Each generator attempt derives its grid and room seeds from this seed.
- `MatchGenerationRequest` selects the physical profile.
- The profile defaults to `FortressV1`.
- `N` requests a new match.
- `R` resets the current match on the same accepted map.
- `--seed=<unsigned decimal>` replays a match.
- HUD shows profile, seed, and fallback status.
- F2 also shows generator attempts.
- `MatchGenerationRequest::attemptBudget` sets the generator-attempt limit.
- The default `attemptBudget` is eight.
- The generator selects the Hub Circuit Systems fallback after attempt exhaustion.
- HUD and F2 show fallback use.
- Fixed configurations stay in tests and F2 previews.
- Headless tests cover replay, variation, reset, fallback, profile selection, and fixed-brief identity.

The boundary does not depend on the old systems dimensions.
The selected quest-recipe identity is fixed across retries.
The full semantic-anchor quest compiler is not implemented.

### 4. Increase physical map scale, not only grid radius — Fortress V1 complete

- Normal maps use radius 8 and `worldScale = 0.22F`.
- Player and enemy collision bodies do not change.
- Systems fixtures and fallback use radius 5 and `0.16F`.
- `PhysicalMapProfile` stays in match metadata.
- HUD and F2 show the profile.
- `MatchMapMetrics` measures doorway width, room area, objective clearance, room span, route distance, ingress separation, spawn capacity, and Hub degree.
- Room span is a room-size proxy.
- It is not a line-of-sight test.
- Fortress V1 rejects candidates below its limits.
- It rejects radius 8 at the systems world scale.
- The camera uses an independent 25-unit orthographic view.
- Actor size and combat distance did not receive a global scale change.
- Headless tests cover profile replay, metric limits, semantic roles, and radius-only rejection.

Fortress V1 acceptance requires:

| Metric | Minimum |
|---|---:|
| Doorway width | 3.5 player diameters |
| Every substantial-room area | 270 player-diameter squares |
| Anchor-room area | 300 player-diameter squares |
| Hub/Anchor/Exit local clearance | 2.25 player diameters |
| Anchor-room center span | 18 player diameters |
| Start-to-Exit traversable route | 135 player diameters |
| Maximum usable cross-room ingress separation | 130 player diameters |
| Statically usable enemy spawn candidates | 220 |
| Rooms with statically usable spawn candidates | 12 |
| Hub published doorway degree | 3 |

Static spawn usability requires reachability in the immutable all-open graph.
It requires enemy-size source clearance and no immutable-wall overlap.
It does not prove initial-lock access.
It does not check distance from the current player.
It does not check occupancy at a spawn step.

Progression-stage acceptance also requires approachable gates and reachable objectives.
A stage must add reachable floor or save at least two cell transitions.
Each checked post-gate Fortress stage must have 18 statically packed spawn slots in at least two rooms.
This result does not prove runtime spawn placement.
Runtime placement also uses player distance and active walls.
It also uses occupancy and a larger separation distance.

The remaining physical work includes connector dimensions and true sightline bands.
It includes initial-lock capacity and physical route separation.
It includes traversal time, movement, dash, projectile reach, enemy visibility, and interaction range.
It also includes lighting, shadows, floor-detail density, and navigation performance.

### 5. Bind concurrent recipe-authored quests

Puzzle logic must be an independent persistent state machine.
A step can specify `minimumRound`, enemy role, kill, currency, room, or powered-device requirements.
It must not control round transitions.

- Round requirements must unlock puzzle actions.
- They must not keep an intermission open.
- Incomplete puzzle steps must continue across rounds.
- Combat steps must define whether progress continues, pauses, or resets.
- The game must show this rule before activation.
- Anchor holdouts, relays, clues, and devices must work while rounds advance.
- Quest completion must unlock extraction, a boss, a major reward, or a pressure tier.
- Quest completion must not stop spawning without a visible rule.
- Run extraction must require explicit player input.
- Otherwise, the match must continue until defeat.

Replace the hard-coded Round 2 Anchor and Round 5 Exit checks with recipe-authored metadata.
The puzzle model must use semantic room and device anchors.
It must not use world coordinates or a fixed five-round schedule.

### 6. Rework generation for sustained endless play

Fortress V1 provides large random spaces.
It now provides two useful cycles and safe checked progression stages.
It does not pass all endurance, room-variation, and runtime spawn requirements.

Generation and scoring still need these properties:

- Bounded ordinary Combat leaves.
- Limited routine dead-end depth.
- Physical separation of alternate routes.
- Room-shape and combat grammar for compact, elongated, concave, split, and multi-entrance briefs.
- Objective sites that preserve required circulation and enemy ingress.
- Quest dependencies across useful route choices.
- Initial-lock spawn capacity.
- Visibility bands and spawn-role compatibility.
- Bounded recovery from temporary runtime spawn failure.
- Population capacity, sightline variety, ranged-enemy positions, and late-round navigation limits.
- Economy pacing for automatic rounds.
- A viable opening component before the first gate purchase.

Keep the three small recipes as deterministic regression fixtures.
Normal generation requests a Fortress V1 layout.
It can return the Systems fallback after attempt exhaustion.
Current gates prove map-wide capacity and safe checked progression.
They also prove that checked post-gate states pass the static post-gate spawn-packing check.
They do not prove runtime spawn placement.
Future gates must prove the initial lock state, runtime recovery, quest realization, endurance, and physical route separation.
Curated large layouts can support balance work.
They must not replace random normal play.

### 7. Acceptance and test coverage

Headless tests cover automatic Round 1 startup.
They cover cleanup, intermission, and next-round transitions.
They cover input and puzzle independence.
They cover deterministic recipe schedules and rounds 1, 5, 10, 25, and 100.
They cover bounded pressure and maximum-round arithmetic.
They cover Anchor progress, extraction, upgrades, rewards, and reset.

Match-generation tests cover cross-seed input variation and same-seed replay.
They cover fixed generation briefs across retries.
They cover restart input that preserves the accepted map.
They do not cover `R` or `N` keyboard wiring.
They cover fallback, structured rejection data, and best-valid ranking.
They cover Fortress V1 physical limits and radius-only rejection.
They cover progression stages and the static post-gate spawn-packing check.

Room-generation tests assert exactly two useful cycles for production archetypes.
They also assert that each production Shortcut saves at least two arena transitions.

Add future coverage for these items:

- Bound ordinary Combat leaves and dead-end depth.
- Check physical route separation.
- Reproduce room briefs, semantic anchors, and quest placement.
- Check recipe-authored round requirements without director blocking.
- Check quest completion with continued spawning before extraction.
- Check initial-lock capacity, visibility bands, and spawn-role compatibility.
- Check runtime spawn recovery.
- Run endurance and navigation tests at representative pressure levels.
- Reproduce seeds with the portable project RNG.

[`small-puzzle-horde-slice.md`](small-puzzle-horde-slice.md) is the acceptance guide for fixed systems fixtures.
A later guide must cover quest binding and complete Fortress V1 pacing.

## Intended horde-mode boundary

The game finalizes `GeneratedLevel` before gameplay.
Gameplay then treats it as read-only data.
The game puts persistent match state above it:

```text
GeneratedLevel
    └── replayable random map, physical-scale profile, room roles, Start/Exit sites, typed quest anchors, doorway thresholds, navigation, spawn candidates

Horde match state
    ├── authoritative LevelSession player and active collision walls
    ├── endless automatic director: countdown → buildup → peak → cleanup → timed intermission → next round
    ├── round-indexed difficulty profile + deterministic spawn schedule
    ├── stable enemy collection with bounded active-population pressure
    ├── map progression: currency, gates, services, traps, and powered rooms
    ├── concurrent quest state: requirements, Grid Anchors, clues, secrets, and extraction unlock
    └── terminal state: death or explicit player-chosen extraction
```

The vertical slice maps gates and objectives to exact doorway lock bits.
Gate purchase and Hub activation update rendering, collision, player traversal, and enemy navigation together.
Waves use the current open component.
They do not use separate room encounters.
The director advances while required routes stay closed.
For this reason, the initial component must support early ingress, circulation, and economy.
A future lock model can replace the Boolean lock.
It can use sealed, purchasable, open, and temporary-lock reasons.

Published room roles provide map semantics.
Start is the opening survival area.
Hub contains the central machine.
Combat rooms contain routes and holdouts.
Connectors are choke and trap sites.
Reward rooms contain services or secrets.
Exit is an optional extraction site.

Authored recipe families must bind to semantic rooms and generated anchors after candidate acceptance.
They can define devices and round requirements.
They can define clues and enemy access.
They can also define wonder-weapon behavior.
They must not use fixed world coordinates or one curated seed.

Common enemies must use pursuit and interception.
The orb enemy and projectile systems define ranged attacks for Casters, Elites, and bosses.
Each bullet-pattern event must use a distinct telegraph.
Limit simultaneous bullet-pattern events so the player can identify each event.

Main quest steps must show state through geometry and animation.
They must also use lighting, symbols, or audio where applicable.
Optional discovery steps can require inspection of additional world-state signals.
Each accepted action must give persistent feedback.
Puzzle verbs must stay physical and combat-linked.
The player can hold a zone or defeat enemies near a device.
The player can shoot or ricochet into targets.
The player can carry a component or lure an Elite.
The player can also activate a discovered sequence.

Keep the `F1` hard-coded arena as the focused combat regression path.
The product does not need multi-floor progression or an ECS.
It does not need a generic asset manager or save-anywhere support.
It does not need a general scripting system.

## Known limitations

- Some production archetypes can exceed two ordinary Combat leaves.
- Routine dead-end depth does not have a hard production limit.
- Useful-cycle and Shortcut metrics use arena transitions.
- These metrics do not measure physical centerline or doorway-angle separation.
- Multi-entry substantial-room coverage is 66.7% for the 94 accepted Fortress maps in the latest audit.
- This result is below the 70% target.
- The room-shape and combat grammar uses one main growth rule.
- Room-shape and combat briefs are not published.
- Firing lanes, holdout footprints, and presentation attachments are not published.
- Gate and Anchor binding is constraint-based, but the complete quest set does not compile to immutable semantic anchors.
- The static post-gate spawn-packing check is a hard gate.
- Initial-lock capacity is not a hard gate.
- The static post-gate spawn-packing check does not prove runtime spawn placement.
- Spawn validation does not check visibility bands or role compatibility.
- Runtime spawn failure does not have a bounded recovery rule.
- Automated endurance runs and reverse-field navigation caching are not complete.
- Portable seed replay still depends on standard-library random behavior.
- Movement, dash, projectiles, interactions, lighting, detail density, and traversal time need large-map playtests.
- Navigation runs a cell BFS for each enemy update.
- The first economy has one point currency and recipe-scaled gate prices.
- It has three damage, fire-rate, and dash tiers.
- It has pressure-scaled rewards and repeatable Hub repair.
- It has no ammunition economy, traps, service variants, or dynamic price balance.
- The Anchor objective is an eight-second holdout in the gold ring.
- Progress pauses when the player leaves the ring.
- The optional three-relay sequence shows the next target directly.
- Hub Circuit has no Start-to-Anchor Shortcut by design.
- Broken Ring and Twin Wings can keep their optional recipe route.
- Start and Anchor transitions from the Hub must have a normalized direction dot product of `0.42F` or less.
- The automatic director, bounded scaling, and voluntary extraction are implemented.
- Bespoke bosses and mutation systems are not implemented.
- Each fifth round uses the current Elite as its event.
- World lighting uses one directional shadow map and two presentation point lights.
- Actor contact shadows use projected decals.
- The combat regression ground and debug grid cover 80 by 80 units.
- Gameplay constants are compiled into their owner modules.
- Audio and rendering have graphical smoke coverage only.
- Debug runtime builds use `-Og` with GCC/Clang or `/O1` with MSVC.
- Use `-DSTALBERG_OPTIMIZE_DEBUG_RUNTIME=OFF` for fully unoptimized stepping.
- Use a Release build for performance tests.

## Validation and debugging

Run all checks:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run the seed audit:

```sh
./build/stalberg_level_seed_audit 1 100 > level-audit.csv
```

Run the graphical smoke test:

```sh
timeout 3s xvfb-run -a ./build/stalberg_game
```

Run on the local display:

```sh
DISPLAY=:0 ./build/stalberg_game
```

Look for this runtime evidence in raylib logs:

- Custom vertex and fragment shaders compile.
- Ground, wall, player, projectile, target, enemy, and shadow VAOs upload.
- Models unload before the custom shader at window close.

Use a Release build for performance conclusions.
Do not use only a Debug build for performance conclusions.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
