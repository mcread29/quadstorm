# Game Prototype Handoff

This is the continuation guide for the `stalberg_game` runtime. Read [`game-roadmap.md`](game-roadmap.md) for the milestone sequence, [`level-identity-pass.md`](level-identity-pass.md) for the immediate anti-oatmeal generation pass, and the generator documents for current procedural-level contracts.

## Product destination

Milestone 7, generated-room encounters and floor progression, is complete. The Milestone 8 player foundation is also complete: dash, always-available generated-map firing, and stable generated enemy collections now work. The first level-identity slice is complete: a runtime overview exposes the full exact floor and a read-only representative-seed browser without mutating the active session. Before crowd navigation and rounds continue, the immediate priority remains the rest of the level identity and legibility pass: replace repetitive arena/connector chains with explicit topology archetypes, diversify substantial-room geometry, and publish anchors for recognizable role- and district-driven landmarks. The destination is a **round-based horde shooter built around one persistent, learnable generated map per match**. The player survives escalating crowds, earns currency, buys routes through exact doorway thresholds, powers distributed machinery, improves weapons, solves a readable main objective, discovers optional Easter eggs, and reaches a boss or extraction. Common enemies create contact pressure and crowd-routing problems; ranged enemies, elites, objectives, and bosses introduce readable bullet-hell patterns.

The procedural map is not a disposable floor in a multi-floor run. Its room roles become persistent landmarks, its doorway graph becomes an economy-controlled network for player and enemy movement, and its irregular geometry becomes part of combat and puzzle solving. Shipped maps should pair curated, validated generated seeds with authored map recipes so players can learn routes and clues across repeated attempts without losing the Stålberg geometry.

## Current behavior

Run:

```sh
cmake -S . -B build
cmake --build build -j
./build/stalberg_game
```

Controls:

| Input | Action |
|---|---|
| WASD | Camera-relative movement |
| Space | Dash in the movement direction, or facing direction while stationary |
| Mouse | Aim on the XZ ground plane |
| Hold left mouse button | Fire anywhere on the generated map or in the regression arena |
| R | Reset to Start, or restart after combat defeat/victory |
| F1 | Toggle generated floor / combat regression arena |
| F2 | Toggle the generated full-level developer overview |
| Left/Right in overview | Browse the fixed representative configurations read-only |
| Home in overview | Return to the active generated layout |
| Escape/window close | Exit |

The runtime starts in the generated floor. It builds and retains a relaxed grid, exact dual geometry, neutral room graph, shooter layout, and doorway threshold segments in one immutable `GeneratedLevel`; renders every assigned dual-cell polygon in a cached floor mesh; walls floor/void boundaries and unauthorized room contacts; and omits only exact published doorway pairs. Mutable player, current-room, room-lifecycle, doorway-lock, and active-collision-wall state lives in `LevelSession`. The F2 developer overview renders exact floor triangles and room boundaries in fitted 2D, overlays the published room graph and threshold lock state, labels current baseline topology and room geometry, and can construct six fixed representative configurations as read-only previews while simulation is paused. Only configuration 1 is the active session; browsing never replaces it.

`GeneratedEncounterCoordinator` owns a promoted player attack state that keeps the generated player's weapon and projectile pool active during traversal as well as combat, with the same muzzle blocking, swept wall collision, lifetime, and reset rules in both states. It reacts when the session enters dormant `Combat` or `Hub` rooms, deterministically filters enemy-spawn candidates for room ownership, player occupancy/distance, scaled local clearance, static walls, retained doorway thresholds, and inter-enemy separation; starts up to three stable-ID enemies without discarding shots already in flight; locks every incident threshold; and updates the collection against `LevelSession::player()` and current active walls. Player projectiles choose the earliest swept enemy hit with stable identity as the exact-time tie-breaker. Defeating every enemy clears the room and reopens its doors. Player defeat freezes the locked encounter until `R`; reaching Exit marks the floor complete. Reset restores the Start spawn, player health, room lifecycle, doors, weapon/projectile state, encounter state, and completion state.

`F1` switches to the preserved hard-coded 20-by-20 combat regression arena. That path still contains the stationary target, deterministic hostile orb, separate projectile pools, swept wall and damage collision, defeat/victory freeze, restart, effects, HUD, and procedural tones described by the first-enemy milestone.

## Runtime architecture

```text
main.cpp
    ├── constructs immutable GeneratedLevel generation + geometry data
    ├── reads PlayerInput through game_input
    ├── advances generated room progression or combat-regression fixed state
    ├── interpolates simulation state for rendering
    └── asks PrototypeRenderer to draw

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

GeneratedEncounterCoordinator
    ├── owns persistent generated-player weapon + projectile state
    ├── advances firing and shots during traversal and combat
    ├── reacts to Combat/Hub entry and Exit entry
    ├── selects up to three deterministic stable-ID enemy spawns
    ├── updates enemies in identity order and resolves earliest swept hits
    └── drives lock → fighting → all-defeated → cleared and floor completion

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

Camera3D + GeneratedLevel + interpolated player ──→ PrototypeRenderer
Camera3D + Arena + interpolated combat state ──→ PrototypeRenderer
CombatStepResult / EncounterStepResult ──→ CombatAudio
```

