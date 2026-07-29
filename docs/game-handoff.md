# Game Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence, [`level-identity-pass.md`](level-identity-pass.md) for the immediate anti-oatmeal generation pass, and the generator documents for current procedural-level contracts.

## Product destination

The first small-map horde **systems** vertical slice is complete. Radius-5 shooter generation chooses Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing, then publishes Start, Hub, Anchor-capable Combat, Reward, and Exit structure. The persistent match includes five rounds, point income, permanent exact-threshold gates, damage/fire-rate/dash upgrades, map-wide Drifter/Runner/Caster/Elite pressure, an Anchor holdout, Hub activation, an optional ordered relay puzzle, and explicit Exit completion. Hub Circuit now has exactly one branch per semantic room: the redundant Start → Anchor shortcut was removed after playtesting showed two adjacent routes reaching effectively the same place. Small-map validation also rejects Start and Anchor transitions that leave the Hub in nearly the same direction. The F2 overview and command-line recipe selection make every slice directly inspectable. The next design priority is genuine puzzle depth and broader room-shape grammar rather than calling the required holdout itself a puzzle. The destination is a **round-based horde shooter built around one persistent, learnable generated map per match**. The player survives escalating crowds, earns currency, buys routes through exact doorway thresholds, powers distributed machinery, improves weapons, solves a readable main objective, discovers optional Easter eggs, and reaches a boss or extraction. Common enemies create contact pressure and crowd-routing problems; ranged enemies, elites, objectives, and bosses introduce readable bullet-hell patterns.

The procedural map is not a disposable floor in a multi-floor run. Its room roles become persistent landmarks, its doorway graph becomes an economy-controlled network for player and enemy movement, and its irregular geometry becomes part of combat and puzzle solving. Shipped maps should pair curated, validated generated seeds with authored map recipes so players can learn routes and clues across repeated attempts without losing the Stålberg geometry.

## Current behavior

Run:

```sh
cmake -S . -B build
cmake --build build -j
./build/stalberg_game --recipe=hub
# alternatives: --recipe=ring, --recipe=wings
```

Controls:

| Input | Action |
|---|---|
| WASD | Camera-relative movement |
| Space | Dash in the movement direction, or facing direction while stationary |
| Mouse | Aim on the XZ ground plane |
| Hold left mouse button | Fire anywhere on the generated map or in the regression arena |
| N | Start the next round during intermission |
| E | Buy a nearby gate or activate the Anchor, Hub, or Exit; at Anchor, fund-and-start atomically when affordable |
| 1/2/3 at Hub | Buy damage, fire-rate, or dash upgrades |
| R | Reset the complete match, or restart regression combat |
| F1 | Toggle generated horde match / combat regression arena |
| F2 | Toggle the generated full-level developer overview |
| Left/Right in overview | Browse the fixed representative configurations read-only |
| Home in overview | Return to the active generated layout |
| Escape/window close | Exit |

The runtime starts in the generated horde match. `GeneratedLevel` retains the relaxed grid, exact dual geometry, neutral room graph, selected small-map recipe, shooter layout, exact floor, walls, doorway thresholds, and immutable navigation. `LevelSession` owns the authoritative player, current room, dynamic doorway collision, and matching traversal state. `HordeMatch` owns points, permanent gate purchases, upgrades, persistent player attack state, the deterministic round schedule, map-wide enemies and hostile projectiles, Anchor/Hub/relay/Exit state, and whole-match reset.

Round 1 guarantees enough points to buy the first gate; Round 2 guarantees the Anchor route. Optional spending stays disabled until the required Anchor route is funded. Drifters and Runners pursue through the currently opened exact cell graph, Casters and the finale Elite use ranged fan patterns, and local separation prevents complete crowd overlap. E resolves contextual gate/device interactions; at the Anchor it can atomically fund a still-closed Anchor gate and begin the holdout when the player has enough points. N starts intermission rounds, and 1/2/3 buys authoritative upgrades at Hub. Locked gates render as one connected barred frame rather than disconnected posts. The presentation pass gives each enemy role a stronger silhouette, ranged wind-up telegraphs, contact shadows, hit/death effects, brighter role-aware floors, high-value wall caps, sparse floor traces, and a quieter void grid. The F2 overview renders exact floor triangles, recipe graph, live lock state, semantic objective sites, relay order, seeds, candidate, and quality score. Browsing previews never mutates the active match.

