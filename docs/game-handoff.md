# Game Handoff

This is the continuation guide for the `stalberg_game` runtime. The previous future milestone sequence is superseded. Read [`level-generation-lock-in.md`](level-generation-lock-in.md) for the only active roadmap and acceptance gates; use the generator documents for current procedural-level contracts. [`level-identity-pass.md`](level-identity-pass.md) is retained as historical context for the first, insufficient anti-oatmeal pass.

## Planning reset — generation and quests only

The topology-label pass did not solve the oatmeal problem. A named graph archetype may still produce zero useful cycles, many single-entry arenas, repetitive dead-end traversal, and uniformly compact room mechanics. Those results are no longer acceptable normal-map output.

Until the lock-in exits, do not advance bosses, enemy roster, economy breadth, meta-progression, controller work, or unrelated polish. The active order is:

1. Require two or three useful cycles, multi-entry substantial rooms, limited intentional leaves, shallow routine dead ends, and progression-driven shortcuts.
2. Generate and validate distinct room-shape/combat briefs with safe entrances, multiple firing lanes, objective footprints, and separated ingress.
3. Publish semantic anchors and bind one authored main quest plus an optional discovery path without fixed coordinates or room IDs.
4. Validate every gate stage for circulation, population, economy, quest solvability, and backtracking saved by shortcuts.
5. Iterate through deterministic bad-seed regressions and manual feel reviews until fresh maps are consistently memorable in both F2 and play.

The current five large-map archetypes and `TopologySignature` remain useful construction inputs and diagnostics, not proof of quality. Candidate acceptance must enforce the stronger gates in [`level-generation-lock-in.md`](level-generation-lock-in.md).

## Product destination

The first small-map horde **systems** vertical slice and its automatic endless-round rework are complete. Radius-5 shooter generation chooses Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing, then publishes Start, Hub, Anchor-capable Combat, Reward, and Exit structure. The runtime now starts Round 1 after a three-second countdown, advances every cleared round through a five-second intermission without input, and keeps the round index in overflow-safe 64-bit state. Point income, permanent exact-threshold gates, three tiers each of damage/fire-rate/dash upgrades, repeatable powered-Hub repair, map-wide Drifter/Runner/Caster/Elite pressure, the Anchor holdout, Hub activation, optional ordered relay puzzle, and voluntary Exit extraction remain persistent match systems. Hub Circuit has exactly one branch per semantic room, and small-map validation rejects Start and Anchor transitions that leave the Hub in nearly the same direction. The F2 overview and command-line recipe selection make every slice directly inspectable.

The product direction is an **automatically advancing, endless round-based horde shooter built around one fresh procedural map per new match**. The accepted map persists for the run and is reproducible from a displayed match seed; restarting keeps that seed, while starting a new match requests a different one. Difficulty remains deterministic for the accepted map and round index. Puzzle, gate, Anchor, and Hub state never pause or authorize the director, and extraction remains voluntary.

The procedural map is not a disposable floor in a multi-floor run, but fixed curated layouts are not the normal content model. A new match must derive grid seed, room seed, topology recipe, candidate sequence, physical-scale profile, and quest binding from one match seed, retry within a bounded budget, and accept only a fully validated result. Curated seeds remain regression fixtures and an emergency fallback. Authored recipes describe semantic relationships and requirements; generated geometry, routes, room shapes, and semantic-anchor placement vary between matches.

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
| R | Restart mutable match state on the same immutable generated map, or restart regression combat |
| N | Generate and enter a fresh match with a new seed |
| F1 | Toggle generated horde match / combat regression arena |
| F2 | Toggle the generated full-level developer overview |
| Left/Right in overview | Browse the fixed representative configurations read-only |
| Home in overview | Return to the active generated layout |
| Escape/window close | Exit |

The runtime starts by creating a fresh public match seed and deterministically deriving radius-8 Fortress V1 grid/room inputs. The application-level generator has an eight-attempt budget, accepts only candidates that satisfy the current systems plan plus Fortress V1 physical/capacity gates, and visibly publishes the known radius-5 Hub Circuit fixture if that budget is exhausted. `--seed=<unsigned decimal>` reproduces the accepted inputs, profile, retry count, and geometry with the same game build/toolchain; `N` requests another seed, while `R` only resets mutable state. `--recipe=hub|ring|wings` still launches fixed diagnostic fixtures and cannot be combined with `--seed`.

