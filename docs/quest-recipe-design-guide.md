# Procedural Quest Recipe Design Guide

This guide uses practical ASD-STE100 Simplified Technical English. It uses short requirements and consistent terms.

This guide defines how to author, show, bind, validate, and generate quests. It expands the quest-realization phase in [`level-generation-lock-in.md`](level-generation-lock-in.md).

Development goals:

- Build a curated library of validated multi-stage puzzle recipes.
- Place rooms, routes, devices, clues, rewards, and optional discoveries procedurally.
- Generate full quest graphs from validated authored motifs later.
- Use the same runtime for curated and generated quest graphs.

Current production status:

- Production maps have exactly two useful cycles.
- Each Shortcut saves at least two arena transitions.
- Progression-stage checks are complete.
- The implemented stage-spawn validation is a static post-gate spawn-packing check.
- The static post-gate spawn-packing check does not prove runtime spawn behavior.

Current audit:

- The audit used 100 requested match seeds.
- Six requests used the radius-5 fallback.
- The audit calculated production-map metrics from 94 accepted radius-8 maps.
- The mean multi-entry substantial-room ratio was 66.7%, not 70%.

Current gaps:

- Bounded Combat leaves and dead ends.
- Physical route separation.
- Room and combat grammar.
- Typed semantic anchors.
- The full quest compiler.
- Initial-lock, visibility, and role spawn checks.
- Runtime recovery, endurance, and navigation checks.

## Experience goal

A quest must use fortress geometry, routes, and persistent world state. It must not operate as a checklist that is independent of the generated map.

The player must be able to:

- Find dormant machines, strange symbols, blocked routes, and reactive objects during normal movement.
- Infer relationships from repeated visual, spatial, and audio language.
- Complete multi-stage objectives during continuous horde rounds.
- Cause persistent and visible changes to landmarks and routes.
- Find optional content that rewards observation and exploration.
- Get local interaction help without disclosure of the full solution.

The level generator and quest binder must operate together. A valid dependency graph does not correct geometry that fails its spatial requirements. A valid room graph does not correct objective placement that ignores route and anchor constraints.

## Presentation rules: assistance without signposting

### The directive

The directive gives global state only. It can report a route, service, reward, or extraction change. It must not name the next room. It must not draw a route. It must not list undiscovered stages. It must not reveal a sequence.

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

F2 and F3 can show the full binding for development. Normal play must not show it.

### Local tooltips

Show a tooltip only when the player is near an interactable. Require line of sight when it is applicable. Explain the available action or the immediate prerequisite. Do not explain the puzzle solution.

Examples:

- `E — Inspect dormant terminal`
- `E — Insert charged cell`
- `Hold E — Redirect power`
- `Requires a matching sigil`
- `The mechanism rejects this sequence`
- `Resonates when enemies fall nearby`

Do not give an undiscovered object a global marker. A discovered object can keep a low-intensity world-state signal. Do not make it a permanent navigation arrow by default.

### Environmental communication

Give related quest elements one generated presentation language. Use:

- A symbol family.
- A color or light rhythm with shape and sound support.
- Cable, conduit, beam, or floor-trace connections.
- A repeated mechanical silhouette.
- A separate activation tone.
- Persistent dormant, discovered, partial, failed, and complete states.

Do not use color as the only signal. Show the same relationship with symbol, placement, animation, or audio.

### Discovery record

A player discovery record can contain discovered facts only. These facts can include copied symbols and heard tone patterns. They can also include inspected device text and visible machine reactions. Do not convert these facts into an ordered checklist.

## Keep three graphs separate

Quest realization uses three related graphs.

### 1. Spatial circulation graph

This graph is the generated network of substantial rooms, connectors, and authorized doorway thresholds. It defines routes, cycles, dead ends, gates, Shortcuts, and enemy navigation.

Useful-cycle metric:

