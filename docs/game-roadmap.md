# Game Roadmap

This document tracks the evolution of the procedural-generation demo into a top-down 2.5D round-based horde shooter. The finished game is built around one persistent, learnable map per match: the player survives escalating waves, earns currency, opens routes, powers strange machinery, discovers hidden quests, acquires transformative weapons, and reaches a final confrontation or extraction.

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
- Players can accelerate quiet periods when they are ready for the next round.

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

Death ends the current match and restores the map, economy, devices, enemies, and quest state to a deterministic starting condition.

## Map and quest model

The generator remains the spatial compiler, but shipped maps should be learnable. The intended content model is a set of curated and validated generated seeds, each paired with a map recipe that assigns landmarks, devices, clue families, enemy access, and finale behavior to semantic room roles. Geometry can remain irregular and generated without randomizing away the relationships that make a mystery solvable.

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

### Milestones 1–5: combat prototype — complete

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

### Milestone 8B: level identity and legibility — next

The current generator is physically valid but often reads as an alternating arena/connector chain, substantial rooms share similar compact growth, and the runtime follow camera hides the complete layout. Complete the pass in [`level-identity-pass.md`](level-identity-pass.md) before adding more crowd systems.

- Add a full-level runtime overview and deterministic representative-seed browser.
- Select and publish strong map-level topology archetypes before physical routing.
- Penalize repetitive room/connector alternation and weak graph signatures.
- Add explicit room-shape grammar and geometry validation.
- Add role-, shape-, and district-driven semantic anchors and landmarks.
- Add per-layout and cross-seed structural diversity tests plus repeatable overview screenshots.

Acceptance check: representative layouts are distinguishable at a glance by silhouette, graph structure, room-shape distribution, and landmark hierarchy; no representative layout is dominated by repetitive arena/connector alternation; and all existing deterministic geometry, doorway, navigation, combat, and reset contracts remain correct.

### Milestone 8C: horde combat continuation — planned

- Add crowd pursuit and pathfinding through the published room/door graph.
- Add local separation, wall-safe steering, and deterministic spawn control.
- Add crowd-readable hit, death, and threat feedback.
- Introduce Drifter, Runner, and Caster roles.
- Organize combat into rounds with build-up, peak, cleanup, and intermission states.

Acceptance check: generated rooms and connectors support readable crowd movement, deliberate dodging, and escalating round pressure without enemies crossing closed geometry.

### Milestone 9: persistent map progression

- Keep one generated map active for the complete match.
- Turn doorway thresholds into purchasable gates whose state affects collision and navigation for players and enemies.
- Add the primary match currency, fixed services, traps, and combat upgrades.
- Turn the generated role/shape/district identity established in Milestone 8B into persistent economy, service, trap, and objective locations rather than one-time room-clear labels.
- Let waves and enemies move across every currently opened part of the map.

Acceptance check: spending, route choice, and gate state materially change both survival strategy and enemy flow throughout a match.

### Milestone 10: the living map

- Add distributed Grid Anchors, powered-room state, and a central Hub machine.
- Add readable objective prompts through world animation, lighting, symbols, and audio.
- Add holdouts and other combat-driven interactions that alter the map.
- Add a main quest, optional side Easter eggs, and persistent quest feedback.
- Add the first wonder weapon whose behavior uses the map's irregular geometry.

Acceptance check: players can understand and complete the main objective from in-game evidence, while optional secrets reward deeper observation and experimentation.

### Milestone 11: complete match arc

- Expand round composition across common enemies, specials, elites, and challenge waves.
- Add a Warden or boss that combines horde pressure, projectile patterns, and powered map mechanics.
- Add extraction, victory, endless continuation, score, death, and deterministic restart.
- Balance map expansion, economy, quest timing, and combat power across the full match.

Acceptance check: the game supports a complete round-based horde match with a beginning, expanding tactical possibilities, discoverable objectives, a finale, and a reason to replay the same map with greater knowledge.

### Milestone 12: maps, mastery, and presentation

- Use the seed browser and structural-diversity baseline from Milestone 8B to curate shipped seeds, then pair them with authored map recipes, finished themes, quests, enemy mixes, and wonder weapons.
- Add alternate routes, optional challenges, hidden audiovisual events, and multiple finale conditions.
- Complete controller support, accessibility options, visual telegraphs, combat audio, and map-state presentation.
- Evaluate cooperative play only after the solo simulation, content, and readability remain strong at full match scale.

Acceptance check: each shipped map is recognizable, learnable, replayable, and mechanically distinct while preserving the shared horde, economy, quest, and combat rules.

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