`GeneratedLevel` retains the accepted match/profile metadata, relaxed grid, exact dual geometry, neutral room graph, shooter layout, exact floor, walls, doorway thresholds, and immutable navigation. Larger layouts select and publish Hub and Spokes, Ring and Branches, Main Spine, Twin Districts, or Dense Core/Sparse Branch before candidate routing; they publish one high-degree Hub and prefer a leaf arena for Reward semantics. `LevelSession` owns the authoritative player, current room, dynamic doorway collision, and matching traversal state. `HordeMatch` owns points, permanent gate purchases, upgrades, persistent player attack state, the deterministic round schedule, map-wide enemies and hostile projectiles, Anchor/Hub/relay/Exit state, and whole-match reset. Room-shape grammar, semantic anchors, dynamic quest binding, opening-component validation, and full larger-map pacing are not implemented yet.

Round 1 guarantees enough points to buy the recipe-scaled first gate; Round 2 guarantees the Anchor route. Optional spending stays disabled until the required Anchor route is funded. Drifters and Runners pursue through the currently opened exact cell graph, while Casters and Elites use difficulty-scaled ranged fan patterns and local separation prevents complete crowd overlap. E resolves contextual gate/device interactions; the gate HUD prompt and authoritative purchase query share the same 2.2-world-unit threshold distance. At the Anchor, E can atomically fund a still-closed Anchor gate and begin the holdout when affordable. Rounds continue independently through active or incomplete Anchor/Hub state. The Hub sells three increasingly expensive tiers of each authoritative upgrade through 1/2/3, repairs one missing health per E interaction for a pressure-scaled price after activation, and the relay grants the next fire-rate tier. Locked gates render as one connected barred frame rather than disconnected posts. The F2 overview renders exact floor triangles, recipe graph, live lock state, semantic objective sites, relay order, seeds, candidate, and quality score. Browsing previews never mutates the active match.

`F1` switches to the preserved hard-coded 20-by-20 combat regression arena. That path still contains the stationary target, deterministic hostile orb, separate projectile pools, swept wall and damage collision, defeat/victory freeze, restart, effects, HUD, and procedural tones described by the first-enemy milestone.

## Runtime architecture

```text
main.cpp
    ├── requests immutable GeneratedLevel data through MatchGenerator
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
    ├── binds semantic sites and economy gates from the selected recipe
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
| `src/game/match_generator.hpp/.cpp` | Public match/profile derivation, bounded candidate construction, systems-plan and Fortress V1 acceptance, accepted-attempt metadata, and visible deterministic fallback |
| `src/game/match_map_metrics.hpp/.cpp` | World-space/player-relative doorway, room, objective, room-span, route, statically usable ingress/spawn-capacity, and Hub-degree measurements |
| `src/game/generated_level.hpp/.cpp` | Immutable generator artifact package, accepted match/profile metadata, exact floor triangles, closed walls, retained doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.hpp/.cpp` | Mutable generated traversal, authoritative generated player, room lifecycle/location, doorway locking, active walls, effect timers, and reset |
| `src/game/horde_match.hpp/.cpp` | Small-map recipe binding, endless director/difficulty, points/gates/tiered upgrades/Hub repair, scaled horde combat, Anchor/Hub/relay/Exit progression, and reset |
| `src/game/generated_encounter.hpp/.cpp` | Preserved generated-room regression coordinator and spawn-selection coverage; no longer the active generated runtime path |
| `src/game/enemy_collection.hpp/.cpp` | Stable enemy identities/order, collection movement and firing, earliest swept-hit selection, and all-defeated queries |
| `src/game/combat.hpp/.cpp` | Promoted player attack state plus reusable regression combat update order and injected wall geometry |
| `src/game/encounter.hpp/.cpp` | Hard-coded regression wrapper, optional target integration, and deterministic whole-encounter reset |
| `src/game/enemy.hpp/.cpp` | Deterministic orbit movement, fan cadence, health/damage, and hostile projectile profile |
| `src/game/combat_audio.hpp/.cpp` | Audio-device ownership and generated combat tones |
| `src/game/arena.hpp/.cpp` | Immutable arena segments, reusable circle collision/sliding, and projectile-wall resolution |
| `src/game/collision_2d.hpp/.cpp` | Reusable closest-point, swept-circle, segment, earliest-hit, and closed-loop containment queries |
| `src/game/player.hpp/.cpp` | Player/Input state, movement, cooldown-based dash, facing, health/damage/invulnerability, and interpolation |
| `src/game/weapon.hpp/.cpp` | Fire cadence and muzzle spawning |
| `src/game/projectile_pool.hpp/.cpp` | Profile-driven preallocated projectile slots, fixed-step movement/lifetime, reuse, and interpolation |
| `src/game/target.hpp/.cpp` | Target health/reset state and swept projectile-versus-circle collision |
| `src/game/game_camera.hpp/.cpp` | Camera creation/following, camera-relative movement, ground projection, and camera interpolation |
| `src/game/game_input.hpp/.cpp` | All current polling of raylib keyboard and mouse input |
| `src/game/game_renderer*` | Runtime scene, role-readable entities and telegraphs, scaled HUD, landmark, and fitted full-level overview drawing |
| `src/game/render_resources.hpp/.cpp` | Procedural model/mesh ownership, sparse floor-detail mesh, projectile glow, and bundled UI-font loading |
| `src/game/render_style.hpp/.cpp` | Shared world/HUD palette, 1280-by-800 virtual UI canvas, room-role colors, and HUD panels |
| `src/game/directional_shader.hpp` | Embedded GLSL, floor/wall surface treatment, and shared directional-light vector |
| `src/game/post_process_*` | Depth-aware ambient grounding, edge treatment, emissive bloom, tone mapping, FXAA, and gameplay screen effects |
| `tests/generated_level_tests.cpp` | Artifact alignment, exact floor area, wall/door authorization, representative-browser validity, traversal firing/preservation, deterministic multi-spawn filtering/identity, partial/all-enemies clear transitions, hostile cleanup, defeat/reset/Exit, Start spawn, and reachability coverage |
| `tests/game_tests.cpp` | Headless dash/collision, caller-owned combat, projectile ownership/profile/pool, blocked muzzles, weapon, single-enemy and collection determinism/damage/defeat, earliest-hit/identity tie-breaking, closed-wall containment, player damage/death, interpolation freeze, victory, and restart coverage |
| `tests/horde_match_tests.cpp` | Automatic director transitions, puzzle independence, difficulty/schedule snapshots, recipe sites/costs, economy guards, Fortress gate prompt-range interaction, gate/navigation safety, scaled spawning, concurrent objectives, extraction, upgrades, and reset |
| `tests/match_generation_tests.cpp` | Same-seed accepted-layout/profile/retry replay, cross-seed variation, same-map restart, real rejection/fallback, Fortress V1 actor-relative gates, and radius-only failure |
| `CMakeLists.txt` | Runtime/test source lists, raylib linkage, warnings, and Debug runtime optimization |