- Start with the connector-contracted arena graph.
- Remove one non-Primary mission edge.
- Find the shortest alternate path between that edge's arenas.
- Count the edge as a useful cycle only when the alternate path has at least three arena transitions.
- Require exactly two useful cycles for production circulation.
- Require each Shortcut to save at least two arena transitions.
- Calculate Shortcut savings as the alternate-path length minus the direct Shortcut edge.

### 2. Semantic-anchor graph

This graph defines valid positions for quest content in the generated geometry. Anchors can include:

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

Anchors publish types, capabilities, and measurements. They do not select a quest. Typed semantic anchors are not implemented yet.

### 3. Quest dependency graph

A recipe is a directed acyclic graph. It contains discoveries, requirements, actions, state changes, optional branches, and rewards. Its nodes use semantic requirements. They do not use room IDs or world coordinates.

Binding requirements:

- Map the dependency graph to the semantic-anchor and circulation graphs.
- Satisfy all mandatory role constraints.
- Satisfy all route-order and distance constraints.
- Satisfy all visibility, clearance, and separation constraints.
- Reject the full map candidate if no valid binding exists.

Current binder status:

- The binder handles gates and the Anchor.
- The full quest compiler is not complete.

## Use curated recipes before generated recipes

The first implementation must use curated dependency graphs. A curated recipe defines intentional puzzle logic. It permits variable spatial realization.

A recipe must not hard-code:

- Room IDs.
- Cell IDs or world coordinates.
- A fixed east or west orientation.
- One exact path through the level.
- Specific weapon instances or random rewards.
- Round 2 or Round 5 as universal progression rules.

For each match, deterministic binding can change:

- The compatible rooms that contain stages.
- The anchors that contain devices and clues.
- The symbol, tone, or light vocabulary.
- The required order of equivalent devices.
- The first valid parallel branch that the player finds.
- The position of Shortcuts and optional branches.
- The passageway supplies and final reward variants.

Use this implementation sequence:

1. Use a curated graph with generated placement.
2. Add generated clue, order, branch, and reward variants to the curated graph.
3. Assemble a generated graph from curated motifs.
4. Generate and validate the full dependency graph and its placement.

Generated recipes must produce the same declarative data as curated recipes. Runtime quest code must use one execution path.

## Recipe model

Each recipe must publish the following data.

### Recipe-level fields

- Stable recipe identifier and version identifier.
- Main dependency graph and optional dependency graph.
- Required topology capabilities.
- Minimum stage count and maximum stage count.
- Required interaction verbs.
- Clue and presentation families.
- Gate and Shortcut effects.
- Reward categories and fallback rewards.
- Permitted recovery policies.
- Complexity tier or implementation tier.

### Stage-level fields

- Stable stage identifier.
- Prerequisite stages.
- Prerequisite mode: `all`, `any`, ordered sequence, or configured count.
- Interaction verb and completion condition.
- Room-role and room-shape requirements.
- Route-order, distance, visibility, clearance, and separation limits.
- Hidden, discoverable, active, optional, or complete state.
- Persistent world feedback for each state.
- Failure behavior and recovery behavior.
- Outputs such as power, clues, gates, Shortcuts, rewards, or extraction availability.

### Generic interaction verbs

The first executor must use reusable verbs where possible:

- Inspect a device or clue.
- Activate or hold an interaction.
- Hold a zone while combat continues.
- Kill enemies near a device or in a region.
- Shoot a target or an ordered target sequence.
- Power or redirect a route.
- Return to a changed landmark.
- Install or carry a component after the carry system exists.
- Lure a normal enemy, crowd, or Elite into a marked region.
- Use a compatible weapon property after weapon classification exists.

A recipe combines these verbs. It must not implement a separate round director. It must not implement enemy simulation, interaction input, or navigation.

## Binding pipeline

One match seed must derive these binding inputs deterministically:

- Geometry.
- The circulation brief.
- Room-shape briefs.
- Semantic anchors.
- The selected recipe and binding variants.
- Rewards.
- The bounded retry sequence.

Use this binding order:

1. Generate and validate the circulation graph.
2. Generate room-shape and combat briefs.
3. Publish semantic, presentation, and loot anchors.
4. Select a compatible curated recipe from the match seed.
5. Select deterministic recipe variants, such as symbol vocabulary and device order.
6. Bind mandatory stages in dependency order and route order.
7. Bind optional branches and their rewards.
8. Bind gates and Shortcuts to exact doorway thresholds.
9. Populate normal loot sockets around the finished quest binding.
10. Validate each progression component.
11. Reject an impossible candidate.
12. Store the full binding and retry metadata for replay and tests.

Select the quest before candidate acceptance. This prevents acceptance of geometry that cannot realize the selected recipe. Candidate scoring must not change to an easier recipe without a report.

Current binding status:

- Progression-stage admission is complete for the current gate and Anchor plan.
- The static post-gate spawn-packing check is complete.
- The static post-gate spawn-packing check measures capacity in simulated lock states.
- The static post-gate spawn-packing check does not run the runtime spawn system.
- The full recipe compiler remains incomplete.
- Immutable typed-anchor binding remains incomplete.

## Binding and acceptance rules

Each accepted quest must satisfy these rules.

### Solvability

- Make each mandatory stage reachable when its prerequisites are satisfied.
- Do not put a required item behind the gate that consumes it.
- Do not let one stage permanently invalidate another required stage.
- Keep extraction or another declared finale possible after completion.
- Give each failure-capable stage a recovery rule.

### Spatial meaning

- Make each recipe supply a minimum device-separation distance.
- Reject a binding when two devices are closer than that limit.
- Make route order support the dependency graph.
- Make each recipe supply a maximum Hub-backtracking distance or arena-transition count.
- Reject a binding when it exceeds that limit.
- Make a Shortcut save at least its required measured travel.
- Make a Shortcut reduce measured travel or add a separate approach.
- Keep optional branches outside every mandatory route.
- Do not put a mandatory stage or item in an optional branch.
- Give each used dead end a quest result or loot result.
- Make each recipe supply a minimum physical route-separation limit.
- Reject a binding when alternate routes do not meet that limit.

Current production graph status:

- Minimum Shortcut savings are two arena transitions.
- The code does not measure physical centerline separation.
- The code does not measure doorway-angle separation.

### Combat safety and pressure

- Make each holdout recipe supply minimum footprint area, ingress count, packed spawn count, and entry-space clearance.
- Reject a holdout binding when it fails one of these limits.
- Make each target recipe supply a minimum firing-lane length and width.
- Reject a target binding when it fails one of these limits.
- Keep targets clear of doors and walls.
- Do not overlap a required device footprint with a packed spawn slot.
- Make each late-stage recipe supply its maximum scheduled living population.
- Reject a binding when that population exceeds validated room or packed-spawn capacity.
- Quest state must not start, pause, or advance the round director.

Static post-gate spawn-packing check:

- Each checked Fortress state requires 18 packed slots.
- These slots must occur in at least two rooms.
- The static post-gate spawn-packing check does not simulate runtime occupancy.
- The static post-gate spawn-packing check does not run runtime spawn selection.
- The static post-gate spawn-packing check does not prove that runtime spawning can complete a round.
- Initial-lock, visibility, role compatibility, and runtime recovery checks remain incomplete.

### Legibility

- Put necessary clues before the point that requires their answer.
- Give related objects one consistent presentation family.
- Give persistent feedback for each accepted action.
- Communicate a wrong action without arbitrary repetition.
- Do not make mandatory progress depend on an unmarked random drop.

### Determinism

The same match seed must reproduce:

- Recipe and version.
- Stage graph and selected variants.
- Room and anchor binding.
- Clue vocabulary and order.
- Gates, Shortcuts, and reward category.
- Retry count and rejection metadata.

## Persistent state and automatic rounds

Quest state operates at the same time as the endless director.

- A round requirement can make an interaction available.
- A round requirement must not hold an intermission open.
- Keep incomplete stages across all rounds.
- Define whether combat progress persists, pauses, or resets.
- Do not remove quest devices, clues, carried components, or complete state during hostile cleanup.
- Quest completion can enable extraction, a configured reward, a route change, or a pressure change.
- Quest completion must not stop spawning without an explicit rule.
- Keep extraction as a voluntary interaction.

