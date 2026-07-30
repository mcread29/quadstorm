# Procedural Quest Recipe Design Guide

This guide defines how quests should be authored, presented, bound to generated levels, validated, and eventually generated. It expands the quest-realization phase in [`level-generation-lock-in.md`](level-generation-lock-in.md).

The near-term goal is a curated library of strong, multi-stage puzzle recipes whose rooms, routes, devices, clues, rewards, and optional discoveries are placed procedurally. The long-term goal is to generate complete quest graphs from validated authored motifs without rewriting the runtime.

## Experience goal

A quest should feel like a mechanism embedded in a fortress, not a task list placed on top of a random map.

The player should:

- Discover dormant machinery, strange symbols, blocked routes, and reactive objects while moving through the level.
- Infer relationships from repeated visual, spatial, and audio language.
- Complete multi-stage objectives during uninterrupted horde rounds.
- Cause persistent, visible changes to landmarks and circulation.
- Find optional discoveries that reward attention and exploration.
- Receive local interaction help without being told the complete solution.

The level generator and quest binder must cooperate. A good dependency graph bound to poor geometry is not a good quest, and an interesting room graph with arbitrary objective placement is not a good generated level.

## Presentation philosophy: assistance without signposting

### The directive

The directive communicates only broad state and major consequences. It should not name the next room, draw a route, enumerate undiscovered stages, or reveal a sequence.

Good directive messages:

- `The fortress is dormant.`
- `Power is reaching the outer district.`
- `Something answered beneath the Hub.`
- `An extraction signal is available.`

Messages to avoid:

- `Go to the eastern Combat room.`
- `Activate relay 2 of 3.`
- `The next symbol is the triangle.`
- `Follow the marker to the Anchor.`

The F2/F3 developer views may expose the complete binding for debugging. Normal play should not.

### Local tooltips

Tooltips appear only when the player is near an interactable and, where appropriate, has line of sight to it. They explain the available verb or an immediate prerequisite, not the puzzle solution.

Examples:

- `E — Inspect dormant terminal`
- `E — Insert charged cell`
- `Hold E — Redirect power`
- `Requires a matching sigil`
- `The mechanism rejects this sequence`
- `Resonates when enemies fall nearby`

An undiscovered object receives no global marker. A discovered object may retain a subtle world-state treatment, but it should not become a permanent navigation arrow by default.

### Environmental communication

Related quest elements should share a generated presentation language:

- A symbol family.
- A color or light rhythm supported by shape and sound.
- Cable, conduit, beam, or floor-trace connections.
- A repeated mechanical silhouette.
- A distinct activation tone.
- Persistent states for dormant, discovered, partially complete, failed, and complete.

Color alone is insufficient. The same relationship should remain readable through symbol, placement, animation, or audio.

### Discovery record

If a player-facing record is added, it should contain only discovered facts: copied symbols, heard tone patterns, inspected device text, and visible machine reactions. It should not convert those facts into an explicit ordered checklist.

## Three graphs, kept separate

Quest realization involves three related graphs.

### 1. Spatial circulation graph

This is the generated network of substantial rooms, connectors, and authorized doorway thresholds. It determines routes, cycles, dead ends, gates, shortcuts, and enemy navigation.

### 2. Semantic-anchor graph

This describes where meaningful content can safely exist within the generated geometry. Anchors may include:

- High-clearance focal points.
- Doorway-facing device sites.
- Perimeter clue sites.
- Holdout footprints.
- Shootable target positions.
- Shortcut controls.
- Central obstruction sites.
- Passageway loot alcoves.
- Hidden or partially occluded discovery sites.
- Landmark and presentation attachment points.

Anchors publish capabilities and measurements. They do not decide which quest uses them.

### 3. Quest dependency graph

A recipe is a directed acyclic graph of discoveries, requirements, actions, state changes, optional branches, and rewards. Its nodes refer to semantic requirements rather than room IDs or world coordinates.

The quest binder maps this dependency graph onto the semantic-anchor and circulation graphs. If no coherent binding exists, the complete map candidate is rejected.

## Curated recipes now, generated recipes later

The first implementation should curate complete dependency graphs. A curated recipe defines intentional puzzle logic but leaves realization variable.

A recipe should not hard-code:

- Room IDs.
- Cell IDs or world coordinates.
- A fixed east/west orientation.
- One exact path through the level.
- Specific weapon instances or random rewards.
- Round 2 or Round 5 as universal progression rules.

For each match, deterministic binding may vary:

- Which compatible rooms host stages.
- Which anchors host devices and clues.
- Symbol, tone, or light vocabulary.
- The required order of equivalent devices.
- Which valid parallel branch is encountered first.
- Shortcut and optional-branch placement.
- Passageway supplies and final reward variants.

This progression preserves the runtime architecture:

1. Curated graph with generated placement.
2. Curated graph with generated clue, order, branch, and reward variants.
3. Generated graph assembled from curated motifs.
4. Fully generated and validated dependency graph plus placement.

Generated recipes should eventually produce the same declarative data consumed by curated recipes. Runtime quest code should not need a second execution path.

## Recipe model

Each recipe should publish the following information.

### Recipe-level fields

- Stable recipe and version identifier.
- Main and optional dependency graphs.
- Required topology capabilities.
- Minimum and maximum stage counts.
- Required interaction verbs.
- Clue and presentation families.
- Gate and shortcut effects.
- Reward categories and fallback rewards.
- Allowed recovery policies.
- Complexity or implementation tier.

### Stage-level fields

- Stable stage identifier.
- Prerequisite stages.
- Whether prerequisites use `all`, `any`, an ordered sequence, or a configured count.
- Interaction verb and completion condition.
- Room-role and room-shape requirements.
- Route-order, distance, visibility, clearance, and separation constraints.
- Whether the stage is hidden, discoverable, active, optional, or complete.
- Persistent world feedback for each state.
- Failure and recovery behavior.
- Outputs such as power, clues, gates, shortcuts, rewards, or extraction availability.

### Generic interaction verbs

The initial executor should prefer reusable verbs:

- Inspect a device or clue.
- Activate or hold an interaction.
- Hold a zone while combat continues.
- Kill enemies near a device or inside a region.
- Shoot a target or ordered target sequence.
- Power or redirect a route.
- Return to an altered landmark.
- Install or carry a component when that system is introduced.
- Lure a normal enemy, crowd, or Elite into a readable region.
- Use a compatible weapon property when weapon classification exists.

A recipe combines these verbs; it should not implement its own round director, enemy simulation, interaction input, or navigation system.

## Binding pipeline

One match seed deterministically derives the geometry inputs, circulation brief, room-shape briefs, semantic anchors, selected recipe, binding variants, rewards, and bounded retry sequence.

Recommended binding order:

1. Generate and validate the circulation graph.
2. Generate room-shape and combat briefs.
3. Publish semantic, presentation, and loot anchors.
4. Select a compatible curated recipe from the match seed.
5. Select deterministic recipe variants, such as symbol vocabulary and device order.
6. Bind mandatory stages in dependency and route order.
7. Bind optional branches and their rewards.
8. Bind gates and shortcuts to exact doorway thresholds.
9. Populate ordinary loot sockets around the completed quest binding.
10. Validate every progression component and reject an impossible candidate.
11. Persist the complete binding and retry metadata for replay and tests.

Quest selection must happen early enough that candidates cannot accept geometry incapable of realizing the selected recipe. Candidate scoring must not silently switch to an easier recipe.

## Binding and acceptance rules

Every accepted quest must satisfy the following.

### Solvability

- Every mandatory stage is reachable when its prerequisites can be satisfied.
- No required item can spawn behind the gate that consumes it.
- A stage cannot permanently invalidate another required stage.
- Extraction or another declared finale remains possible after completion.
- Failure-capable stages provide a recovery rule.

### Spatial meaning

- Devices are separated enough to require navigation and observation.
- Route ordering supports the dependency graph without excessive Hub backtracking.
- A shortcut saves a measured amount of travel and changes a useful route.
- Optional branches occupy genuinely optional space.
- A dead end used by the quest or loot has a clear payoff.

### Combat safety and pressure

- Holdouts have sufficient footprint, ingress, spawn capacity, and readable entry space.
- Shootable targets preserve firing lanes and do not overlap doors or walls.
- Device interactions do not require standing inside an unavoidable spawn point.
- Late-stage objectives support the scheduled enemy population.
- Puzzle state never pauses or authorizes automatic rounds.