The renderer's destructor unloads models before unloading the shared lighting shader. `CombatAudio` unloads sounds before closing its audio device. Both presentation owners must be destroyed before `CloseWindow()`, which is why they live inside an inner scope in `main.cpp`.

## Technical decisions and invariants

### Coordinate model

```text
world X/Z = authoritative gameplay plane
world Y   = visual height
```

The sphere center stays at `PLAYER_RADIUS` above `Y = 0`. Aim points are ray intersections with the infinite `Y = 0` plane.

### Camera and controls

The camera uses orthographic projection with both a 45-degree elevation and diagonal heading. Its first Fortress V1 framing pass uses a 25-world-unit base orthographic size, 19.5-unit height, 14-unit diagonal offset, and 2.5-unit facing look-ahead. Viewports wider than 1.9:1 reduce the vertical orthographic size so ultrawide windows reveal only a bounded amount of additional world. Movement is derived from the camera's planar forward/right vectors; do not restore a hardcoded isometric input matrix. This framing was tuned independently of the 1.375× geometry-scale increase; locomotion and combat-range pacing still need playtesting.

### Timing

Simulation advances in fixed `1/120` second steps. Rendering interpolates between the previous and current player, enemy, camera, and projectile states. New gameplay behavior—especially dash state, weapon cooldowns, projectile movement, collision, and enemy logic—belongs in the fixed update loop, not the render path.

The player dash moves at 18 world units/second for 0.18 seconds and has a 0.8-second activation cooldown. It uses the camera-relative movement direction, falling back to the current aim-facing direction while stationary. Dash does not grant invulnerability and continues to use the standard circle-versus-wall collision and sliding path. Space is an edge input latched until a fixed step consumes it.

Frame time is clamped to 50 ms before entering the accumulator to avoid an unbounded catch-up spiral after pauses or debugger stops.

### Projectile contract

Projectile state uses `Vector2` X/Z coordinates and converts to `Vector3` only in the renderer. Each `ProjectilePool` owns a read-only `ProjectileProfile`, so player and enemy pools can use different tuning without ownership flags or per-projectile configuration. Spawning requires finite positive speed/lifetime and a finite nonnegative radius. Every pool contains 192 stable slots and scans from the beginning for the first inactive slot. A full pool drops the attempted shot; the weapon still consumes its cooldown so exhaustion cannot create a burst when a slot becomes available.

The base projectile profiles are:

| Constant | Player | Enemy |
|---|---:|---:|
| Fire interval | 0.1 seconds | 1.15 seconds |
| Projectile speed | 22 world units/second | 6.5 world units/second |
| Projectile lifetime | 1.8 seconds | 3.4 seconds |
| Projectile radius | 0.16 world units | 0.22 world units |

The player muzzle is 1.3 world units from the player center. The enemy emits three directions at 0 and ±14 degrees from its current player-facing direction. The F1 regression always uses the base enemy values. `HordeMatch` rebuilds its hostile pool at round start with projectile speed scaled from 6.5 up to 9.1 units/second and gives each ranged horde enemy a firing interval scaled from 1.15 down to 0.7475 seconds; lifetime and radius remain fixed.

Every fixed projectile update copies `position` to `previousPosition` before advancing. `updateCombat()` preserves the regression update order. The active generated path uses `HordeMatch`: `LevelSession` moves the player, persistent player attacks advance against active walls, relay/enemy swept hits resolve, horde movement and ranged patterns advance, hostile shots resolve, and contact/projectile player damage share invulnerability. Player attacks persist across rounds; hostile shots clear at intermission and whole-match reset.

Wall-aware weapon and enemy-pattern overloads reject a muzzle path blocked by injected geometry while still consuming cooldown. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment and keep wall collision before enemy/target/player collision; do not replace swept collision with a current-position overlap test.

### Target contract

The target is fixed at X/Z `(-5, -5)` with radius `0.85`, five health, a `0.16` second hit flash, and a one-second reset delay. `updateTarget()` calls the shared swept circle-versus-circle query for each active projectile, using the projectile pool profile's radius plus the target radius. Zero-length sweeps and initial overlaps are handled without division.

A hit immediately deactivates the projectile and removes one health. Processing stops when a hit defeats the target, so later slots in that fixed step remain active. While defeated, the target does not collide; it resets at the same position with full health after the delay. Target collision has no dedicated headless coverage by explicit request, so preserve and manually verify direct hits, fast crossing hits, misses, and single-hit behavior when changing it.

### Enemy and player-combat contract

The regression enemy starts at X/Z `(5, 5)`, has 20 health, moves at 2.4 units/second, circles counter-clockwise relative to the player, and adds a clamped radial correction toward a five-unit preferred distance. Its state contains no random source, and identical fixed-step inputs must produce identical movement and shots. Player hits use projectile motion relative to the enemy's previous/current positions, include both radii, consume the projectile, and remove one health. At zero health the regression encounter freezes in a victory state.

The preserved generated-encounter regression retains source-cell IDs and earliest-hit tie breaking. Active horde enemies instead receive monotonically increasing match spawn IDs, role-specific health/speed/reward values, exact door-aware pursuit, and deterministic local separation. A player projectile damages at most one enemy; hit and death transitions award points exactly once.

The player has five health. Hostile collision uses projectile motion relative to the player's previous/current fixed-step positions, includes both radii, deactivates a shot on contact even during invulnerability, and applies the current bounded horde-damage value before starting 0.8 seconds of invulnerability. Horde damage is one through pressure tier 7 and two thereafter; the F1 regression remains fixed at one. At zero player health the encounter freezes in a defeat state. A post-victory or post-defeat `restartPressed` input restores the owning match or regression encounter and clears its projectile pools.

### Player ownership boundary

There are two deliberate player owners in mutually exclusive runtime views: `LevelSession` owns the generated-match player, while the preserved regression `Encounter` owns its arena player. `HordeMatch` passes `LevelSession::player()` directly through movement, economy, puzzle, and horde combat while owning the independent persistent attack state. Regression `CombatState` remains the caller-owned single-enemy F1 path. Do not copy or synchronize parallel generated players.

### Arena contract

`ARENA_WALLS` contains four ordered X/Z segments forming a square from `-10` to `10` on each axis. Reusable `updateCombat()` requires an injected wall span; `updateEncounter()` preserves its injected-wall overload, while its three-argument regression overload forwards `ARENA_WALLS`. Player collision treats the player as a `PLAYER_RADIUS` circle, resolves contacts with four fixed passes, and projects away only velocity into each contact normal so tangential movement survives. The pre-movement player position selects the stable side of wall-face contacts.

Projectile collision uses the shared `collision_2d` queries to treat each projectile as a moving circle and test its full previous-to-current path against segment faces and endpoint circles. The query accepts the pool profile's radius, selects the earliest hit across all walls, and lets the arena resolver clip the position to contact and deactivate the projectile. Because the muzzle can extend beyond a wall while the player remains inside, projectile centers already outside the closed convex wall loop are also deactivated before they can escape. Wall meshes extend outward from the ordered arena segments, leaving each mesh's inner face aligned with its simulation segment.