Replace the hard-coded Round 2 Anchor and Round 5 Exit checks with recipe requirements. Remove the checks when a selected recipe does not need them.

## Passageways, supplies, weapons, and upgrades

Put resource opportunities in connectors. Do not make them empty traversal tubes. Publish passageway loot sockets with clearance, visibility, concealment, route stage, nearby-door, and risk measurements.

### Content categories

Sockets can contain these items when the related economy exists:

- Weapon pickups or weapon-swap stations.
- Damage, fire-rate, dash, health, or armor upgrades.
- Temporary speed, shield, damage, piercing, or crowd-control boosts.
- Healing, armor, and currency caches.
- One-use explosives, mines, decoys, or deployable turrets.
- Upgrade, reroll, or service tokens.
- Quest components and clue fragments.
- Breakable or inspectable secret caches.
- Risk/reward shrines.
- Ammunition after an ammunition economy exists.

### Distribution rules

Use random placement only inside deterministic limits.

- Make each recipe supply minimum opening-component resource counts or values by category.
- Reject a binding when the opening component fails one of these limits.
- Increase reward weights for routes with higher measured risk, cost, or length when applicable.
- Give each dead end loot, a clue, a service, or a secret.
- Put utility in Shortcuts instead of mandatory progression where possible.
- Use guaranteed recipe placement for mandatory quest components.
- Do not use the normal loot roll for mandatory components.
- Populate normal loot only after quest binding.
- Keep items clear of doorway reading space and gate collision.
- Keep items clear of objective footprints and primary crowd lanes.
- Limit clustering across the map economy.
- Make each recipe supply a maximum resource count or value for one connector.
- Reject a loot binding when one connector exceeds that limit.
- Reproduce all placements from the same match seed.

## Recovery patterns

Give each multi-stage puzzle explicit recovery. Do not permit accidental soft locks.

Use these policies where applicable:

- **Persist:** Keep completed work complete.
- **Pause:** Stop progress while a condition is false. Continue it when the condition is true again.
- **Reset stage:** Reset only the active local sequence.
- **Re-arm:** Require a short interaction before another attempt.
- **Respawn component:** Return a lost mandatory component to its last safe socket.
- **Alternate proof:** Replace a failed optional challenge with a more expensive combat or currency requirement.

Permit a full-quest reset only when the recipe declares that policy. A bounded runtime recovery rule for impossible spawn conditions is still a current gap.

# Ten curated quest recipes

The following recipes are authored dependency graphs for procedural binding. The seed can change names, symbols, rooms, directions, and rewards. It can also change device counts inside the stated limits.

## 1. Awaken the Fortress

[Open quest graph](diagrams/quest-recipes/01-awaken-the-fortress.html)

**Identity:** The player finds the purpose of a dormant central machine. The player completes two parallel combat trials. The player then starts the changed fortress.

**Primary novelty:** A clue defines an ordered activation. This activation opens two parallel objectives. The objectives join again at a newly exposed target.

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

- Give the Hub or focal landmark a unique silhouette.
- Make the landmark visible from several approaches.
- Put two separate clue sites before the ordered activation.
- Put two combat-capable rooms on different branches or different sides of a useful cycle.
- Use a locked Shortcut that meets the configured return-travel savings limit.
- Expose a target anchor only after both parallel trials are complete.

**Clue language:** Put two or three symbols on environmental fragments. Put the same symbols on the related power devices. Inspection of the Hub tells the player that order is important. It does not give the order.

**Recovery:** Reset only the two-device sequence after a wrong activation. Pause holdout progress when the player leaves. Keep the kill count for the conduit trial.

**Optional branch:** A third damaged fragment shows a Reward-room cache. Make the cache available after the Shortcut opens.

**Reward:** Enable extraction. Give one configured high-tier upgrade or weapon choice at the changed Hub.

## 2. Broken Circuit

[Open quest graph](diagrams/quest-recipes/02-broken-circuit.html)