### File map

| Path | Responsibility |
|---|---|
| `src/game/main.cpp` | Window lifetime, generated/combat view switching, 120 Hz accumulator, interpolation, audio-event forwarding, and composition |
| `src/game/generated_level.hpp/.cpp` | Immutable generator artifact package, exact floor triangles, closed walls, retained doorway thresholds, navigation, and Start spawn |
| `src/game/level_session.hpp/.cpp` | Mutable generated traversal, authoritative generated player, room lifecycle/location, doorway locking, active walls, effect timers, and reset |
| `src/game/generated_encounter.hpp/.cpp` | Persistent generated-player attack state, generated-room activation, deterministic multi-spawn filtering, collection combat/door lifecycle, defeat/reset, and Exit completion |
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
| `src/game/prototype_renderer.hpp/.cpp` | GPU resource ownership, runtime drawing, and the immediate-mode fitted full-level overview |
| `src/game/directional_shader.hpp` | Embedded GLSL and shared directional-light vector |
| `tests/generated_level_tests.cpp` | Artifact alignment, exact floor area, wall/door authorization, representative-browser validity, traversal firing/preservation, deterministic multi-spawn filtering/identity, partial/all-enemies clear transitions, hostile cleanup, defeat/reset/Exit, Start spawn, and reachability coverage |
| `tests/game_tests.cpp` | Headless dash/collision, caller-owned combat, projectile ownership/profile/pool, blocked muzzles, weapon, single-enemy and collection determinism/damage/defeat, earliest-hit/identity tie-breaking, closed-wall containment, player damage/death, interpolation freeze, victory, and restart coverage |
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

The camera uses orthographic projection with both a 45-degree elevation and diagonal heading. Movement is derived from the camera's planar forward/right vectors; do not restore a hardcoded isometric input matrix.

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

Every fixed projectile update copies `position` to `previousPosition` before advancing. `updateCombat()` updates the caller-owned player, resolves player-wall contacts, attempts player firing, advances player projectiles, resolves wall hits, advances/resolves the enemy, resolves enemy then optional regression-target hits, emits and advances hostile shots, resolves their wall hits, and finally resolves player damage. Outside an active generated encounter, `updateGeneratedPlayerWeapon()` performs the same fire → advance → wall collision → retirement sequence. Generated encounter activation replaces only the generated enemy collection and hostile projectile state; generated-player weapon cooldown and active shots survive the transition. Clearing an encounter likewise clears hostile shots without discarding player shots.

Wall-aware weapon and enemy-pattern overloads reject a muzzle path blocked by injected geometry while still consuming cooldown. A newly spawned projectile therefore has a valid muzzle-to-first-step segment immediately. Preserve that segment and keep wall collision before enemy/target/player collision; do not replace swept collision with a current-position overlap test.

### Target contract

The target is fixed at X/Z `(-5, -5)` with radius `0.85`, five health, a `0.16` second hit flash, and a one-second reset delay. `updateTarget()` calls the shared swept circle-versus-circle query for each active projectile, using the projectile pool profile's radius plus the target radius. Zero-length sweeps and initial overlaps are handled without division.

A hit immediately deactivates the projectile and removes one health. Processing stops when a hit defeats the target, so later slots in that fixed step remain active. While defeated, the target does not collide; it resets at the same position with full health after the delay. Target collision has no dedicated headless coverage by explicit request, so preserve and manually verify direct hits, fast crossing hits, misses, and single-hit behavior when changing it.

### Enemy and player-combat contract

The regression enemy starts at X/Z `(5, 5)`, has 20 health, moves at 2.4 units/second, circles counter-clockwise relative to the player, and adds a clamped radial correction toward a five-unit preferred distance. Its state contains no random source, and identical fixed-step inputs must produce identical movement and shots. Player hits use projectile motion relative to the enemy's previous/current positions, include both radii, consume the projectile, and remove one health. At zero health the regression encounter freezes in a victory state.

Generated encounters retain enemies in ascending stable-ID order; current IDs are their selected source cell indices. For each player projectile slot, collection damage selects the lowest swept contact amount across every living enemy and uses the lower stable ID only when contact amounts are exactly equal. A projectile damages at most one enemy. Generated combat clears only after the non-empty collection has no living members.

