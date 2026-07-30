# Random Large-Map Identity, Scale, and Legibility Pass

> Historical status: superseded by [`level-generation-lock-in.md`](level-generation-lock-in.md). The first topology pass improved metadata and diagnostics but did not solve the oatmeal problem. Preserve this document for implementation history; do not use its slice ordering as the active roadmap.

This document defines the earlier generation slice for the game map. The full-level overview, three small-map systems fixtures, replayable new-match boundary, first physically larger Fortress V1 profile, and first large-map topology-archetype pass are complete. Normal generation combines radius 8 with `worldScale = 0.22`, actor-relative acceptance, and one of five validated graph briefs rather than merely increasing hex radius. Room-shape grammar, districts, semantic quest anchors, opening-component validation, and pacing remain because scale and graph variation alone cannot make every room compelling.

The current shooter generator is valid, deterministic, physically measured, and door-aware, but its accepted layouts often read as variations of the same sequence: arena → connector → arena → connector. The runtime follow camera hides the complete silhouette and circulation graph, while most substantial rooms use similar compact growth and only subtle role colors. Together these produce a “bowl of oatmeal” effect: local irregularity without memorable global or local identity.

For the implemented generator pipeline, see [`shooter-level-generation.md`](shooter-level-generation.md). For current scoring and validation, see [`layout-quality-and-testing.md`](layout-quality-and-testing.md). Runtime invariants remain in [`game-handoff.md`](game-handoff.md).

## Current baseline

- The active game creates a fresh public match seed and deterministically derives radius-8 Fortress V1 grid/room inputs, with eight bounded attempts and a visible radius-5 Hub Circuit fixture fallback. `--seed` reproduces an accepted match within the same build/toolchain, `N` generates another, and `R` preserves the current map. Fortress V1 uses `worldScale = 0.22`; fixed CLI recipes and F2 configurations remain radius-5, `0.16` regression presets.
- The gameplay camera follows the player. F2 now opens a developer whole-level overview with exact runtime floor, boundaries, role/ID labels, the published room graph, open/locked thresholds, role markers, player position, and generator metadata.
- Left/Right browses six fixed representative configurations as read-only previews; Home returns to the active session. Preview browsing pauses and never replaces or mutates the active simulation.
- Until later slices publish archetype and room-shape metadata, the overview explicitly labels the current derived topology and compact/routed baseline rather than pretending the planned grammar already exists.
- Radius-5 shooter mission graphs select Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing. Larger maps retain the spatial-tree planner while broader archetypes are developed.
- At least one explicit connector is required; long routed edges become connector regions.
- Small-map recipe edges and semantic branch placement are validated, while larger-map direct links and occasional dense clusters still do not select or validate a strong map-level archetype.
- Most shooter arenas grow from the same compact-room process. Roles describe gameplay purpose, not a distinct geometry grammar.
- Runtime room identity now includes stronger role-aware floors, sparse floor traces, wall caps, semantic objective landmarks, and a role label. Broader room-shape and district metadata still does not exist.

The pass must preserve deterministic generation for a requested match seed, exact dual geometry, authorized doorway pairs, complete reachability, physical clearance, Start/Exit guarantees, and the immutable `GeneratedLevel` boundary. A new match normally chooses a fresh seed; restart retains the accepted map; fixed seeds remain replay, regression, and fallback tools.

## Design principles

1. **Global structure must be readable before decoration.** A palette swap cannot repair an alternating chain.
2. **Connectors are notable spatial events, not mandatory separators between every pair of rooms.**
3. **Room role and room shape are independent.** A Combat room may be compact, elongated, concave, split, or multi-entrance while remaining a Combat room.
4. **Generated variation must remain testable.** Archetype labels and geometry signatures should be published or reproducibly derived rather than inferred by screenshots alone.
5. **Runtime landmarks use semantic anchors, never hand-authored world coordinates.**
6. **The overview is an iteration tool first.** A later player-facing map may add discovery or fog rules without weakening the debug view.
7. **Map extent and physical scale are separate controls.** Radius controls available cell count; generated-to-world scale controls room, doorway, and route size relative to actors. Production generation must increase both where needed.
8. **Random does not mean unvalidated.** Every new-match seed is deterministic, candidates are retried within a bounded budget, and only geometry-, endurance-, economy-, and quest-valid maps enter play.

## Slice 1: full-level overview and seed browser — complete

The runtime debug view fits the complete generated floor onscreen and exposes the structure hidden by the follow camera.

It should show:

- Exact floor silhouette and room boundaries.
- Room role, stable room ID, and geometry/archetype label.
- Published doorway thresholds and the room connectivity graph.
- Start, Hub, Reward, and Exit markers.
- Current grid seed, room seed, candidate index, and quality score.
- Locked versus open thresholds when viewing an active session.
- Controls to move through a deterministic set of representative configurations without changing seeds implicitly during simulation.

The first version is a developer view, not the final player map. It reveals the complete layout. The fixed browser configurations live in `REPRESENTATIVE_LEVEL_CONFIGS`, and generated-level tests ensure that each remains unique, deterministic, and valid. If a player-facing tactical map is added later, unexplored-room rules should be layered separately.

## Slice 2: topology archetypes — first large-map pass complete

Radius-5 maps choose Hub Circuit, Broken Ring, or Twin Wings before candidate placement and route materialization. Physical routing rejects candidates that cannot realize the selected graph safely. Hub Circuit is an exact four-edge Hub-and-spokes graph with no Start → Anchor shortcut; each semantic room owns one distinct branch. Broken Ring and Twin Wings may add their one recipe-specific optional edge. Candidate validation also requires the Start and Anchor transitions to leave the Hub at least approximately 65 degrees apart, preventing semantically different routes from collapsing into adjacent doors. Larger layouts now deterministically choose Hub and Spokes, Ring and Branches, Main Spine, Twin Districts, or Dense Core/Sparse Branch before candidate routing. Each planner publishes a required tree plus its declared optional loop, and candidate validation enforces the selected degree, cycle, junction, branch-depth, and Start-to-Exit signature. `RoomLayout` publishes substantial/connector counts, contracted/direct edge counts and ratio, degree histogram, maximum degree, junction count, cycle rank, branch depth, Start-to-Exit distance, and longest alternating room/connector chain. F2 displays the selected archetype and key metrics. Compact four-arena layouts retain the reliable spatial-tree fallback rather than misreporting a large archetype.

Initial archetypes:

| Archetype | Structural identity |
|---|---|
| Hub and spokes | One high-degree central arena, several branches, and optional branch-to-branch shortcut |
| Ring and branches | A readable circulation loop with attached leaves or short spurs |
| Main spine | A strong Start-to-Exit route with asymmetrical side branches and selected cross-links |
| Twin districts | Two dense room clusters joined by one memorable bridge or paired routes |
| Dense cluster to sparse branch | Several directly adjacent arenas opening into a longer isolated route |

Archetypes must vary more than seed placement. Each defines expected degree patterns, cycle rank, branch depth, direct arena adjacency, and connector usage.

### Anti-alternation rules

Candidate validation and scoring should measure:

- Fraction of doorway edges that are arena-to-arena, arena-to-connector, and connector-to-connector.
- Longest alternating arena/connector chain.
- Connector count relative to substantial-room count.
- Count of multi-door substantial rooms.
- Degree histogram and number of meaningful junctions.
- Cycle rank and alternate-route usefulness.
- Spatial clustering and graph-community separation.

The current prohibition on connector-to-connector doorways may be revisited only if a topology archetype needs a genuine corridor junction. It must not be relaxed accidentally. Any change must keep connector traversal and doorway publication explicit.

## Slice 3: random new-match generation and physical scale — foundation complete

Normal gameplay derives grid seed, room seed, Fortress V1 profile, and an eight-attempt candidate stream from one match seed. The same seed reproduces accepted inputs, profile, layout, retry count, and fallback status within the same build/toolchain; different seeds vary normal generation. `R` resets without regenerating, `N` requests a new seed, `--seed` supports replay, and fixed representative configurations remain in F2 and tests. Fallback is used and visibly marked only after bounded failure.

Fortress V1 increases both extent and physical gameplay scale: radius 8 and `worldScale = 0.22` replace the normal radius-5/`0.16` systems footprint while player and enemy bodies remain unchanged. Acceptance measures minimum doorway width, substantial and Anchor room area, objective clearance, Anchor room-center span, Start-to-Exit route distance, statically usable cross-room ingress separation, usable spawn candidates, spawn-bearing rooms, and Hub doorway degree in player-relative/gameplay units. Radius-only growth fails explicitly. Larger maps now require a Hub with at least three doorway edges and prefer a leaf arena for Reward semantics. Camera framing was widened independently to a 25-unit orthographic view rather than applying one global multiplier.

