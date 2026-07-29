# Game Roadmap

This document tracks the evolution of the procedural-generation demo into a top-down 2.5D round-based horde shooter. The finished game builds a fresh, replayable-seed procedural map for each new match and keeps that map persistent for the life of the run: the player survives escalating waves, earns currency, opens routes, powers strange machinery, discovers hidden quests, acquires transformative weapons, and reaches a final confrontation or extraction. Restarting a run keeps its map seed; starting a new match requests a new one.

Bullet-hell combat remains part of the identity, but as punctuation rather than the entire game. Most enemies create a moving crowd that the player must route through the map; ranged enemies, elites, objectives, and bosses introduce readable projectile patterns that disrupt comfortable strategies.

For the current implementation boundary and technical invariants, see [`game-handoff.md`](game-handoff.md).

## Game concept

### Player fantasy

The player enters a dormant geometric fortress with a basic weapon and limited access to the map. Each round wakes more of the structure and sends a larger, more varied horde through its rooms. Fighting generates the currency needed to open gates, activate devices, improve weapons, and take tactical risks.

The map is both an arena and a machine. Its routes, chambers, symbols, powered thresholds, and hidden mechanisms become understandable over repeated attempts. A successful match is not only about surviving damage; it is about learning how the place works, choosing an efficient route through it, assembling enough power to face its final threat, and deciding whether to extract or continue into endless waves.

### Core pillars

#### Crowd movement

- Movement, aiming, and firing remain independent twin-stick actions.
- A short, responsive dash gives the player an intentional way through closing gaps.
- Common enemies form crowds that can be gathered, redirected, split at doorways, and punished with area attacks.
- Wide rooms support circular training routes; connectors and narrow doors create dangerous chokepoints.
- Collision, navigation, and telegraphs remain readable under the tilted orthographic camera.

#### Pressure and release

- Combat is organized into explicit rounds.
- Each round moves through build-up, peak pressure, cleanup, and respite rather than sustaining maximum intensity continuously.
- Intermissions provide time to explore, spend currency, read clues, and prepare devices.
- Objectives can deliberately interrupt the normal rhythm with holdouts, escorts, traps, or special enemy compositions.
- A short fixed intermission advances automatically so exploration and puzzle work happen under predictable pressure.

#### A map that opens over time

- The match takes place on one persistent generated layout rather than a sequence of disposable floors.
- Doorway thresholds become visible gates with sealed, purchasable, open, and objective-locked states.
- Currency creates a recurring choice between opening safer or more profitable routes and buying immediate combat power.
- Start, Hub, Combat, Connector, Reward, and Exit rooms become recognizable gameplay landmarks.
- Power activation changes navigation, lighting, available services, enemy access, and the state of the central Hub.

#### Discovery and Easter eggs

- Every map has a readable main objective that can be understood from environmental feedback.
- Optional side quests are more cryptic and reward observation, experimentation, and community discovery.
- Puzzle actions reuse the game's physical verbs: fight near a device, hold a zone, shoot a distant target, ricochet through a marked angle, lure an elite, carry a component, or activate mechanisms in a discovered order.
- Every successful step gives persistent visual and audio confirmation.
- Secrets reward distinctive weapons, shortcuts, challenge waves, alternate finales, audiovisual changes, or unusual combat modifiers rather than minor stat increases alone.

#### Bullet hell as escalation

- The base horde is dominated by contact pressure and movement denial, not projectile spam.
- Ranged enemies add aimed spreads that force the player to leave established routes.
- Elites use strongly telegraphed attacks with deliberate safe gaps.
- Boss phases temporarily reshape safe space with radial patterns, sweeping lanes, hazards, and summoned crowds.
- Wonder weapons let the player answer density with effects such as ricochets, piercing, chain lightning, gravity, freezing, or explosive propagation.

## Match structure

A complete match follows this arc:

1. **Awakening** — The player begins in the Start region, learns the current map's immediate routes, and survives the first rounds with basic equipment.
2. **Expansion** — Kills fund permanent gate openings. The player chooses which branches, arenas, services, and clues to expose first.
3. **Power** — Distributed Grid Anchors are activated through combat holdouts. Each one powers a local benefit and visibly changes the central Hub.
4. **Investigation** — Powered rooms reveal the main quest, optional sequences, hidden targets, components, and unusual interactions.
5. **Transformation** — Weapon upgrades, perks, traps, and quest rewards allow the player to control denser crowds and specialized enemies.
6. **Confrontation** — A late round introduces the map's Warden or boss and resolves the main objective under peak pressure.
7. **Extraction or descent** — The player can secure a completed match or continue into increasingly hostile endless rounds for score and mastery.

Death ends the run. Restart restores economy, devices, enemies, and quest state while retaining the exact accepted map and match seed; choosing New Match generates another map.

## Map and quest model

The generator remains the spatial compiler. Normal play should choose a fresh match seed, deterministically derive grid and room seeds plus a compatible topology/quest recipe, and retry candidates until the complete result is valid. The accepted map is reproducible from its displayed seed, but its geometry, routes, room shapes, and quest sites vary between new matches. Curated seeds are regression fixtures and an emergency fallback only.

Recipes preserve coherent mysteries without fixing layouts: they assign landmarks, devices, clue families, enemy access, and finale behavior to generated semantic room roles and anchor types rather than room IDs or world coordinates. Quest-aware validation rejects maps that cannot realize required separation, route order, objective clearance, holdout capacity, or optional branches.

A representative map recipe is **The Grid Engine**:

- Three Grid Anchors occupy separated Combat rooms.
- Activating an anchor begins a local holdout and powers a nearby service or shortcut.
- Each anchor reveals a persistent glyph and tone.
- Powering all anchors wakes the machine in the Hub, enables the primary weapon forge, and arms the finale.
- The Hub contains clues connecting the glyphs to an optional activation or shooting sequence.
- Solving that sequence opens a Reward chamber containing a geometry-driven wonder weapon, such as a projectile that gains power after ricocheting from a wall.
- The final Warden uses the powered doorway network and projectile patterns, making the player's accumulated map knowledge relevant to the fight.

Main objectives should remain achievable without external instructions. Side Easter eggs may be obscure, but they must be internally consistent and provide enough feedback for players to form and test hypotheses.

## Enemy ecology

The horde is composed from roles that change how the player uses space:

- **Drifters** are numerous melee pursuers that form the body of a train.
- **Runners** are fragile, fast interceptors that close open gaps and punish straight-line retreat.
- **Bulwarks** are slow bodies that split crowds, absorb frontal fire, and make chokepoints less reliable.
- **Casters** preserve the current ranged-combat lineage by firing readable patterns through or around the crowd.
- **Saboteurs** pressure powered objectives, traps, or open routes and force movement across the map.
- **Wardens** are periodic elites with telegraphed attacks and map-specific interactions.
- **Bosses** combine crowd control, projectile patterns, objective states, and environmental hazards rather than acting as isolated health bars.

Enemy composition, spawn direction, and pacing vary by round, but the simulation remains deterministic for a given map and match seed.

## Economy and power

The economy exists to create route and timing decisions, not to produce several interchangeable currencies.

- Defeating enemies awards the primary match currency.
- Gates, weapon upgrades, perks, traps, ammunition services, trials, and objective devices compete for that currency.
- Permanent gate openings alter both player movement and enemy navigation.
- Fixed-location services reward map knowledge; selected rewards may move among authored candidate locations.
- Temporary power-ups create urgent movement decisions during a wave.
- Perks and weapon transformations change behavior—dash recovery, piercing, crowd control, ricochet, area damage, or emergency escape—not only damage percentages.

The map should remain survivable without finding every secret, while knowledge and execution let experienced players reach power earlier and take on harder optional content.

## Current foundation

### Milestones 1–5: combat foundation — complete

The hard-coded regression arena establishes:

- Camera-relative movement and mouse aiming on the XZ gameplay plane.
- A tilted orthographic follow camera and fixed 120 Hz simulation with interpolation.
- Pooled player and enemy projectiles with swept collision.
- Player, target, enemy, health, damage, invulnerability, defeat, victory, and restart state.
- Arena-wall collision, smooth wall sliding, blocked-muzzle handling, effects, audio, and headless combat coverage.

