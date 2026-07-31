# Historical Random Large-Map Identity, Scale, and Legibility Pass

> **Historical document.**
>
> [`level-generation-lock-in.md`](level-generation-lock-in.md) supersedes this document.
> The first topology pass improved metadata and diagnostics.
> It did not eliminate repeated compact arena and connector layouts.
> Keep this document as implementation history.
> Do not use its slice order as the active roadmap.

This document uses practical ASD-STE100 Simplified Technical English. It uses short instructions and consistent technical terms.

This document defines an earlier generation slice for the game map. The team completed the full-level overview and three small-map systems fixtures. The team also completed the replayable new-match boundary and the first larger Fortress V1 profile. The team completed the first large-map topology pass.

Normal generation uses radius 8 and `worldScale = 0.22`. It uses actor-relative acceptance limits. It also selects one of five graph briefs. It does not only increase the hex radius.

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
- Runtime spawn recovery.
- Endurance and navigation work.

Observed generator properties:

- The generator is valid and deterministic.
- It measures physical space and uses published doors.
- Many accepted layouts repeat this sequence: arena → connector → arena → connector.
- The follow camera hides the full silhouette and circulation graph.
- Most large rooms use the same compact growth process.
- Differences between role colors are small.
- A map can have local boundary variation while it keeps the same room sequence and visual hierarchy.

See [`shooter-level-generation.md`](shooter-level-generation.md) for the implemented generator pipeline. See [`layout-quality-and-testing.md`](layout-quality-and-testing.md) for current scoring and validation. See [`game-handoff.md`](game-handoff.md) for runtime invariants.

## Current baseline

- The game creates a new public match seed for each new match.
- `MatchGenerationRequest` selects `physicalProfile`.
- The selected physical profile supplies grid radius, world scale, and physical limits.
- For each attempt, `matchSeed` derives the grid seed and room seed.
- The normal request uses eight bounded attempts.
- Bounded failure uses a visible radius-5 Hub Circuit fixture.
- `--seed` reproduces an accepted match in the same build and toolchain. `N` generates another match. `R` keeps the current map.
- Fortress V1 uses `worldScale = 0.22`. Fixed CLI recipes and F2 configurations use radius 5 and `0.16`. These configurations are regression presets.
- The gameplay camera follows the player. F2 opens a developer overview of the full level. The overview shows the exact runtime floor and boundaries. It shows role and ID labels. It shows the published room graph and open or locked thresholds. It also shows role markers, player position, and generator metadata.
- Left and Right browse six fixed configurations. These configurations are read-only previews. Home returns to the active session. Preview browsing pauses the session. It does not replace or change the active simulation.
- The overview shows the current published topology and room data. It does not show planned grammar as implemented data.
- Radius-5 mission graphs select Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing.
- Production radius-8 graphs select one of five `LargeMapArchetype` values. Accepted production graphs have exactly two useful cycles.
- Each layout has at least one explicit connector. Long routed edges become connector regions.
- A useful cycle is a non-Primary mission edge whose removal leaves an alternate path of at least three arena transitions in the connector-contracted arena graph.
- The planned Shortcut must save at least two arena transitions. Shortcut savings equal the alternate-path length minus the direct Shortcut edge.
- The audit used 100 requested match seeds. It produced 94 accepted radius-8 maps and six radius-5 fallbacks.
- The 94 accepted radius-8 maps had a mean multi-entry substantial-room ratio of 66.7%. The 70% hard-gate experiment failed and was reverted.
- Progression-stage checks simulate the gate sequence. These checks reject soft locks and low-value progression gates.
- The static post-gate spawn-packing check requires 18 spawn slots in at least two rooms for each checked Fortress state.
- The static post-gate spawn-packing check does not simulate runtime occupancy, player position, visibility, enemy role, or spawn recovery.
- Combat leaves and routine dead ends do not yet have the required bounds for all archetypes.
- Most shooter arenas use one compact-room growth process. A role defines gameplay purpose. It does not define a separate geometry grammar.
- Runtime room identity includes role-aware floors and sparse floor traces. It also includes wall caps, semantic objective landmarks, and a role label. Typed semantic-anchor metadata does not exist yet. Room-shape and district metadata also do not exist yet.

The pass must keep:

- Deterministic generation for a requested match seed.
- Exact dual geometry and authorized doorway pairs.
- Full reachability and physical clearance.
- Start and Exit guarantees.
- The immutable `GeneratedLevel` boundary.
- A new seed for a normal new match.
- The accepted map during restart.
- Fixed seeds as replay, regression, and fallback tools.