### Input boundary

`updatePlayer()`, `updateCombat()`, and `updateEncounter()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. Restart and dash are edge inputs, and `main.cpp` latches each until one fixed step consumes it. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model`, `Shader`, `Font`, or `Sound` handles. `GameRenderer` and its rendering subsystems own runtime graphics resources, while `CombatAudio` owns the audio device and generated sounds. Projectiles expose stable simulation state to presentation modules rather than issuing draw or audio calls from simulation code.

HUD and overview drawing use a centered 1280-by-800 virtual canvas scaled uniformly to the current framebuffer. Position HUD elements against `UI_CANVAS_WIDTH` and `UI_CANVAS_HEIGHT`, not the native window dimensions. Text uses the bundled ComicShannsMono Nerd Font Mono loaded by `RenderResources`; CMake copies the asset beside native builds and preloads it into the web virtual filesystem. The Boost bar communicates readiness through its full cyan fill without a redundant text label.

The web build keeps raylib's framebuffer fixed at 1280 by 800 and lets `web/shell.html` scale that 16:10 canvas uniformly within the viewport. Do not enable `FLAG_WINDOW_RESIZABLE` on web: raylib otherwise sizes the framebuffer to the browser aspect ratio while CSS letterboxes the canvas, stretching the image and making GLFW mouse coordinates disagree with `GetScreenWidth()` and `GetScreenHeight()`.

### Procedural generation boundary