Acceptance check: the regression arena supports deterministic movement, firing, damage exchange, terminal states, and restart without projectile tunneling or wall escape.

### Milestone 6: generated level runtime — complete

The runtime packages the relaxed grid, exact dual geometry, room graph, room layout, floor triangles, walls, doorway thresholds, Start spawn, and door-aware navigation in an immutable `GeneratedLevel`. A mutable `LevelSession` owns the generated player, room lifecycle, active walls, and doorway locks.

Acceptance check: the player can traverse every authorized part of a generated level without crossing closed contacts or leaving the exact floor.

### Milestone 7: generated encounters — complete

The original generated-encounter slice activated one deterministic enemy in entered Combat and Hub rooms, locked incident thresholds during combat, reopened them on clear, preserved player health, handled defeat/reset, and marked the current floor complete on entering Exit. Milestone 8 later replaced that single-enemy assumption with the current stable collection. The hard-coded arena remains available through **F1** as a regression path.

Acceptance check: a generated floor can be entered at Start, cleared room by room, and completed at Exit with collision, navigation, rendering, and room lifecycle kept synchronized.

## Planned milestones

### Milestone 8A: horde combat foundation — complete

- A short cooldown-based dash and always-available generated-map firing work during traversal and encounters.
- Generated encounters spawn up to three simultaneous enemies with deterministic identities, spawn/update order, earliest swept-hit selection, exact-time identity tie-breaking, whole-match reset, and all-enemies-clear transitions.
- Player shots survive encounter activation and clearing; hostile shots and doorway locks retain atomic cleanup behavior.

Acceptance check: several generated enemies coexist and can be defeated deterministically in one locked room while the player moves, dashes, and fires without crossing walls or losing shots at encounter transitions.

### Milestone 8B: level identity, physical scale, and legibility — in progress

The current generator is physically valid but often reads as an alternating arena/connector chain, substantial rooms share similar compact growth, and the runtime follow camera hides the complete layout. The active radius-5 preset also converts generated geometry with a `0.16` world scale, producing a systems-test footprint rather than the intended large fortress. Complete the remaining pass in [`level-identity-pass.md`](level-identity-pass.md) before deeper puzzle content.

- **Complete:** add a fitted full-level runtime overview and read-only deterministic representative-seed browser.
- **Complete for small maps:** select and publish Hub Circuit, Broken Ring, and Twin Wings gameplay recipes before candidate placement and routing.
- **Next for normal play:** generate a fresh validated map from a replayable match seed instead of selecting a fixed preset.
- Extend archetypes and room-shape grammar beyond the radius-5 vertical slice.
- Increase both map extent and the world-space size of cells, rooms, connectors, and travel routes relative to unchanged actor bodies; a larger radius alone is insufficient.
- Penalize repetitive room/connector alternation and weak graph signatures.
- Add role-, shape-, district-, and quest-driven semantic anchors and landmarks.
- Add per-layout and cross-seed structural diversity tests plus repeatable overview screenshots.
- Validate generation in gameplay units: actor clearance, ingress capacity, objective footprint, sightline range, traversal time, and projectile/camera suitability.

Acceptance check: consecutive new matches normally produce different valid maps, entering the same match seed reproduces the same map and quest binding, and restart preserves that seed. Accepted layouts are distinguishable by silhouette, graph structure, room-shape distribution, and landmark hierarchy; their rooms and routes are measurably larger relative to actors than the systems slice; and all deterministic geometry, doorway, navigation, combat, and reset contracts remain correct.

### Milestone 8C: horde combat continuation — endless small-map slice complete

- The radius-5 vertical slice has crowd pursuit through exact opened doorway cells, local separation, wall-safe steering, deterministic spawn control, Drifter/Runner/Caster/Elite roles, and automatic countdown → buildup → peak → cleanup → timed-intermission rounds.
- A recipe-aware profile with tier-0 baseline plus ten escalation tiers bounds spawn budget, simultaneous pressure, composition, pacing, health, movement, hostile projectile speed, firing cadence, damage, and rewards. Every fifth round schedules an Elite event, and overflow-safe schedules remain deterministic at the maximum round index.
- Continue tuning route caching, crowd readability, spawn visibility, bespoke bosses, and larger-map population pressure after the small-map recipe gate.