## Design principles

1. **Make global structure readable before you add decoration.** A palette change cannot repair an alternating chain.
2. **Use connectors for routed transitions.** Do not put a connector between every pair of rooms.
3. **Keep room role and room shape independent.** A Combat room can be compact, elongated, concave, split, or multi-entry.
4. **Make generated variation testable.** Publish archetype labels and geometry signatures. Do not infer them only from screenshots.
5. **Use semantic anchors for runtime landmarks.** Do not use hand-authored world coordinates.
6. **Use the overview first as an iteration tool.** A later player map can add discovery or fog rules. Do not reduce the debug view.
7. **Control map extent and physical scale separately.** Radius controls the available cell count. Generated-to-world scale controls room, doorway, and route size relative to actors.
8. **Validate all random output.** Derive deterministic candidates from each match seed. Retry candidates within a bounded budget. Admit only maps that pass all current geometry and gameplay checks.

## Slice 1: full-level overview and seed browser — complete

The runtime debug view fits the full generated floor on the screen. It shows structure that the follow camera hides.

It must show:

- Exact floor silhouette and room boundaries.
- Room role, stable room ID, and geometry or archetype label.
- Published doorway thresholds and the room connectivity graph.
- Start, Hub, Reward, and Exit markers.
- Current grid seed, room seed, candidate index, and quality score.
- Locked and open thresholds for an active session.
- Controls for a deterministic set of representative configurations.
- No implicit seed change during simulation.

The first version is a developer view. It is not the final player map. It shows the full layout. The fixed browser configurations are in `REPRESENTATIVE_LEVEL_CONFIGS`. Generated-level tests verify that each configuration is unique, deterministic, and valid. Add unexplored-room rules as a separate layer if the game gets a tactical player map.

## Slice 2: topology archetypes — first large-map pass complete

Radius-5 maps select Hub Circuit, Broken Ring, or Twin Wings before candidate placement and route construction. Physical routing rejects a candidate if it cannot make the selected graph safely.

Hub Circuit has an exact four-edge Hub-and-spokes graph. It has no Start → Anchor Shortcut. Each semantic room has one separate branch. Broken Ring and Twin Wings can add their one recipe-specific optional edge.

Five-arena small-map candidate validation requires a normalized direction dot product of `0.42F` or less between the Start and Anchor transitions from the Hub. This rule does not apply to large-map candidates.

Large-map planning uses these rules:

- Select Hub and Spokes, Ring and Branches, Main Spine, Twin Districts, or Dense Core/Sparse Branch before candidate routing.
- Publish required typed edges for the selected archetype.
- Publish one Cycle edge and one Shortcut edge for each production graph.
- Require both edges to meet the useful-cycle definition.
- Require the Shortcut to save at least two arena transitions.

Candidate validation checks:

- Degree constraints.
- Cycle constraints.
- Junction constraints.
- Branch-depth constraints.
- The Start-to-Exit signature.

`RoomLayout` publishes:

- Substantial-room and connector counts.
- Contracted and direct edge counts.
- The direct-edge ratio.
- The degree histogram and maximum degree.
- Junction count and cycle rank.
- Useful-cycle count and branch depth.
- Start-to-Exit distance.
- The longest alternating room and connector chain.

F2 shows the archetype and key metrics. Compact four-arena layouts use the spatial-tree fallback. They do not report a `LargeMapArchetype`.

Initial archetypes:

| Archetype | Structural identity |
|---|---|
| Hub and Spokes | One high-degree central arena, several branches, and optional branch-to-branch Shortcut |
| Ring and Branches | A readable circulation loop with attached leaves or short spurs |
| Main Spine | A Start-to-Exit route with asymmetrical side branches and selected cross-links |
| Twin Districts | Two dense room clusters joined by one bridge or physically separated paired routes |
| Dense Core/Sparse Branch | Several directly adjacent arenas opening into a longer isolated route |

Archetypes must differ by more than seed placement. Each archetype defines degree patterns and cycle requirements. It also defines branch depth, direct arena adjacency, and connector use.

### Anti-alternation rules

Candidate validation and scoring must measure:

- The fraction of arena-to-arena, arena-to-connector, and connector-to-connector doorway edges.
- The longest alternating arena and connector chain.
- Connector count relative to substantial-room count.
- Multi-entry substantial-room count and ratio.
- Degree histogram and junction count.
- Useful-cycle count and Shortcut savings.
- Ordinary Combat leaves and dead-end depth.
- Physical separation of alternate routes.
- Spatial clustering and graph-community separation.