The player has five health. Hostile collision uses projectile motion relative to the player's previous/current fixed-step positions, includes both radii, deactivates a shot on contact even during invulnerability, and removes at most one health before starting 0.8 seconds of invulnerability. At zero player health the encounter freezes in a defeat state. A post-victory or post-defeat `restartPressed` input assigns a fresh `Encounter`, clearing both pools and restoring player, weapon, target, and enemy state exactly.

### Player ownership boundary

There are two deliberate player owners in mutually exclusive runtime views: `LevelSession` owns the generated-floor player, while the preserved regression `Encounter` owns its arena player. `PlayerAttackState` promotes weapon and player-projectile ownership independently of any enemy. Regression `CombatState` combines that attack state with one enemy and hostile pool; `updateCombat()` accepts a caller-owned `Player&` and injected walls. `Encounter` wraps it with its regression player, target, and whole-state restart semantics. `GeneratedEncounterCoordinator` instead combines its persistent attack state with a stable enemy collection and hostile pool, passing `LevelSession::player()` directly. The future horde, economy, device, and quest systems must continue to operate on the one authoritative `LevelSession` player rather than copying or synchronizing parallel player state.

### Arena contract

`ARENA_WALLS` contains four ordered X/Z segments forming a square from `-10` to `10` on each axis. Reusable `updateCombat()` requires an injected wall span; `updateEncounter()` preserves its injected-wall overload, while its three-argument regression overload forwards `ARENA_WALLS`. Player collision treats the player as a `PLAYER_RADIUS` circle, resolves contacts with four fixed passes, and projects away only velocity into each contact normal so tangential movement survives. The pre-movement player position selects the stable side of wall-face contacts.

Projectile collision uses the shared `collision_2d` queries to treat each projectile as a moving circle and test its full previous-to-current path against segment faces and endpoint circles. The query accepts the pool profile's radius, selects the earliest hit across all walls, and lets the arena resolver clip the position to contact and deactivate the projectile. Because the muzzle can extend beyond a wall while the player remains inside, projectile centers already outside the closed convex wall loop are also deactivated before they can escape. Wall meshes extend outward from the ordered arena segments, leaving each mesh's inner face aligned with its simulation segment.

### Input boundary

`updatePlayer()`, `updateCombat()`, and `updateEncounter()` must not call `IsKeyDown()`, `GetMousePosition()`, or other input APIs. Extend `PlayerInput`, then populate it in `readPlayerInput()`. Restart and dash are edge inputs, and `main.cpp` latches each until one fixed step consumes it. This keeps simulation code testable and leaves room for controller or replay input.

### Rendering boundary

Gameplay code does not own raylib `Model`, `Shader`, or `Sound` handles. `PrototypeRenderer` owns temporary runtime graphics resources, while `CombatAudio` owns the audio device and generated sounds. Projectiles expose stable simulation state to presentation modules rather than issuing draw or audio calls from simulation code.

The web build keeps raylib's framebuffer fixed at 1280 by 800 and lets `web/shell.html` scale that 16:10 canvas uniformly within the viewport. Do not enable `FLAG_WINDOW_RESIZABLE` on web: raylib otherwise sizes the framebuffer to the browser aspect ratio while CSS letterboxes the canvas, stretching the image and making GLFW mouse coordinates disagree with `GetScreenWidth()` and `GetScreenHeight()`.

### Procedural generation boundary

The game target links the generator libraries only through `GeneratedLevel`. The package retains `StalbergGrid`, `DualGrid`, `RoomGrid`, and `RoomLayout` together because exact floor polygons and doorway segments are not all present in `RoomLayout` alone. It also recovers and retains one `DoorwayThreshold` segment per published doorway. Its public API is read-only after construction.

Within one room, neighboring assigned cells are traversable. Across rooms, immutable navigation uses only exact cell pairs published by `RoomLayout::getDoorways()`; physical contact between regions is never automatically traversable. `LevelSession` owns mutable lock state and adds locked threshold segments to both its active collision walls and traversal checks. Connected exterior entrance cells remain enclosed because Exit completion currently ends the generated floor.

The active runtime currently uses the default `GeneratedLevelConfig`—radius 6, grid seed 1, and room seed 1—and the normal gameplay camera follows the player. F2 opens a complete developer overview with exact floor, room roles/IDs, baseline geometry labels, published graph, open/locked thresholds, role markers, player position, seeds, selected candidate, and quality score. Left/Right constructs one of six fixed read-only representative configurations; Home returns to the active layout. Do not confuse this visibility tool with structural variety: topology, published room grammar, and semantic landmark work still provide the actual identity.

## Next implementation slice: level identity and legibility

Continue the pass specified in [`level-identity-pass.md`](level-identity-pass.md) before adding more horde systems. Slice 1—the fitted developer overview and read-only deterministic representative browser—is complete.