Acceptance check: generated rooms and connectors support readable crowd movement, deliberate dodging, and escalating round pressure without enemies crossing closed geometry.

### Milestone 9: persistent map progression — endless small-map vertical slice complete

- The small-map match advances endlessly without input, with one point currency, recipe-scaled permanently purchasable exact-threshold gates, synchronized collision/navigation, pressure-scaled rewards, three tiers each of damage/fire-rate/dash upgrades, and repeatable pressure-scaled health repair at the powered Hub.
- Additional fixed services, placement variants, and traps remain broader-milestone work beyond the implemented Hub repair.
- Turn the generated role/shape/district identity established in Milestone 8B into persistent economy, service, trap, and objective locations rather than one-time room-clear labels.
- Let waves and enemies move across every currently opened part of the map.

Acceptance check: spending, route choice, and gate state materially change both survival strategy and enemy flow throughout a match.

### Milestone 10: the living map — basic objective slice complete, puzzle depth pending

- The small-map recipe includes one Anchor holdout, Hub activation, an objective-locked Exit, contextual prompts, persistent feedback, and an optional ordered three-relay Reward sequence that grants fire rate.
- The required Anchor is explicitly a concurrent pressure objective—remain in its gold ring for eight accumulated seconds during active combat rounds—not a logic puzzle. It never authorizes or pauses the director. The optional relay exposes the next correct target and is intentionally shallow.
- The next puzzle slice must add a readable multi-step dependency with clues and meaningful failure/recovery rather than relabeling another holdout as a puzzle.
- Multiple distributed Anchors, deeper Easter eggs, traps, authored clue families, and a geometry-driven wonder weapon remain future expansion.

Acceptance check: players can understand and complete the main objective from in-game evidence, while optional secrets reward deeper observation and experimentation.

### Milestone 11: complete match arc — endless director foundation complete

- **Complete:** automatic endless continuation, deterministic bounded pressure scaling, explicit extraction after the current Anchor/Hub objective, death, and deterministic restart.
- Expand round composition across common enemies, specials, elites, mutation events, and challenge waves.
- Add a Warden or boss that combines horde pressure, projectile patterns, and powered map mechanics.
- Add score and replace hard-coded Round 2/5 objective checks with recipe-authored requirements.
- Balance map expansion, economy, quest timing, and combat power across the full match.

Acceptance check: the game supports a complete round-based horde match with a beginning, expanding tactical possibilities, discoverable objectives, a finale, and reasons both to replay a known seed and to explore a newly generated map.

### Milestone 12: procedural mastery and presentation

- Ship a bounded random-map pipeline that selects validated generated layouts and dynamically binds authored recipe families; preserve explicit seed replay and fixed diagnostic fixtures.
- Add alternate routes, optional challenges, hidden audiovisual events, and multiple finale conditions that remain solvable across accepted random layouts.
- Complete controller support, accessibility options, visual telegraphs, combat audio, and map-state presentation.
- Evaluate cooperative play only after the solo simulation, content, and readability remain strong at full physical map scale.

Acceptance check: each new match is a mechanically viable procedural variation, its seed reproduces geometry and quest binding exactly, and repeated matches remain readable and distinctive while preserving the shared horde, economy, quest, and combat rules.

## Deferred or excluded systems

- Multi-floor roguelite progression; the intended match is built around one persistent map.
- ECS or generic scene graph.
- A general-purpose scripting language before map and quest rules require one.
- General-purpose asset management unrelated to shipped content.
- Permanent stat-grind meta-progression as a substitute for map knowledge or combat mastery.
- Save-anywhere support.
- Real-time shadow maps.
- Infinite or chunked procedural terrain.
- Competitive multiplayer.
- Network and replay protocol guarantees before the solo match is complete.

## Engineering gates

After every change:

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For graphical smoke testing without a visible desktop:

```sh
xvfb-run -a ./build/stalberg_game
```

For interactive testing on the local X display used during development:

```sh
DISPLAY=:0 ./build/stalberg_game
```