`F1` switches to the preserved hard-coded 20-by-20 combat regression arena. That path still contains the stationary target, deterministic hostile orb, separate projectile pools, swept wall and damage collision, defeat/victory freeze, restart, effects, HUD, and procedural tones described by the first-enemy milestone.

## Runtime architecture

```text
main.cpp
    ├── constructs immutable GeneratedLevel generation + geometry data
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
    ├── advances buildup → peak → cleanup → intermission rounds
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
| `src/game/generated_level.hpp/.cpp` | Immutable generator artifact package, exact floor triangles, closed walls, retained doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.hpp/.cpp` | Mutable generated traversal, authoritative generated player, room lifecycle/location, doorway locking, active walls, effect timers, and reset |
| `src/game/horde_match.hpp/.cpp` | Small-map recipe binding, points/gates/upgrades, rounds, horde roles/navigation/combat, Anchor/Hub/relay/Exit progression, and reset |
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
| `tests/horde_match_tests.cpp` | Recipe sites, economy reserves, atomic Anchor interaction, exact point awards, gate/navigation safety, geometry-safe spawning, round roles/cleanup, Anchor/Hub/Exit, optional Reward routing, relay order/reward, upgrades, and whole-match reset |
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

The camera uses orthographic projection with both a 45-degree elevation and diagonal heading. Its base orthographic size is 21.5 world units and its target looks 2.15 units ahead along the player's facing direction. Viewports wider than 1.9:1 reduce the vertical orthographic size so ultrawide windows reveal only a bounded amount of additional world. Movement is derived from the camera's planar forward/right vectors; do not restore a hardcoded isometric input matrix.

### Timing

Simulation advances in fixed `1/120` second steps. Rendering interpolates between the previous and current player, enemy, camera, and projectile states. New gameplay behavior—especially dash state, weapon cooldowns, projectile movement, collision, and enemy logic—belongs in the fixed update loop, not the render path.

The player dash moves at 18 world units/second for 0.18 seconds and has a 0.8-second activation cooldown. It uses the camera-relative movement direction, falling back to the current aim-facing direction while stationary. Dash does not grant invulnerability and continues to use the standard circle-versus-wall collision and sliding path. Space is an edge input latched until a fixed step consumes it.

Frame time is clamped to 50 ms before entering the accumulator to avoid an unbounded catch-up spiral after pauses or debugger stops.

### Projectile contract

Projectile state uses `Vector2` X/Z coordinates and converts to `Vector3` only in the renderer. Each `ProjectilePool` owns a read-only `ProjectileProfile`, so player and enemy pools can use different tuning without ownership flags or per-projectile configuration. Spawning requires finite positive speed/lifetime and a finite nonnegative radius. Every pool contains 192 stable slots and scans from the beginning for the first inactive slot. A full pool drops the attempted shot; the weapon still consumes its cooldown so exhaustion cannot create a burst when a slot becomes available.

The current projectile profiles are:

| Constant | Player | Enemy |
|---|---:|---:|
| Fire interval | 0.1 seconds | 1.15 seconds |
| Projectile speed | 22 world units/second | 6.5 world units/second |
| Projectile lifetime | 1.8 seconds | 3.4 seconds |
| Projectile radius | 0.16 world units | 0.22 world units |

The player muzzle is 1.3 world units from the player center. The enemy emits three directions at 0 and ±14 degrees from its current player-facing direction.

Every fixed projectile update copies `position` to `previousPosition` before advancing. `updateCombat()` preserves the regression update order. The active generated path uses `HordeMatch`: `LevelSession` moves the player, persistent player attacks advance against active walls, relay/enemy swept hits resolve, horde movement and ranged patterns advance, hostile shots resolve, and contact/projectile player damage share invulnerability. Player attacks persist across rounds; hostile shots clear at intermission and whole-match reset.