1. Select an explicit topology archetype before arena placement and routing. Initial families are hub-and-spokes, ring-and-branches, main spine, twin districts, and dense-cluster-to-sparse-branch.
2. Extend validation and scoring to detect repetitive arena → connector → arena chains, excessive connector use, weak degree structure, missing direct arena adjacency, and archetype collapse across representative seeds.
3. Give substantial rooms a published geometry grammar—compact, elongated, L-shaped, concave pocket, twin-lobed, perimeter route, or crossroads—and validate their geometric signatures.
4. Publish deterministic semantic anchors and render role-, shape-, and district-aware landmarks. Preserve the rendering ownership boundary and never hard-code world coordinates into map content.
5. Add headless structural/diversity tests and a repeatable full-map screenshot matrix while preserving all current geometry, doorway, navigation, collision, encounter, and reset invariants.

Acceptance check: representative layouts are distinguishable at a glance in the full-level overview by silhouette, graph archetype, room shapes, and landmark hierarchy; none is dominated by alternating rooms and connectors; and each contains several mechanically distinct substantial rooms without weakening determinism or exact traversal.

After this gate, resume crowd navigation through the published doorway graph, local separation, deterministic spawn pacing, Drifter/Runner/Caster roles, and the buildup → peak → cleanup → intermission round director.

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

Doorway thresholds need states that distinguish sealed or purchasable gates, permanently opened routes, and temporary objective locks. Every state change must update rendering, collision, player traversal, and enemy navigation together. Waves operate across all currently opened rooms rather than treating each room as an isolated one-enemy encounter.

The published room roles provide map semantics: Start is the opening survival area, Hub owns the central machine, Combat rooms host training routes and holdouts, Connectors become chokepoints and trap sites, Reward rooms contain services or secrets, and Exit becomes the finale or extraction site. Curated generated seeds should be paired with authored map recipes that assign devices, clue families, enemy access, and wonder-weapon behavior without hard-coding world coordinates.

Common enemies should create crowd pressure through pursuit and interception. The existing deterministic orb enemy and projectile systems establish the ranged language for Casters, elites, and bosses. Bullet patterns must remain identifiable exceptions inside the horde rather than becoming undifferentiated projectile noise.

Main quest steps should communicate state through world geometry, animation, lighting, symbols, and audio. Optional Easter eggs can demand deeper observation, but every accepted action must produce persistent feedback. Puzzle verbs should remain physical and combat-linked: hold a zone, defeat enemies near a device, shoot or ricochet into targets, carry a component, lure an elite, or activate a discovered sequence.

Keep the `F1` hard-coded arena as the focused combat regression path. The destination does not require multi-floor progression, an ECS, a generic asset manager, save-anywhere support, or a general scripting system.

## Known limitations

- The active game session always uses the default radius-6, grid-seed-1, room-seed-1 level. The developer overview can browse six fixed read-only configurations, but cannot apply them to the active match. Current shooter candidates are primarily one spatial tree with at most one loop, require at least one connector, do not publish a topology archetype or room-shape grammar, and use only baseline-derived labels and modest role colors for runtime identity. The resulting maps can read as repetitive arena/connector chains with visually similar rooms.
- Each generated `Combat` or `Hub` room starts up to three deterministic stable-ID enemies. There is no crowd navigation, local separation, wave director, spawn pacing, or generated target. The player dash and always-available generated-map firing are complete, while the stationary target remains regression-arena-only.
- Every published doorway starts open and can only be temporarily encounter-locked; there are no purchasable gates, match currency, powered rooms, services, traps, quests, Easter eggs, wonder weapons, boss, extraction, or endless rounds yet.
- Exit currently marks the generated floor complete instead of acting as a finale or extraction site.
- Lighting is diffuse-only and the player shadow is a projected decal rather than general occlusion.
- The combat regression ground and debug grid cover a finite 80-by-80 area.
- Gameplay constants are compiled into their owning modules.
- Generated-level tests cover retained artifact alignment, exact assigned-floor area, wall/door authorization, doorway clearance, deterministic multi-spawn filtering and identities, traversal firing and in-flight-shot preservation, Start behavior, complete door-aware reachability, generated encounter activation/locking/partial defeat/all-enemies clearing/defeat/reset/Exit completion, session lifecycle, and dynamic doorway collision/navigation locking. Combat tests cover caller-owned combat, injected encounter walls, reusable swept-circle queries, dash activation/cooldown/wall collision, profile-driven projectile movement/lifetime/radius, pool ownership/exhaustion/reuse, blocked and valid muzzle spawning, fire cadence, deterministic enemy and collection behavior, earliest enemy hit and identity tie-breaking, simultaneous enemy defeat, enemy damage/defeat/victory restart, player damage/invulnerability/death/restart, player-wall faces/endpoints/corners/sliding, projectile wall faces/endpoints/earliest hits, and outside-muzzle rejection. Target collision, free player movement, audio, and rendering lack dedicated tests.
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