This is a physical foundation, not final pacing or map identity. Anchor span is a size proxy rather than line-of-sight proof; static spawn capacity uses all-open reachability and immutable-wall clearance rather than current lock/player/occupancy state. Connector-specific metrics, true sightline bands, opening-component circulation/economy, movement/dash, projectile reach, interactions, lighting, detail density, and navigation performance remain. Quest recipe and semantic-anchor derivation are also pending; the current boundary binds and validates the existing Anchor/Hub/relay/Exit systems plan.

## Slice 4: room-shape grammar

Shooter arenas should select a geometry grammar compatible with their available cells, role, doorway brief, and topology position.

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

Generation should publish the selected shape identity rather than forcing the renderer to guess it from area alone. Shape validation should measure compactness, elongation, concavity, lobe/neck structure, doorway count, doorway angular spread, and usable local clearance.

A map should contain several distinct substantial-room signatures. Changing only cell count or rotating an otherwise identical blob does not count as meaningful variety.

## Slice 5: landmarks, districts, quests, and presentation — small-map baseline complete

The radius-5 runtime has deterministic Start, Hub, Anchor, Reward-relay, gate, and Exit landmarks plus stronger role-aware surface treatment. Production generation must publish shape-aware semantic anchors and bind recipe-authored quest devices after candidate acceptance. A quest recipe may constrain role, shape, separation, route order, clearance, or optionality, but must not name fixed room IDs or world coordinates. Candidates that cannot realize the selected quest are invalid.

Current and planned landmark language:

- Start: unmistakable spawn marker and opening-area boundary.
- Hub: central machine anchor visible from multiple approaches.
- Combat: cover/perimeter features that reinforce the selected room shape.
- Connector: threshold frames, directional lighting, and chokepoint treatment.
- Reward: tucked-away focal object and distinct local symbol.
- Exit: large finale/extraction monument visible before activation.

The generator or level package should provide deterministic semantic anchors such as high-clearance focal cells, doorway-facing points, perimeter candidates, and central obstruction candidates. `GameRenderer` may turn these into temporary primitive-based landmarks; gameplay code must not own graphics resources.

District palettes and symbols should reinforce topology clusters without replacing room-role readability. Color must be supplemented by silhouette, placement, scale, and symbols.

## Validation strategy

### Per-layout structural checks

- Match seed, derived inputs, accepted candidate, physical-scale profile, and quest binding are reproducible.
- Archetype-specific graph constraints are satisfied.
- Every assigned cell remains reachable through authorized doorway pairs.
- Start and Exit remain distinct and meaningfully separated.
- Connectors and direct arena links match the chosen archetype.
- Substantial rooms satisfy their published shape grammar.
- Doorway width and actor-clearance guarantees remain intact.
- Landmark anchors belong to the advertised room and do not obstruct retained thresholds.
- World-space room and route dimensions are larger relative to unchanged actors than the radius-5 systems baseline.
- Opening and expanded components satisfy configured ingress, population, circulation, sightline, and quest-footprint capacity.

### Cross-seed diversity checks

Use a fixed representative matrix of grid radii, grid seeds, and room seeds. Record a structural signature containing at least:

- Topology archetype.
- Room and connector counts.
- Direct arena-edge ratio.
- Degree histogram and cycle rank.
- Start-to-Exit distance and branch-depth distribution.
- Room-shape histogram.
- Compactness, elongation, concavity, and area distributions.

Tests should reject accidental collapse toward one signature. They should not demand random nondeterminism: identical inputs must still produce identical layouts, metadata, and quality scores.

### Visual review

The overview should support a repeatable screenshot matrix for representative seeds. Review silhouettes, graph readability, room-shape contrast, landmark hierarchy, and district identity at full-map scale, then verify traversal and combat readability from the normal camera.

## Acceptance check

Consecutive new matches normally produce different fully valid maps, while the same match seed reproduces geometry, scale, accepted retry, topology, semantic anchors, and quest binding exactly. `R` preserves that map. Accepted layouts are distinguishable by silhouette, topology, room-shape distribution, and landmark hierarchy; no layout is dominated by repetitive arena/connector alternation. Rooms and routes are demonstrably larger relative to actor bodies than the systems slice, and radius-only growth cannot satisfy the physical-scale checks. Exact walls, authorized doorways, navigation, spawn clearance, bounded fallback, and runtime reset remain correct.

Crowd navigation, local separation, deterministic spawn pacing, enemy roles, the endless director, random large-map generation, first physical scaling, and the first large-map topology pass are complete. The next work should establish room-shape identity, use the published graph metrics in anti-repetition scoring, add quest-aware semantic anchors and binding, and validate opening-component/larger-map pacing before deeper clue families.