### Legibility

- Necessary clues exist before their answer is required.
- Related objects share a consistent presentation family.
- Every accepted action produces persistent feedback.
- Wrong actions communicate failure without requiring arbitrary repetition.
- Mandatory progression never depends on finding an untelegraphed random drop.

### Determinism

The same match seed must reproduce:

- Recipe and version.
- Stage graph and selected variants.
- Room and anchor binding.
- Clue vocabulary and order.
- Gates, shortcuts, and reward category.
- Retry count and rejection metadata.

## Persistent state and automatic rounds

Quest state is concurrent with the endless director.

- Round requirements may make an interaction available; they never hold an intermission open.
- Incomplete stages persist across any number of rounds.
- Combat-linked progress explicitly persists, pauses, or resets according to the recipe.
- Hostile cleanup must not erase quest devices, clues, carried components, or completed state.
- Quest completion may enable extraction, a major reward, a route change, or a pressure change, but must not silently stop spawning.
- Extraction remains a voluntary interaction.

The current hard-coded Round 2 Anchor and Round 5 Exit checks should become recipe-authored requirements or be removed where the selected recipe does not need them.

## Passageways, supplies, weapons, and upgrades

Connectors should be useful resource routes rather than empty traversal tubes. The generator should publish passageway loot sockets with clearance, visibility, concealment, route-stage, nearby-door, and risk measurements.

### Content categories

Depending on the eventual economy, sockets may contain:

- Weapon pickups or weapon-swap stations.
- Damage, fire-rate, dash, health, or armor upgrades.
- Temporary speed, shield, damage, piercing, or crowd-control boosts.
- Healing, armor, and currency caches.
- One-use explosives, mines, decoys, or deployable turrets.
- Upgrade, reroll, or service tokens.
- Quest components and clue fragments.
- Breakable or inspectable secret caches.
- Risk/reward shrines.
- Ammunition if an ammunition economy is introduced.

### Distribution rules

Placement is random only within deterministic constraints.

- The opening component receives enough basic survival resources.
- Dangerous, expensive, or long routes may receive better reward weights.
- Dead ends justify themselves with loot, a clue, a service, or a secret.
- Shortcuts usually contain utility rather than mandatory progression.
- Mandatory quest components use guaranteed recipe placement, not the ordinary loot roll.
- Ordinary loot is populated only after quest binding so it cannot occupy required anchors.
- Items remain clear of doorway reading space, gate collision, objective footprints, and primary crowd lanes.
- The binder limits clustering so one lucky corridor does not contain the entire map economy.
- The same match seed reproduces all placements.

## Recovery patterns

Multi-stage puzzles need explicit recovery instead of accidental soft locks.

Useful policies include:

- **Persist:** completed work remains complete permanently.
- **Pause:** accumulated progress stops while a condition is false and resumes later.
- **Reset stage:** only the active local sequence resets.
- **Re-arm:** the player performs a short interaction before retrying.
- **Respawn component:** a lost mandatory component returns to its last safe socket.
- **Alternate proof:** a failed optional challenge can be replaced with a more expensive combat or currency requirement.

Whole-quest reset should be rare in an endless horde match.

# Ten curated quest recipes

The recipes below are authored dependency graphs intended for procedural binding. Names, symbols, rooms, directions, device counts within stated bounds, and rewards may vary by seed.

## 1. Awaken the Fortress

[Open quest graph](diagrams/quest-recipes/01-awaken-the-fortress.html)

**Identity:** The player reconstructs the purpose of a dormant central machine, completes two parallel combat trials, and brings the transformed fortress online.

**Primary novelty:** A clue-driven ordered activation opens into parallel objectives that reconverge at a newly exposed target.

```text
Discover dormant machine
        ↓
Find and interpret clue fragments
        ↓
Power two devices in the inferred order
       ↙ ↘
Hold a resonant zone   Kill enemies near a conduit
       ↘ ↙
Restore the shortcut network
        ↓
Expose and shoot the hidden target
        ↓
Return to the transformed machine
        ↓
Enable voluntary extraction
```

**Realization requirements:**

- A memorable Hub or focal landmark visible from multiple approaches.
- Two separated clue sites available before ordered activation is required.
- Two combat-capable rooms on different branches or sides of a useful cycle.
- A locked shortcut whose opening saves meaningful return travel.
- A target anchor visible only after the parallel trials complete.