**Identity:** A power network sends energy into a damaged branch. The player finds the fault and isolates it. The player repairs two endpoints and reconnects the network safely.

**Primary novelty:** The player compares network behavior to find a fault. The player does not follow a specified device order.

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

- Use a junction room with three or more separate outgoing thresholds.
- Put two test devices and one faulty endpoint in different route directions.
- Use one defendable repair site.
- Use a separate kill-near-device site.
- Provide two power destinations. Each destination must open a route or activate a service.
- Do not require either destination to be a room leaf.

**Clue language:** Make healthy branches return matching light and sound pulses. Make the faulty branch return a reverse rhythm or a broken segment. Use cables or floor traces to show the relationships. Do not use a marker.

**Recovery:** Permit unlimited tests. A wrong isolation gives a visible and harmless rejection. It also causes a short re-arm delay. Keep repair progress.

**Optional branch:** After the first destination gets power, let the player restore the other destination. Use a higher configured enemy budget for the local surge.

**Reward:** Open a Shortcut that reduces route cost, or open an armory service. Give an economy bonus after both destinations operate.

## 3. Resonant Sequence

[Open quest graph](diagrams/quest-recipes/03-resonant-sequence.html)

**Identity:** Different districts contain parts of a tone and symbol sequence. The player reconstructs the sequence. The player performs it during combat pressure.

**Primary novelty:** The player combines information from separate places. The answer uses sound and symbols.

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

- Put two or three clue anchors in separate routes and districts.
- Use one arena with three safely separated shootable target anchors.
- Provide a Reward chamber or a sealed passage.
- Support tone and shape.
- Do not make the puzzle audio-only.

**Clue language:** Give each observation an ordered fragment. Give adjacent fragments one overlap. This lets the player infer the full sequence. Use the same shapes and tones on the target resonators.

**Recovery:** Reset only performance progress after a wrong hit. Play the expected relationship. Do not name the next target. Let the player prime the chamber again after a short cooldown.

**Optional branch:** Open a second cache if the player makes no error. Normal completion must still progress the quest.

**Reward:** Give access to the Reward room. Give a fire-rate or weapon upgrade. Give an optional flawless bonus.

## 4. Perimeter Wake

[Open quest graph](diagrams/quest-recipes/04-perimeter-wake.html)

**Identity:** The fortress has a dormant perimeter network. The player activates its stations. This action changes circulation and completes an outer loop.

**Primary novelty:** Traversal direction and route changes organize the puzzle. A central-room sequence does not organize it.

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

- Use a partial or gated cycle.
- Put three or four station anchors around it.
- Provide physically separate alternate routes.
- Use a defendable final station with several ingress regions.
- Activate a missing Shortcut edge to complete a useful cycle.

**Clue language:** Send a visible pulse on walls or floor traces after activation. Point the pulse toward the next network segment. Show a relationship. Do not show a map destination.

**Recovery:** Keep completed stations active. Pause the final defense if the player leaves. Do not reset it.

**Optional branch:** Let activation in the reverse direction expose a hidden perimeter cache.

**Reward:** Open a permanent Shortcut with measured route savings. Improve passageway loot rolls. Give access to the finale.

## 5. The Hollow Signal

[Open quest graph](diagrams/quest-recipes/05-the-hollow-signal.html)

**Identity:** Directional receivers detect an unknown transmission. The player finds the source by triangulation. The player opens the correct wall section and confronts the transmitter.

**Primary novelty:** The player uses directional readings for spatial inference. The puzzle does not use an explicit route or sequence.

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

- Use two mandatory receiver anchors that meet the configured separation limit.
- Give the receiver anchors different directions.
- Put one optional receiver on a branch with higher measured risk or a gate.
- Put a hidden passage or obscured alcove near the inferred intersection.
- Use a transmitter site that supports shooting or a short hold interaction.

**Clue language:** Show a broad directional arc on each receiver. Use orientation, pulse strength, and sound. The optional third receiver reduces ambiguity. Do not make it mandatory.