The game target links the generator libraries only through `GeneratedLevel`. The package retains `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone. It also recovers and retains one `DoorwayThreshold` segment per published doorway. Its public API is read-only after construction.

Within one room, neighboring assigned cells are traversable. Across rooms, immutable navigation uses only exact cell pairs published by `RoomLayout::getDoorways()`; physical contact between regions is never automatically traversable. `LevelSession` owns mutable lock state and adds locked threshold segments to both active collision walls and traversal checks. Connected exterior entrance cells remain enclosed. The current powered-Exit requirement is still hard-coded to Round 5, but the interaction is now explicit voluntary extraction and never gates automatic round advancement. Replace that requirement with recipe-authored quest metadata in the next slice.

Normal runtime generation uses the `FortressV1` physical profile: radius 8 and `GeneratedLevelConfig::worldScale = 0.22F`, derived with grid/room seeds from a fresh 64-bit match seed. `MatchGenerator` attempts at most eight candidates, records the accepted profile/attempt, and visibly uses the radius-5, `0.16F` Hub Circuit fixture only after exhaustion. Systems-plan validation requires Start, Hub, Anchor-capable Combat, Reward, and Exit rooms; three relay targets; and Expansion, Anchor, Reward, and Exit gates. Physical validation additionally requires minimum player-relative doorway width, substantial/Anchor area, objective clearance, Anchor room span, route distance, statically usable cross-room ingress separation, usable spawn candidates, spawn-bearing rooms, and Hub doorway degree. A radius-8 layout left at `0.16F` fails. `--seed` supplies the public match seed explicitly. The launch presets use room seeds 7/2/3 for Hub Circuit/Broken Ring/Twin Wings; `--recipe=hub|ring|wings` and the six F2 previews remain deterministic regression tools.

Current replay determinism is scoped to the same game build/toolchain. Lower-level grid and room generation still use standard-library shuffle and distribution implementations, so reproducing a seed across a different C++ standard library is not guaranteed. Production seed compatibility needs fixed project-owned random algorithms or an explicit generation-version contract before seeds can be promised portable across releases.

## Completed implementation slice: automatic endless rounds and scaling

The replacement for the finite input-gated director and the first bounded pressure profile are implemented. Deterministic fixed-step simulation and the generated-level ownership boundaries remain unchanged.

### 1. Decouple and automate the round director — complete

- Removed `HORDE_FINAL_ROUND`, `PlayerInput::startRoundPressed`, `KEY_N`, queued round-start input, and the deploy-next-wave prompt.
- Round 1 starts after a three-second countdown; cleanup starts a five-second intermission that advances automatically.
- `Intermission`, `Buildup`, `Peak`, and `Cleanup` remain explicit, director-owned phases. Puzzle, gate, Hub, and Anchor state cannot block them.
- The round index is overflow-safe `std::uint64_t`; schedule and profile arithmetic clamps before multiplication, including at the maximum representable round.
- The HUD shows the current round and intermission countdown. Hostile projectiles clear at cleanup/intermission while player attacks, purchases, upgrades, and puzzle state persist.

### 2. Add deterministic difficulty scaling — complete

One reproducible profile now derives from round index and map recipe:

1. Spawn budget grows from 6 to a cap of 48; simultaneous living pressure grows from 6 to 18 while pacing shortens within fixed bounds.
2. Composition substitutes Runners, Casters, and up to three Elites for Drifters as pressure tiers rise.
3. Deterministic role ordering and monotonic spawn IDs vary role/ingress sequencing by round and recipe.
4. Health, movement, hostile projectile speed, firing cadence, damage, and rewards scale from tier-0 baseline through ten bounded escalation tiers.
5. Every fifth round schedules an Elite event without creating a terminal round.

Health tops out at 1.8×, movement at 1.25×, hostile projectile speed at 1.4×, firing interval at 0.65×, damage at two, and reward income at 1.5×. Spawn budget and simultaneous population cap by Round 25; attribute and role-substitution scaling reaches its final pressure tier at Round 51, while five-round Elite events continue. Recipe-scaled gate costs preserve the opening economy, three increasingly expensive tiers of each upgrade match the bounded threat curve, and activated-Hub repairs remain a repeatable post-cap sink. Bespoke bosses and mutation events remain future content.

## Generation foundation status and superseded plan notes

Sections 3–4 record completed foundations. The old ordering in sections 5–7 is superseded by the circulation → room grammar → semantic anchors/quests → component validation → feel-gate sequence in [`level-generation-lock-in.md`](level-generation-lock-in.md). Keep the technical constraints below, but do not implement them in their former order.

### 3. Add the new-match generation boundary — complete

- Normal play creates one 64-bit match seed and deterministically derives grid seed, room seed, physical profile, and candidate retries from it.
- `N` requests a fresh match. `R` continues to reset the current match on the same accepted map.
- `--seed=<unsigned decimal>` replays a match, and the HUD/F2 overview display the accepted seed, attempt count, and fallback status.
- Candidate construction and systems-plan/physical validation use an eight-attempt budget. Exhaustion selects the known-valid Hub Circuit fixture and marks fallback use visibly.
- Representative configurations remain tests and F2 previews rather than the normal runtime selection pool.
- Headless coverage verifies same-seed reproduction, cross-seed input variation, same-map restart, deterministic fallback, and scale-profile participation in derivation.

The boundary is now independent of the old systems dimensions, but topology/quest choice still lacks authored recipe/anchor metadata.

### 4. Increase physical map scale, not only grid radius — Fortress V1 complete

- Normal maps now use radius 8 and `worldScale = 0.22F`; player/enemy collision bodies remain unchanged. Systems fixtures and deterministic fallback remain radius 5 at `0.16F`.
- `PhysicalMapProfile` is recorded with accepted match metadata and displayed in the HUD/F2 overview.
- `MatchMapMetrics` measures doorway width, substantial and Anchor room area, objective clearance, Anchor room-center span, Start-to-Exit route distance, statically usable cross-room ingress separation, usable spawn candidates, spawn-bearing rooms, and Hub doorway degree. The span is a room-size proxy, not a true line-of-sight test.
- Fortress V1 rejects candidates below explicit thresholds and rejects a radius-8 map left at systems world scale. Larger shooter layouts publish one high-degree Hub and prefer a leaf arena for Reward so the existing optional route remains bindable.
- Camera framing was widened independently to a 25-unit orthographic view with adjusted height, offset, and look-ahead; actors and all combat distances were not globally scaled.
- Headless coverage proves profile replay, accepted metric thresholds, larger semantic roles, and radius-only rejection.

Fortress V1 acceptance currently requires:

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

Static spawn usability currently means reachable in the immutable all-open graph, sufficient enemy-sized source clearance, and no overlap with immutable walls. It does not claim that a candidate is reachable through current locks, far from the player's dynamic position, or unoccupied at a particular spawn step.

The remaining physical work is pacing and deeper validation: connector-specific dimensions, true sightline bands, opening-component ingress/circulation/economy, traversal time, movement/dash, projectile reach/lifetime, enemy visibility, interaction radii, lighting/shadows, floor-detail density, and navigation performance.

### 5. Bind concurrent recipe-authored quests

Puzzle logic becomes an independent persistent state machine. A step may advertise `minimumRound`, enemy-role, kill, currency, room, or powered-device requirements, but it must never own the round transition.

- Round requirements unlock puzzle actions; they do not hold an intermission open.
- Incomplete puzzle steps persist across any number of rounds.
- Combat-linked steps must define whether progress persists, pauses, or resets, and communicate that rule before activation.
- Anchor holdouts, relays, clue sequences, and future devices must remain usable while the endless director advances.
- Quest completion should unlock extraction, a boss, a major reward, or a new pressure tier. It must not silently stop spawning.
- If extraction ends a run, make it an explicit player interaction; otherwise the match continues until defeat.

Replace the current hard-coded `Round 2` Anchor and `Round 5` Exit checks with recipe-authored requirement metadata. The puzzle model should consume semantic room/device anchors rather than world coordinates or assumptions about a five-round schedule.

### 6. Rework generation for sustained endless play

Fortress V1 establishes large random spaces but does not yet prove sustained endless-combat quality. Generation and candidate scoring still need to account for:

- Multiple separated enemy ingress regions with wall-safe spawn capacity.
- Loops, alternate kiting routes, and recovery space after gates open.
- Room-shape grammar for compact, elongated, concave, split, and multi-entrance combat briefs.
- Objective sites that do not permanently collapse circulation or create dominant safe spots.
- Puzzle dependencies distributed across meaningful route choices rather than one round-gated branch.
- Population capacity, sightline variety, ranged-enemy positions, and late-round navigation cost.
- Economy pacing and unlock order under automatic rounds, including a viable opening component before the first gate purchase.

Keep the three small recipes as deterministic regression fixtures. Normal play now uses fresh validated Fortress V1 layouts with larger extent and world-space geometry. Current gates prove map-wide spawn capacity and ingress separation; the next validation must prove capacity within the currently opened component, enough valid ingress lanes as it expands, authored quest realization, circulation, and economy order. Curated large configurations may support balancing but must not replace random normal play.

### 7. Acceptance and test coverage

Headless coverage now verifies automatic Round 1 startup, cleanup → intermission → next-round transitions, input and puzzle independence, deterministic recipe-aware schedules, snapshots at rounds 1/5/10/25/100, monotonic bounded pressure, maximum-round arithmetic, concurrent Anchor progress, explicit extraction, tiered upgrades, scaled economy rewards, and reset of countdown/difficulty/match state.

The new match-generation suite covers different seeds deriving different geometry inputs, exact same-seed accepted-layout/profile/retry reproduction, `R` preserving the accepted map, the separate generation path used by `N`, deterministic visible fallback, actor-relative Fortress V1 thresholds, larger-map Hub/Reward semantics, and radius-only rejection. The next slices must add coverage for:

- Cross-seed structural diversity and future semantic quest bindings beyond the current spatial-tree tier.
- Exact reproduction of future semantic anchors and quest placement.
- Recipe-authored minimum-round puzzle unlocks that neither reset nor stop the director.
- Quest completion with continued spawning before voluntary extraction.
- Generated maps meeting ingress-capacity, circulation, objective-clearance, and late-round navigation constraints.

The updated [`small-puzzle-horde-slice.md`](small-puzzle-horde-slice.md) remains the acceptance guide for the fixed automatic-endless systems fixtures. The match-generation suite now covers random generation and physical scale; a successor guide still needs quest-aware binding and full Fortress V1 pacing.

## Intended horde-mode boundary

The completed game keeps `GeneratedLevel` immutable and layers the persistent match on top of it:

```text
GeneratedLevel
    └── replayable random map, physical-scale profile, room roles, semantic anchors, doorway thresholds, navigation, spawn candidates