**Clue language:** Two or three symbols appear on environmental fragments and on the corresponding power devices. Inspecting the Hub establishes that order matters but does not state the order.

**Recovery:** A wrong activation resets only the two-device sequence. Holdout progress pauses when the player leaves. Conduit kills persist.

**Optional branch:** A third, damaged fragment reveals a Reward-room cache after the shortcut opens.

**Reward:** Extraction plus one major upgrade or weapon choice at the transformed Hub.

## 2. Broken Circuit

[Open quest graph](diagrams/quest-recipes/02-broken-circuit.html)

**Identity:** A power network is active but routing energy into a damaged branch. The player traces the fault, isolates it, repairs two endpoints, and safely reconnects the network.

**Primary novelty:** Diagnosis through comparing network behavior rather than following a prescribed device order.

```text
Inspect unstable network terminal
        ↓
Pulse the network from two test points
        ↓
Identify the branch with the mismatched response
        ↓
Isolate the faulty branch
       ↙ ↘
Defend repair endpoint A   Clear corruption near endpoint B
       ↘ ↙
Reconnect the branch
        ↓
Choose power destination: shortcut or armory
        ↓
Stabilize the remaining destination through an optional challenge
```

**Realization requirements:**

- A junction room with three or more readable outgoing routes.
- Two test devices and one faulty endpoint in distinct route directions.
- A defendable repair site and a separate kill-near-device site.
- Two meaningful power destinations, neither required to be a room leaf.

**Clue language:** Healthy branches return matching light and sound pulses. The faulty branch returns a reversed rhythm or broken segment. Cables or floor traces make relationships observable without a marker.

**Recovery:** Testing is unlimited. Isolating the wrong branch causes a visible harmless rejection and short re-arm delay. Repair progress persists.

**Optional branch:** After choosing the first destination, the unpowered destination can be restored by surviving a harder local surge.

**Reward:** The first choice immediately opens a useful shortcut or an armory service. Completing both grants an economy bonus.

## 3. Resonant Sequence

[Open quest graph](diagrams/quest-recipes/03-resonant-sequence.html)

**Identity:** Separate districts contain resonators that teach pieces of a tone-and-symbol sequence. The player reconstructs and performs it while under pressure.

**Primary novelty:** Information is split across spatially separated observations, and the answer combines sound with symbols.

```text
Discover sealed resonant door
        ↓
Find resonator observations in two or three districts
        ↓
Determine the combined sequence
        ↓
Prime the performance chamber
        ↓
Shoot or activate resonators in order during combat
        ↓
Survive the answering resonance
        ↓
Enter the opened chamber
```

**Realization requirements:**

- Two or three clue anchors separated by route and district.
- One arena with three safely spaced shootable target anchors.
- A reward chamber or previously sealed passage.
- Presentation support for both tone and shape so the puzzle is not audio-only.

**Clue language:** Each observation presents an ordered fragment with one overlap, allowing the complete sequence to be inferred. The target resonators reuse the same shapes and tones.

**Recovery:** A wrong hit resets only performance progress and plays the expected relationship without naming the next target. The chamber can be re-primed immediately after a short cooldown.

**Optional branch:** Completing the sequence without an error opens a secondary cache; ordinary completion still advances the quest.

**Reward:** Reward-room access, a fire-rate or weapon upgrade, and an optional flawless bonus.

## 4. Perimeter Wake

[Open quest graph](diagrams/quest-recipes/04-perimeter-wake.html)

**Identity:** The fortress contains a sleeping perimeter network. Activating its stations changes circulation and turns a previously fragmented outer route into a complete loop.

**Primary novelty:** The puzzle is organized around traversal direction and progressive route transformation rather than a central-room sequence.

```text
Discover a dormant perimeter station
        ↓
Activate any one station
        ↓
Follow the visible wake to the next reachable station
        ↓
Complete stations around the perimeter
        ↓
Defend the final station from separated ingress lanes
        ↓
Open the missing link and complete the circulation loop
        ↓
Use the new loop to approach the finale from either side
```

**Realization requirements:**

- A partial or gated cycle with three or four station anchors distributed around it.
- Physically separated alternate routes.
- A final defendable station with multiple ingress regions.
- A missing shortcut edge whose activation completes a useful loop.