**Recovery:** Make incorrect wall controls reject the signal. Keep them available for another attempt. Do not permit permanent transmitter destruction before the correct stage.

**Optional branch:** Use the third receiver to show a second weak source. Put a cache or lore fragment at that source.

**Reward:** Open a hidden route. Give a weapon pickup. Progress the player toward extraction.

## 6. Predator's Lens

[Open quest graph](diagrams/quest-recipes/06-predators-lens.html)

**Identity:** An old focusing machine responds only to an Elite-class hostile signature. The player charges its sensors. The player then lures an Elite through the lens intersection.

**Primary novelty:** The player controls the position of an Elite. The player does not only kill enemies near a fixed objective.

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

- Use a long-lane or crossfire arena.
- Put two pylon anchors far from each other.
- Use a marked central intersection with at least two player escape routes.
- Confirm that enemy navigation can reach the marked region through current gates.
- Provide an Elite deterministically.
- Do not stop normal round advancement.

**Clue language:** Make normal enemies flicker briefly near the dormant lens. Make Elite attacks produce a higher-intensity matching signal. After alignment, make the pylons show their intersection.

**Recovery:** Keep pylon charge. Apply a cooldown after a missed Elite. A miss must not consume the stored pylon charge. If the Elite dies in another place, permit a later eligible Elite to complete the stage.

**Optional branch:** Increase the reward tier if several normal enemies are also in the region when the lens operates.

**Reward:** Give a top-tier temporary modifier, weapon, or upgrade token.

## 7. Borrowed Charge

[Open quest graph](diagrams/quest-recipes/07-borrowed-charge.html)

**Identity:** The player moves an unstable charge through the fortress. The player selects a route. The player keeps the charge stable through combat interactions.

**Primary novelty:** A carried state makes traversal and route selection part of the puzzle.

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

- Make the source-to-destination route meet the configured travel-distance limit.
- Use the route-choice stage only when two outbound approaches exist before installation.
- For the linked route-choice recipe, provide two outbound approaches before installation.
- If a binding has one outbound route and a later return Shortcut, omit the route-choice stage.
- Do not present the later return Shortcut as an outbound route choice before it opens.
- Put stabilizer sockets in passageways.
- Keep the sockets clear of combat lanes.
- Show the carry state clearly.
- Provide deterministic respawn behavior at a valid socket.

**Clue language:** Increase the charge pulse rate as stability decreases. Give passageway stabilizers the same silhouette. Make them react when the player comes near.

**Recovery:** Return a dropped or lost charge to the last active stabilizer after a short delay. Do not let the player lose a mandatory component permanently.

**Optional branch:** Power an additional cache if the player omits one stabilizer and arrives with high residual charge.

**Reward:** Activate a destination service. Open a traversal Shortcut. Give a high-charge bonus.

**Implementation note:** This recipe needs a reusable carry and install system. Implement it after recipes that use existing interaction verbs.

## 8. Arsenal Covenant

[Open quest graph](diagrams/quest-recipes/08-arsenal-covenant.html)

**Identity:** A sealed armory tests the player's use of different weapon properties. Currency alone cannot complete the test.

**Primary novelty:** Weapon behavior is a puzzle input.

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

- Use an armory or Reward room with three separated trial anchors.
- Guarantee access to required weapon properties through passageway pickups or swap stations.
- Use trial geometry that safely supports alignment, clustering, or tracking.
- Provide a fallback trial if the current build does not have a weapon class.

**Clue language:** Use damaged plates, aligned holes, scorch clusters, or moving machines to show effects. Tooltips identify weapon properties. They do not identify the correct seal.

**Recovery:** Make each trial independently repeatable. Require only two of three trials. Do not let one unavailable or unwanted weapon style stop progress.

**Optional branch:** Give an enhanced item or reroll token if the player completes all three trials before selection.

**Reward:** Give a weapon choice. Increase deterministic weights for the selected weapon family in later passageway drops.

**Implementation note:** This recipe needs weapon-property tags and generated access guarantees.

## 9. Blackout Protocol