The current code prohibits connector-to-connector doorways. Change this rule only if an archetype needs a real corridor junction. Keep connector traversal and doorway publication explicit.

Current circulation status:

- The implementation enforces exactly two useful cycles.
- It enforces minimum Shortcut savings of two arena transitions.
- It does not enforce 70% multi-entry rooms.
- It does not bound Combat leaves for every archetype.
- It does not measure physical route separation.

## Slice 3: random new-match generation and physical scale — foundation complete

Normal match generation:

- Takes `physicalProfile` from `MatchGenerationRequest`.
- Takes grid radius and world scale from the selected physical profile.
- In `GeneratedLevelConfig`, `matchSeed` and the attempt index derive only the grid seed and room seed.
- Derives the grid seed from `matchSeed` and the attempt index.
- Derives the room seed from `matchSeed` and the attempt index.
- Does not derive the physical profile from `matchSeed`.
- Uses an eight-attempt candidate stream for the default request.
- Reproduces accepted inputs, profile, layout, retry count, and fallback status in the same build and toolchain.
- Uses different derived inputs for different match seeds.

`R` resets the match without generation. `N` requests a new seed. `--seed` supports replay. F2 and tests keep fixed representative configurations. The game uses a fallback only after bounded failure. It marks the fallback clearly.

Fortress V1 increases extent and physical gameplay scale. It uses radius 8 and `worldScale = 0.22`. The systems fixture uses radius 5 and `0.16`. Player and enemy bodies do not change.

Acceptance measures:

- Minimum doorway width.
- Substantial-room and Anchor area.
- Objective clearance.
- Anchor room-center span.
- Start-to-Exit route distance.
- Usable cross-room ingress separation.
- Spawn candidates and spawn-bearing rooms.
- Hub doorway degree.

These measurements use player-relative gameplay units. Radius-only growth fails. Large maps require a Hub with at least three doorway edges. They prefer a leaf arena for Reward semantics.

The camera uses a separate 25-unit orthographic view. The implementation does not use one global scale multiplier.

Physical-foundation limits:

- This work does not prove final pacing or map identity.
- Anchor span is a size proxy.
- Anchor span does not prove line of sight.
- Static spawn capacity uses all-open reachability and immutable-wall clearance.
- Static spawn capacity does not use current locks, player position, or occupancy.

Implemented admission checks:

- Progression-stage checks validate the complete gate sequence.
- The static post-gate spawn-packing check validates packed slots in post-gate states.
- The static post-gate spawn-packing check validates spawn-bearing room count in post-gate states.
- The static post-gate spawn-packing check does not run the spawn system.
- The static post-gate spawn-packing check does not prove runtime spawn success.

Remaining physical and runtime work:

- Initial-lock spawn capacity.
- Visibility bands and role compatibility.
- Runtime occupancy and spawn recovery.
- Opening-component circulation and economy validation.
- Connector measurements and true sightline bands.
- Movement, dash, and projectile reach.
- Interactions, lighting, and detail density.
- Endurance and navigation performance.

## Slice 4: room-shape grammar

Select a geometry grammar for each shooter arena. Match the grammar to its cells, role, doorway brief, and topology position.

Initial room shapes:

| Shape | Gameplay identity |
|---|---|
| Compact | Circular training route and broad sightlines |
| Elongated | Strong directional pressure and ranged lanes |
| L-shaped | Broken sightlines and a protected inner corner |
| Concave pocket | A risky recess, service alcove, or objective site |
| Twin-lobed | Two combat pockets joined by a wide internal neck |
| Perimeter route | Central obstruction or negative-space island creates circulation around an interior feature |
| Crossroads | Three or more entrances organize movement around a readable center |

Publish the selected shape identity. Do not make the renderer guess it from area. Validate compactness, elongation, concavity, and lobe or neck structure. Also validate doorway count, doorway angular spread, and usable local clearance.

Each map must contain several different substantial-room signatures. A cell-count change alone does not create a separate signature. A rotation of the same shape also does not create a separate signature.

Room and combat grammar is still a current gap. Dedicated lane, perimeter, defended-center, and broken-sightline builders do not exist yet.

## Slice 5: landmarks, districts, quests, and presentation — small-map baseline complete

The radius-5 runtime has deterministic Start, Hub, Anchor, Reward-relay, gate, and Exit landmarks. It also has higher-contrast role-aware surfaces.

Production generation must publish typed, shape-aware semantic anchors. It must bind recipe devices after candidate acceptance. A quest recipe can constrain role, shape, separation, route order, clearance, visibility, or optionality. It must not name fixed room IDs or world coordinates. Reject a candidate if it cannot realize the selected quest.