Horde match state
    ├── authoritative LevelSession player and active collision walls
    ├── endless automatic director: countdown → buildup → peak → cleanup → timed intermission → next round
    ├── round-indexed difficulty profile + deterministic spawn schedule
    ├── stable enemy collection with bounded active-population pressure
    ├── map progression: currency, gates, services, traps, and powered rooms
    ├── concurrent quest state: requirements, Grid Anchors, clues, secrets, and extraction unlock
    └── terminal state: death or explicit player-chosen extraction
```

The vertical slice maps purchasable and objective gates onto exact doorway lock bits; purchases and Hub activation update rendering, collision, player traversal, and enemy navigation together. Waves operate across the currently opened component rather than isolated room encounters. The revised director must continue advancing even when a required route remains closed, so every generated opening component needs enough ingress, circulation, and economy capacity for its scheduled early rounds. A future expansion should replace the current boolean lock with explicit sealed, purchasable, open, and temporary-lock reasons.

The published room roles provide map semantics: Start is the opening survival area, Hub owns the central machine, Combat rooms host training routes and holdouts, Connectors become chokepoints and trap sites, Reward rooms contain services or secrets, and Exit becomes an optional extraction site rather than the automatic end of a final round. Authored recipe families bind devices, minimum-round requirements, clue families, enemy access, and wonder-weapon behavior to semantic rooms and generated anchors after a random candidate is accepted; they must never hard-code world coordinates or require one curated seed.

Common enemies should create crowd pressure through pursuit and interception. The existing deterministic orb enemy and projectile systems establish the ranged language for Casters, elites, and bosses. Bullet patterns must remain identifiable exceptions inside the horde rather than becoming undifferentiated projectile noise.

Main quest steps should communicate state through world geometry, animation, lighting, symbols, and audio. Optional Easter eggs can demand deeper observation, but every accepted action must produce persistent feedback. Puzzle verbs should remain physical and combat-linked: hold a zone, defeat enemies near a device, shoot or ricochet into targets, carry a component, lure an elite, or activate a discovered sequence.

Keep the `F1` hard-coded arena as the focused combat regression path. The destination does not require multi-floor progression, an ECS, a generic asset manager, save-anywhere support, or a general scripting system.

## Known limitations

- The current large-map archetype pass still accepts zero/one-cycle trees, too many single-entry Combat rooms, deep routine dead ends, and uniformly compact room mechanics. This is the primary blocker; named archetypes and F2 signatures do not satisfy the new feel gate.
- Normal runtime selection is fresh, replayable, and physically larger under Fortress V1, but still binds the fixed Anchor/Hub/relay/Exit systems plan rather than an authored semantic quest recipe.
- Fortress V1 proves actor-relative map-wide geometry and spawn capacity, not opening-component circulation/economy or final pacing. Movement, dash, projectiles, interactions, visibility, lighting, detail density, and traversal time still need large-map playtests.
- Larger maps now use five deterministic topology archetypes with archetype-specific graph validation and published structural signatures. They still share compact-room growth, so room-shape grammar, districts, negative-space briefs, and puzzle-specific geometry remain the main oatmeal risk.
- Navigation recomputes a cell BFS per enemy update and should be replaced with cached reverse distance fields by player cell and topology revision before raising population/performance targets on Fortress V1.
- The first economy has one point currency, recipe-scaled gate prices, three tiers each of damage/fire-rate/dash upgrades, pressure-scaled rewards, and repeatable activated-Hub health repair. It has no ammunition economy, traps, service placement variants, or dynamic price balancing.
- The required Anchor interaction is a combat holdout: press E, remain inside the gold ring for eight accumulated seconds, and resume after leaving. It is a pressure objective, not a logic puzzle. The optional three-relay sequence is the only current puzzle and exposes the next correct target directly, so puzzle depth remains a primary design gap.
- Hub Circuit deliberately has no Start → Anchor shortcut: each semantic room receives one distinct Hub branch. Broken Ring and Twin Wings may retain their recipe-specific optional route, but accepted Start and Anchor Hub transitions must be separated by at least about 65 degrees.
- The automatic director, bounded recipe-aware scaling, and voluntary extraction are implemented. There are no bespoke boss roles or mutation mechanics yet; every fifth round currently uses the existing Elite as its readable event.
- World lighting uses one directional shadow map and two presentation-driven point lights; actor contact shadows remain projected decals rather than full dynamic occlusion.
- The combat regression ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Generated-level and combat regression tests retain all prior geometry, doorway, navigation, encounter, projectile, damage, and reset coverage. Horde coverage also verifies that the complete displayed 2.2-unit gate interaction range purchases and unlocks a Fortress V1 threshold. Room-generation tests enforce exact small-recipe edges plus one Hub/Reward arena on larger layouts. `stalberg_match_generation_tests` covers same-seed profile/layout/retry replay, cross-seed variation, same-map reset, real candidate rejection/fallback, Fortress V1 thresholds, and radius-only failure. `stalberg_horde_match_tests` covers automatic progression, difficulty, economy, gates, spawning, concurrent objectives, extraction, upgrades, repair, and reset. Audio and rendering remain graphical-smoke coverage.
- Debug runtime builds use debugger-friendly optimization (`-Og` with GCC/Clang or `/O1` with MSVC) for `stalberg_game` and a bundled raylib while retaining debug symbols and assertions. Configure with `-DSTALBERG_OPTIMIZE_DEBUG_RUNTIME=OFF` when fully unoptimized instruction-by-instruction stepping is required. Use a separate Release build when profiling performance.

## Validation and debugging

Run all checks:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Graphical smoke test:

```sh
timeout 3s xvfb-run -a ./build/stalberg_game
```

Interactive local display:

```sh
DISPLAY=:0 ./build/stalberg_game
```

Useful runtime evidence in raylib logs:

- Custom vertex and fragment shaders compile successfully.
- Ground, wall, player, projectile, target, enemy, and shadow VAOs upload successfully.
- Models unload before the custom shader when the window closes.

When investigating performance, still configure and compare a Release build rather than drawing final conclusions from a Debug build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
DISPLAY=:0 ./build-release/stalberg_game
```