**Clue language:** Activation sends a visible pulse along walls or floor traces toward the next network segment. It identifies a relationship, not a map destination.

**Recovery:** Completed stations remain active. The final defense pauses rather than resets if the player leaves.

**Optional branch:** Activating the stations in the less obvious reverse direction exposes a hidden perimeter cache.

**Reward:** A major permanent shortcut, improved passageway loot rolls, and finale access.

## 5. The Hollow Signal

[Open quest graph](diagrams/quest-recipes/05-the-hollow-signal.html)

**Identity:** Directional listening devices detect an unknown transmission. The player triangulates its source, opens the correct wall section, and confronts what is broadcasting.

**Primary novelty:** Spatial inference from directional readings rather than a sequence or explicit route.

```text
Hear an intermittent unknown signal
        ↓
Inspect directional receiver A
        ↓
Inspect receiver B from a separated room
        ↓
Optionally inspect receiver C for a clearer solution
        ↓
Infer the intersecting source region
        ↓
Activate the matching wall or passage control
        ↓
Destroy or stabilize the hidden transmitter
        ↓
Trace its return signal to a changed landmark
```

**Realization requirements:**

- Two mandatory receiver anchors with strong physical separation and distinct facing.
- One optional receiver on a riskier or gated branch.
- A hidden passage or obscured alcove near the inferred intersection.
- A transmitter site that supports either shooting or a short hold interaction.

**Clue language:** Each receiver shows a broad directional arc using orientation, pulse strength, and sound. The optional third narrows ambiguity but is not mandatory.

**Recovery:** Incorrect wall controls reject the signal and remain retryable. The transmitter cannot be permanently destroyed before the quest reaches that stage.

**Optional branch:** The third receiver reveals a second weak source containing a cache or lore fragment.

**Reward:** Hidden-route access, a weapon pickup, and progression toward extraction.

## 6. Predator's Lens

[Open quest graph](diagrams/quest-recipes/06-predators-lens.html)

**Identity:** An ancient focusing machine responds only to a powerful hostile signature. The player charges its sensors, then deliberately lures an Elite through the lens intersection.

**Primary novelty:** The player manipulates a dangerous enemy's position instead of merely killing everything near a static objective.

```text
Inspect inactive focusing lens
        ↓
Charge two sensor pylons with nearby kills
        ↓
Align the pylons from their local controls
        ↓
Wait for or provoke an Elite event
        ↓
Lure the Elite into the marked lens intersection
        ↓
Trigger the lens while the Elite is present
        ↓
Collect the crystallized output
```

**Realization requirements:**

- A long-lane or crossfire arena with two separated pylon anchors.
- A readable central intersection with safe player escape routes.
- Enemy navigation that can reach the marked region through current gates.
- A deterministic way to provide an Elite without stopping normal round advancement.

**Clue language:** Ordinary enemies briefly flicker near the dormant lens, while Elite attacks produce a stronger matching signature. Pylons visually project their intersection after alignment.

**Recovery:** Pylon charge persists. Missing the Elite with the lens incurs a cooldown but does not consume the quest. If the Elite dies elsewhere, another eligible Elite can satisfy the stage later.

**Optional branch:** Triggering the lens while several ordinary enemies also occupy the region increases the reward tier.

**Reward:** A powerful temporary modifier, rare weapon, or upgrade token.

## 7. Borrowed Charge

[Open quest graph](diagrams/quest-recipes/07-borrowed-charge.html)

**Identity:** The player transports an unstable power charge through the fortress, choosing a route and keeping it alive through combat interactions.

**Primary novelty:** A carried state turns traversal and route selection into the puzzle.

```text
Discover portable charge cradle
        ↓
Prime the unstable charge
        ↓
Choose one of two viable routes to the destination
        ↓
Carry the charge between stabilizer sockets
        ↓
Recharge it through kills or short hold interactions
        ↓
Install it before stability expires
        ↓
Power the destination and open the return shortcut
```

**Realization requirements:**

- A source and destination separated by meaningful travel.
- Two viable approaches or one route plus a later unlockable shortcut.
- Stabilizer sockets distributed through passageways without obstructing combat lanes.
- Clear carry-state presentation and deterministic safe respawn behavior.