Wall-aware weapon and enemy-pattern overloads reject a muzzle path blocked by injected geometry while still consuming cooldown. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment and keep wall collision before enemy/target/player collision; do not replace swept collision with a current-position overlap test.

### Target contract

The target is fixed at X/Z `(-5, -5)` with radius `0.85`, five health, a `0.16` second hit flash, and a one-second reset delay. `updateTarget()` calls the shared swept circle-versus-circle query for each active projectile, using the projectile pool profile's radius plus the target radius. Zero-length sweeps and initial overlaps are handled without division.

A hit immediately deactivates the projectile and removes one health. Processing stops when a hit defeats the target, so later slots in that fixed step remain active. While defeated, the target does not collide; it resets at the same position with full health after the delay. Target collision has no dedicated headless coverage by explicit request, so preserve and manually verify direct hits, fast crossing hits, misses, and single-hit behavior when changing it.

### Enemy and player-combat contract

The regression enemy starts at X/Z `(5, 5)`, has 20 health, moves at 2.4 units/second, circles counter-clockwise relative to the player, and adds a clamped radial correction toward a five-unit preferred distance. Its state contains no random source, and identical fixed-step inputs must produce identical movement and shots. Player hits use projectile motion relative to the enemy's previous/current positions, include both radii, consume the projectile, and remove one health. At zero health the regression encounter freezes in a victory state.

The preserved generated-encounter regression retains source-cell IDs and earliest-hit tie breaking. Active horde enemies instead receive monotonically increasing match spawn IDs, role-specific health/speed/reward values, exact door-aware pursuit, and deterministic local separation. A player projectile damages at most one enemy; hit and death transitions award points exactly once.

The player has five health. Hostile collision uses projectile motion relative to the player's previous/current fixed-step positions, includes both radii, deactivates a shot on contact even during invulnerability, and removes at most one health before starting 0.8 seconds of invulnerability. At zero player health the encounter freezes in a defeat state. A post-victory or post-defeat `restartPressed` input assigns a fresh `Encounter`, clearing both pools and restoring player, weapon, target, and enemy state exactly.

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

Within one room, neighboring assigned cells are traversable. Across rooms, immutable navigation uses only exact cell pairs published by `RoomLayout::getDoorways()`; physical contact between regions is never automatically traversable. `LevelSession` owns mutable lock state and adds locked threshold segments to both active collision walls and traversal checks. Connected exterior entrance cells remain enclosed; map completion requires the powered Exit monument after Round 5.

The active runtime uses radius 5, grid seed 1, and curated room seed 7 by default. Recipe selection follows `(roomSeed - 1) % 3`; the launch presets use room seeds 7/2/3 for Hub Circuit/Broken Ring/Twin Wings. `--recipe=hub|ring|wings` launches each directly. The Hub preset moved from seed 1 to seed 7 after the duplicate-route fix so the representative physical layout visibly exposes separated branches and only one connector. F2 exposes exact floor, semantic roles/sites, published recipe graph, open/locked thresholds, player position, generator metadata, and live objective state. Left/Right browses six fixed previews without mutating the match.

## Next implementation slice: broaden intentional generation

The overview, three small-map recipes, economy, horde director, role navigation, Anchor/Hub/Exit quest, relay reward, and upgrades are complete. Continue the anti-oatmeal pass by:

1. Publishing room-shape grammar driven by each room's mechanical brief.
2. Replacing the required Anchor-only pressure objective with a readable multi-step puzzle dependency; the current optional relay is the only sequence puzzle and is intentionally simple.
3. Scoring anti-alternation, direct arena adjacency, district separation, landmark sightlines, and route changes caused by objectives.
4. Adding authored clue families, traps, kill-powered devices, and geometry-driven weapon rewards.
5. Extending deterministic crowd and economy validation to larger maps without weakening the small recipe signatures.

Use [`small-puzzle-horde-slice.md`](small-puzzle-horde-slice.md) as the current acceptance checklist.

## Intended horde-mode boundary