Current and planned landmark language:

- Start: clear spawn marker and opening-area boundary.
- Hub: central machine anchor that is visible from several approaches.
- Combat: cover and perimeter features that support the selected room shape.
- Connector: threshold frames, directional lighting, and chokepoint treatment.
- Reward: protected focal object and separate local symbol.
- Exit: large finale or extraction monument that is visible before activation.

The generator or level package must provide deterministic semantic anchors. Examples are high-clearance focal cells and doorway-facing points. Other examples are perimeter candidates and central-obstruction candidates. `GameRenderer` can make temporary primitive landmarks from these anchors. Gameplay code must not own graphics resources.

District palettes and symbols must support topology clusters. They must not replace room-role readability. Do not use color as the only signal. Also use silhouette, placement, scale, and symbols.

Typed tactical and presentation anchors are still a gap. The full quest compiler is also a gap. The current gate and Anchor binder does not compile all curated recipes to immutable semantic anchors.

## Validation strategy

### Per-layout structural checks

- Reproduce the match seed, derived inputs, accepted candidate, physical-scale profile, and quest binding.
- Satisfy archetype-specific graph constraints.
- Require exactly two useful production cycles.
- Require minimum Shortcut savings of two arena transitions.
- Keep every assigned cell reachable through authorized doorway pairs.
- Keep Start and Exit separate. Make them meet the configured distance limit.
- Match connectors and direct arena links to the selected archetype.
- Make substantial rooms satisfy their published shape grammar.
- Keep doorway width and actor clearance valid.
- Keep landmark anchors in their published room.
- Keep anchors clear of retained thresholds.
- Make world-space rooms and routes larger relative to unchanged actors than the radius-5 baseline.
- Validate progression stages and the static post-gate spawn-packing check.
- Do not treat the static post-gate spawn-packing check as runtime spawn proof.
- Validate initial-lock capacity, visibility, role compatibility, runtime recovery, endurance, and navigation when those checks are implemented.

### Cross-seed diversity checks

Use a fixed matrix of grid radii, grid seeds, and room seeds. Record a structural signature with at least:

- Topology archetype.
- Room and connector counts.
- Direct arena-edge ratio.
- Degree histogram and useful-cycle count.
- Shortcut savings, Combat leaves, and dead-end depth.
- Start-to-Exit distance and branch-depth distribution.
- Room-shape histogram.
- Compactness, elongation, concavity, and area distributions.

Reject an accidental collapse to one signature. Do not require random nondeterminism. Identical inputs must produce identical layouts, metadata, and quality scores.

### Visual review

Use the overview to make a repeatable screenshot matrix for representative seeds. Review silhouettes and graph readability at full-map scale. Review room-shape contrast, landmark hierarchy, and district identity. Then verify traversal and combat readability with the normal camera.

## Acceptance check

1. Start consecutive new matches. Confirm that they normally produce different valid maps.
2. Start the same match seed again. Confirm exact geometry, scale, accepted retry, and topology.
3. Confirm exactly two useful cycles.
4. Confirm that the Shortcut saves at least two arena transitions.
5. Press `R`. Confirm that the map does not change.
6. Compare silhouette, topology, room-shape distribution, and landmark hierarchy.
7. Confirm that arena and connector alternation does not dominate the layout.
8. Confirm that rooms and routes are larger relative to actor bodies than in the systems slice.
9. Confirm that radius-only growth fails the physical-scale checks.
10. Confirm exact walls, authorized doorways, navigation, spawn clearance, bounded fallback, and runtime reset.
11. Confirm the progression-stage reports and the report for the static post-gate spawn-packing check.

This acceptance check includes planned data that is not complete. Exact semantic-anchor and quest-binding replay remains a target. Room grammar, typed semantic anchors, and the full quest compiler remain gaps.

Completed work:

- Crowd navigation and local separation.
- Deterministic spawn pacing and enemy roles.
- The endless director.
- Random large-map generation.
- The first physical-scaling pass.
- The first large-map topology pass.
- Exactly two useful cycles on each production map.
- Minimum Shortcut savings of two arena transitions.
- Progression-stage checks.
- The static post-gate spawn-packing check.

The static post-gate spawn-packing check does not prove runtime spawn behavior.

Next work:

- Bound Combat leaves and dead ends.
- Measure physical route separation.
- Add room and combat grammar.
- Add typed semantic anchors and the full quest compiler.
- Add initial-lock, visibility, and role spawn checks.
- Add bounded runtime recovery.
- Add endurance and navigation validation.