**Clue language:** The charge's pulse accelerates as stability falls. Passageway stabilizers share its silhouette and respond visibly when approached.

**Recovery:** Dropping or losing the charge returns it to the last activated stabilizer after a short delay. The player never permanently loses a mandatory component.

**Optional branch:** Bypassing one stabilizer and arriving with high residual charge powers an additional cache.

**Reward:** Destination service activation, a traversal shortcut, and a high-charge bonus.

**Implementation note:** This recipe requires a reusable carry/install system and should follow the first recipes built entirely from existing interaction verbs.

## 8. Arsenal Covenant

[Open quest graph](diagrams/quest-recipes/08-arsenal-covenant.html)

**Identity:** A sealed armory tests whether the player understands and uses different weapon properties rather than simply possessing enough currency.

**Primary novelty:** Weapon behavior becomes a puzzle input.

```text
Discover sealed armory and three weapon sigils
        ↓
Inspect passageway traces that demonstrate each property
        ↓
Satisfy two of three weapon trials
      ↙     ↓      ↘
Pierce aligned targets   Detonate clustered seals   Sustain fire on a moving plate
      ↘     ↓      ↙
Choose one armory covenant
        ↓
Receive a weapon and alter future passageway drops
```

**Realization requirements:**

- An armory or Reward room with three readable trial anchors.
- Passageway weapon pickups or swap stations guaranteeing access to required properties.
- Trial geometry that safely supports alignment, clustering, or tracking.
- A fallback trial if a weapon class is unavailable in the current build.

**Clue language:** Environmental traces demonstrate effects through damaged plates, aligned holes, scorch clusters, or moving machinery. Tooltips identify weapon properties, not which seal to use.

**Recovery:** Trials are independently retryable. The recipe requires only two of three, preventing one disliked or unavailable weapon style from blocking progression.

**Optional branch:** Completing all three before choosing grants an enhanced version or reroll token.

**Reward:** A weapon choice plus deterministic weighting toward a selected weapon family in later passageway drops.

**Implementation note:** This recipe requires weapon-property tags and generated access guarantees.

## 9. Blackout Protocol

[Open quest graph](diagrams/quest-recipes/09-blackout-protocol.html)

**Identity:** Activating an emergency system darkens part of the fortress. The player navigates by pulses, restores local substations, and decides which district returns first.

**Primary novelty:** Lighting and temporary information loss change navigation without relying on an objective marker.

```text
Inspect emergency power console
        ↓
Trigger controlled blackout
       ↙ ↘
Follow pulse path A   Follow pulse path B
       ↓                 ↓
Restore local substation A or B
        ↓
Use the restored district to reach the second substation
        ↓
Defend the master restart
        ↓
Observe newly illuminated clue or passage
```

**Realization requirements:**

- Two districts or spatially distinct branches.
- Safe minimum lighting and high-contrast floor or wall pulses.
- Substation anchors reachable during blackout.
- A master console in a multi-ingress combat room.
- Accessibility options that preserve the route language without requiring darkness perception.

**Clue language:** Emergency pulses travel physically from the master console toward substations. Restored lights reveal previously invisible symbols or doorway controls.

**Recovery:** Leaving the blackout area does not reset substations. The master restart defense pauses if abandoned. The player can re-inspect the console to replay the pulse pattern.

**Optional branch:** Restoring the more dangerous district first keeps a hidden cache powered long enough to open it.

**Reward:** A revealed shortcut, clue, or service plus a first-route bonus.

## 10. The Living Lock

[Open quest graph](diagrams/quest-recipes/10-the-living-lock.html)

**Identity:** A gate is controlled by biological occupancy sensors. The player must shape horde movement through several zones instead of immediately killing every enemy.

**Primary novelty:** Crowd routing and restraint become puzzle mechanics in a horde shooter.

```text
Discover gate with dormant occupancy sensors
        ↓
Activate sensor calibration
        ↓
Charge outer sensor with a crowd presence
        ↓
Redirect surviving enemies through the inner sensor
        ↓
Seal or open a temporary route to shape movement
        ↓
Kill the marked crowd inside the final chamber
        ↓
Open the living lock
```

**Realization requirements:**