[Open quest graph](diagrams/quest-recipes/09-blackout-protocol.html)

**Identity:** An emergency system makes part of the fortress dark. The player navigates with pulses. The player restores one local substation and selects the first district to restore.

**Primary novelty:** Lighting and temporary information loss change navigation. The puzzle does not use an objective marker.

```text
Inspect emergency power console
        ↓
Trigger controlled blackout
       ↙ ↘
Follow pulse path A   Follow pulse path B
       ↓                 ↓
Restore local substation A or B
        ↓
Join either first restoration to the master restart
        ↓
Defend the master restart
        ↓
Observe newly illuminated clue or passage
```

**Realization requirements:**

- Use two districts or two spatially different branches.
- Keep the configured minimum light level.
- Use high-contrast floor or wall pulses.
- Keep both alternative substation anchors reachable during blackout.
- Join either first substation restoration to the master restart.
- Do not require restoration of the second substation.
- Put the master console in a multi-entry Combat room.
- Provide accessibility options.
- Keep the route language usable without darkness perception.

**Clue language:** Send emergency pulses from the master console to the substations. Make the pulses move through physical space. Use restored lights to show hidden symbols or doorway controls.

**Recovery:** Do not reset the selected substation when the player leaves the blackout area. Pause the master restart defense if the player leaves. Let console inspection replay the pulse pattern.

**Optional branch:** Keep a hidden cache powered if the player restores the district with higher measured risk first. Let the player open it before power stops.

**Reward:** Show a Shortcut, clue, or service. Give a first-route bonus.

## 10. The Living Lock

[Open quest graph](diagrams/quest-recipes/10-the-living-lock.html)

**Identity:** Biological occupancy sensors control a gate. The player controls horde movement through several zones. Immediate kills do not solve the puzzle.

**Primary novelty:** Crowd routing and restraint are puzzle mechanics in a horde shooter.

```text
Discover gate with dormant occupancy sensors
        ↓
Activate sensor calibration
        ↓
Charge outer sensor with a crowd presence
        ↓
Seal or open a temporary route to shape movement
        ↓
Redirect surviving enemies through the inner sensor
        ↓
Kill the marked crowd inside the final chamber
        ↓
Open the living lock
```

**Realization requirements:**

- Use a multi-entry arena or a connected pair of rooms.
- Provide separate ingress regions.
- Use two non-overlapping occupancy zones and one final kill zone.
- Bind the temporary route-control stage after the outer sensor stage.
- Bind the temporary route-control stage before the inner sensor stage.
- Update door-aware enemy navigation when the temporary route changes.
- Provide at least two exits with actor clearance.
- Do not make one doorway the best standing position.
- Make the recipe supply a minimum normal-enemy count for each occupancy stage.
- Count only enemies from the normal automatic schedule.
- Do not pause the round director to meet the minimum count.
- Reject the recipe if its normal automatic schedule cannot supply the minimum count.

**Clue language:** Increase sensor brightness with occupancy. Project the required direction toward the next zone. The recipe supplies the required occupancy count. Change the final chamber symbol from movement to defeat when the configured number of enemies has crossed into the final chamber.

**Recovery:** Let sensor charge decrease only to the current stage floor. Early crowd kills can delay progress until later spawns. They must not fail the full quest. Reset temporary route controls safely after each attempt.

**Optional branch:** Open a second biological cache if a larger crowd crosses both sensors without losses.

**Reward:** Open the gate. Give a crowd-control upgrade. Give an optional mastery reward.

# Recipe coverage matrix