The completed game keeps `GeneratedLevel` immutable and layers the persistent match on top of it:

```text
GeneratedLevel
    └── exact map, room roles, doorway thresholds, navigation, spawn candidates

Horde match state
    ├── authoritative LevelSession player and active collision walls
    ├── round director: buildup → peak → cleanup → intermission
    ├── stable enemy collection and deterministic spawn schedule
    ├── map progression: currency, gates, services, traps, and powered rooms
    ├── map quest: Grid Anchors, Hub machine, clues, secrets, and finale
    └── terminal state: death, extraction, victory, or endless continuation
```

The vertical slice maps purchasable and objective gates onto exact doorway lock bits; purchases and Hub activation update rendering, collision, player traversal, and enemy navigation together. Waves operate across the currently opened component rather than isolated room encounters. A future expansion should replace the current boolean lock with explicit sealed, purchasable, open, and temporary-lock reasons.

The published room roles provide map semantics: Start is the opening survival area, Hub owns the central machine, Combat rooms host training routes and holdouts, Connectors become chokepoints and trap sites, Reward rooms contain services or secrets, and Exit becomes the finale or extraction site. Curated generated seeds should be paired with authored map recipes that assign devices, clue families, enemy access, and wonder-weapon behavior without hard-coding world coordinates.

Common enemies should create crowd pressure through pursuit and interception. The existing deterministic orb enemy and projectile systems establish the ranged language for Casters, elites, and bosses. Bullet patterns must remain identifiable exceptions inside the horde rather than becoming undifferentiated projectile noise.

Main quest steps should communicate state through world geometry, animation, lighting, symbols, and audio. Optional Easter eggs can demand deeper observation, but every accepted action must produce persistent feedback. Puzzle verbs should remain physical and combat-linked: hold a zone, defeat enemies near a device, shoot or ricochet into targets, carry a component, lure an elite, or activate a discovered sequence.

Keep the `F1` hard-coded arena as the focused combat regression path. The destination does not require multi-floor progression, an ECS, a generic asset manager, save-anywhere support, or a general scripting system.

## Known limitations

- Small maps publish three intentional graph recipes, but substantial rooms still use the compact baseline growth process. Room-shape grammar, districts, negative-space briefs, and puzzle-specific geometry remain the main oatmeal risk.
- Navigation recomputes a cell BFS per enemy update and is appropriate for the small population cap; larger maps should cache reverse distance fields by player cell and topology revision.
- The first economy has one point currency, three upgrades, and fixed gate prices. It has no ammunition economy, traps, service placement variants, or dynamic price balancing.
- The required Anchor interaction is a combat holdout: press E, remain inside the gold ring for eight accumulated seconds, and resume after leaving. It is a pressure objective, not a logic puzzle. The optional three-relay sequence is the only current puzzle and exposes the next correct target directly, so puzzle depth remains a primary design gap.
- Hub Circuit deliberately has no Start → Anchor shortcut: each semantic room receives one distinct Hub branch. Broken Ring and Twin Wings may retain their recipe-specific optional route, but accepted Start and Anchor Hub transitions must be separated by at least about 65 degrees.
- Round 5 contains an Elite but not a bespoke multi-phase boss, extraction choice, or endless continuation.
- World lighting uses one directional shadow map and two presentation-driven point lights; actor contact shadows remain projected decals rather than full dynamic occlusion.
- The combat regression ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Generated-level and combat regression tests retain all prior geometry, doorway, navigation, encounter, projectile, damage, and reset coverage. Room-generation tests enforce exact recipe edges, Hub Circuit's one-branch-per-semantic-room graph, Reward leaf structure, and separated Start/Anchor Hub approaches. `stalberg_horde_match_tests` adds recipe-site validity, reserved progression currency, atomic Anchor purchase/activation, exact one-time point awards, atomic gate traversal, wall-safe spawn/separation behavior, deterministic round roles, holdout-gated cleanup, optional Reward routing, Anchor/Hub/Exit progression, relay ordering/reward, upgrades, and whole-match reset. Audio and rendering remain graphical-smoke coverage.
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