- A multi-entry arena or connected pair of rooms with distinct ingress regions.
- Two readable occupancy zones and a final kill zone.
- Door-aware enemy navigation that updates when the temporary route changes.
- Sufficient safe exits so the optimal tactic is not standing in one doorway.
- Population scheduling that guarantees enough ordinary enemies without pausing rounds.

**Clue language:** Sensors brighten according to occupancy and project their required direction toward the next zone. The final chamber changes from a movement symbol to a defeat symbol once enough enemies cross.

**Recovery:** Sensor charge decays only to the current stage floor. Killing the crowd too early delays progress until later spawns but never fails the complete quest. Temporary route controls reset safely after each attempt.

**Optional branch:** Moving a larger crowd through both sensors without losing members opens a secondary biological cache.

**Reward:** Gate access, a crowd-control upgrade, and an optional mastery reward.

# Recipe coverage matrix

| Recipe | Distinctive mechanic | Reused foundations | Additional system cost |
|---|---|---|---|
| Awaken the Fortress | Clue order plus parallel trials | Inspect, hold, nearby kills, shoot target, shortcut | Low |
| Broken Circuit | Diagnose a faulty branch | Inspect, hold, nearby kills, route power | Low–medium |
| Resonant Sequence | Reconstruct split audiovisual sequence | Ordered targets, combat survival | Low–medium |
| Perimeter Wake | Traversal around a changing cycle | Activate, defend, shortcut | Low |
| The Hollow Signal | Directional triangulation | Inspect, shoot/hold, hidden passage | Medium |
| Predator's Lens | Lure an Elite into a device | Nearby kills, Elite schedule, activate | Medium |
| Borrowed Charge | Carry unstable state through routes | Hold, kills, shortcut | High: carry system |
| Arsenal Covenant | Weapon properties as puzzle inputs | Shoot targets, passageway weapons | High: weapon tags/trials |
| Blackout Protocol | Navigation through controlled darkness | Activate, defend, district routing | Medium: lighting states |
| The Living Lock | Manipulate crowd movement and occupancy | Enemy navigation, zones, route locks | High: occupancy sensors |

The initial implementation should begin with recipes whose system cost is low, but the data model should represent all ten without recipe-specific runtime architecture.

## Testing strategy

### Per-recipe tests

- The same seed reproduces the exact recipe variant and binding.
- Every mandatory stage has a compatible bound anchor.
- Dependency ordering and optionality are correct.
- Wrong actions apply the declared recovery rule.
- Automatic rounds continue during every incomplete stage.
- Completion enables the declared reward and voluntary finale behavior.
- Reset restores initial quest state without changing the accepted map.

### Progression-component tests

For every gate stage:

- Mandatory stages and clues are reachable.
- The reachable component supports scheduled population and ingress.
- Required passageway supplies are present.
- Temporary and permanent route changes update player and enemy navigation together.
- Shortcut savings meet the recipe requirement.

### Cross-seed tests

Across a fixed seed matrix, record:

- Recipe and variant distribution.
- Stage-room and anchor signatures.
- Clue-order and presentation-family distribution.
- Optional-branch placement.
- Passageway loot category and risk distribution.
- Backtracking before and after quest shortcuts.

Tests should reject impossible or collapsed distributions without requiring nondeterminism. Identical inputs must remain identical.

### Manual feel review

For fixed regressions and fresh seeds, verify:

- The player notices important mechanisms without a global marker.
- Clues are inferable before the solution is required.
- Local tooltips clarify verbs without revealing answers.
- Quest stages use different parts and properties of the generated map.
- Puzzle activity remains readable during horde pressure.
- Passageways contain useful discoveries without becoming cluttered vending corridors.
- Completing the quest visibly changes navigation or a major landmark.
- Optional branches feel discovered rather than assigned.

## Future generated-recipe grammar

After several curated recipes prove the executor and binder, recipe generation can compose authored motifs such as:

```text
Discovery
    → Observation or collection
    → Interpretation
    → Ordered, parallel, or choice gate
    → Combat-linked proof
    → Persistent world transformation
    → Verification or return
    → Reward and voluntary finale
```

Generation should select only compatible motifs, enforce bounded complexity, and validate the complete dependency graph before binding. Novelty should come from meaningful combinations and spatial realization, not arbitrary stage shuffling.

The curated recipes remain valuable as regression fixtures, quality references, and fallback content even after generated recipes become available.