| Recipe | Distinctive mechanic | Reused foundations | Additional system cost |
|---|---|---|---|
| Awaken the Fortress | Clue order plus parallel trials | Inspect, hold, nearby kills, shoot target, Shortcut | Low |
| Broken Circuit | Diagnose a faulty branch | Inspect, hold, nearby kills, route power | Low–medium |
| Resonant Sequence | Reconstruct split audiovisual sequence | Ordered targets, combat survival | Low–medium |
| Perimeter Wake | Traversal around a changing cycle | Activate, defend, Shortcut | Low |
| The Hollow Signal | Directional triangulation | Inspect, shoot/hold, hidden passage | Medium |
| Predator's Lens | Lure an Elite into a device | Nearby kills, Elite schedule, activate | Medium |
| Borrowed Charge | Carry unstable state through routes | Hold, kills, Shortcut | High: carry system |
| Arsenal Covenant | Weapon properties as puzzle inputs | Shoot targets, passageway weapons | High: weapon tags/trials |
| Blackout Protocol | Navigation through controlled darkness | Activate, defend, district routing | Medium: lighting states |
| The Living Lock | Manipulate crowd movement and occupancy | Enemy navigation, zones, route locks | High: occupancy sensors |

Start implementation with recipes that have a low system cost. Make the data model represent all ten recipes. Do not add recipe-specific runtime architecture.

## Testing strategy

### Per-recipe tests

- Reproduce the exact recipe variant and binding from the same seed.
- Bind each mandatory stage to a compatible anchor.
- Check dependency order and optional status.
- Apply the declared recovery rule after each wrong action.
- Continue automatic rounds during each incomplete stage.
- Enable the declared reward and voluntary finale behavior after completion.
- Restore initial quest state during reset.
- Do not change the accepted map during reset.

### Progression-component tests

For each gate stage:

- Keep mandatory stages and clues reachable.
- Make the recipe supply a minimum ingress count and maximum scheduled population.
- Check the reachable component against those limits.
- Make the recipe supply passageway resource counts or values by category.
- Check the bound passageway supplies against those limits.
- Update player and enemy navigation together after temporary or permanent route changes.
- Meet the recipe Shortcut savings requirement.
- Run the static post-gate spawn-packing check after each gate.
- Check the initial-lock state when that validator is implemented.
- Check visibility bands and spawn role compatibility when those validators are implemented.

Current progression-test status:

- Progression-stage checks are complete for the current production plan.
- The static post-gate spawn-packing check is complete.
- The static post-gate spawn-packing check is an admission check only.
- The static post-gate spawn-packing check does not prove runtime spawn behavior.
- Initial-lock, visibility, role spawn, and runtime recovery checks remain incomplete.

### Cross-seed tests

Record these values across a fixed seed matrix:

- Recipe and variant distribution.
- Stage-room and anchor signatures.
- Clue-order and presentation-family distribution.
- Optional-branch placement.
- Passageway loot category and risk distribution.
- Backtracking before and after quest Shortcuts.
- Useful-cycle count and Shortcut savings.
- Multi-entry ratio, Combat leaves, and dead-end depth.

Reject impossible or collapsed distributions. Do not require nondeterminism. Identical inputs must remain identical.

### Manual play review

Use fixed regressions and new seeds. Verify these conditions:

- The player sees important mechanisms without a global marker.
- The player can infer clues before the solution is necessary.
- Local tooltips explain actions without disclosure of answers.
- Quest stages use different map areas and map properties.
- The player can read the current puzzle state during horde pressure.
- Passageways contain resources, clues, services, or secrets.
- Passageways do not become cluttered vending corridors.
- Quest completion changes doorway state or landmark state visibly.
- The player can enter optional branches without a directive that assigns them.
- Alternate routes use separate approaches and entry regions.
- Late rounds do not fail because of spawn or navigation exhaustion.

## Future generated-recipe grammar

First, use several curated recipes to prove the executor and binder. Then compose authored motifs such as:

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

Select compatible motifs only. Keep complexity in defined limits. Validate the full dependency graph before binding. Vary validated motif combinations and spatial bindings. Do not vary only the stage order.

Keep curated recipes after generated recipes become available. Use them as regression fixtures, quality references, and fallback content.

Complete this work before full quest generation:

- Bound Combat leaves and dead ends.
- Measure physical route separation.
- Add room and combat grammar.
- Publish typed semantic anchors.
- Implement the full quest compiler.
- Add initial-lock, visibility, and role spawn checks.
- Add bounded runtime recovery.
- Complete endurance and navigation validation.
