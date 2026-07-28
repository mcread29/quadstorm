# Level Identity and Legibility Pass

This document defines the next implementation slice for the generated game map. It is intentionally scheduled before crowd navigation and round pacing because combat systems cannot make a structurally repetitive, visually anonymous map learnable.

The current shooter generator is valid, deterministic, physically measured, and door-aware, but its accepted layouts often read as variations of the same sequence: arena → connector → arena → connector. The runtime follow camera hides the complete silhouette and circulation graph, while most substantial rooms use similar compact growth and only subtle role colors. Together these produce a “bowl of oatmeal” effect: local irregularity without memorable global or local identity.

For the implemented generator pipeline, see [`shooter-level-generation.md`](shooter-level-generation.md). For current scoring and validation, see [`layout-quality-and-testing.md`](layout-quality-and-testing.md). Runtime invariants remain in [`game-handoff.md`](game-handoff.md).

## Current baseline

- The active game constructs the curated small-map `GeneratedLevelConfig`: radius 5, grid seed 1, and Hub Circuit room seed 7. The other CLI recipe presets use room seeds 2 and 3.
- The gameplay camera follows the player. F2 now opens a developer whole-level overview with exact runtime floor, boundaries, role/ID labels, the published room graph, open/locked thresholds, role markers, player position, and generator metadata.
- Left/Right browses six fixed representative configurations as read-only previews; Home returns to the active session. Preview browsing pauses and never replaces or mutates the active simulation.
- Until later slices publish archetype and room-shape metadata, the overview explicitly labels the current derived topology and compact/routed baseline rather than pretending the planned grammar already exists.
- Radius-5 shooter mission graphs select Hub Circuit, Broken Ring, or Twin Wings before candidate placement and routing. Larger maps retain the spatial-tree planner while broader archetypes are developed.
- At least one explicit connector is required; long routed edges become connector regions.
- Small-map recipe edges and semantic branch placement are validated, while larger-map direct links and occasional dense clusters still do not select or validate a strong map-level archetype.
- Most shooter arenas grow from the same compact-room process. Roles describe gameplay purpose, not a distinct geometry grammar.
- Runtime room identity is currently communicated mainly through modest floor-color differences and a role label.

The identity pass must preserve deterministic generation, exact dual geometry, authorized doorway pairs, complete reachability, physical clearance, Start/Exit guarantees, and the immutable `GeneratedLevel` boundary.

## Design principles

1. **Global structure must be readable before decoration.** A palette swap cannot repair an alternating chain.
2. **Connectors are notable spatial events, not mandatory separators between every pair of rooms.**
3. **Room role and room shape are independent.** A Combat room may be compact, elongated, concave, split, or multi-entrance while remaining a Combat room.
4. **Generated variation must remain testable.** Archetype labels and geometry signatures should be published or reproducibly derived rather than inferred by screenshots alone.
5. **Runtime landmarks use semantic anchors, never hand-authored world coordinates.**
6. **The overview is an iteration tool first.** A later player-facing map may add discovery or fog rules without weakening the debug view.

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

## Slice 2: topology archetypes — complete for the small-map tier

Radius-5 maps choose Hub Circuit, Broken Ring, or Twin Wings before candidate placement and route materialization. Physical routing rejects candidates that cannot realize the selected graph safely. Hub Circuit is an exact four-edge Hub-and-spokes graph with no Start → Anchor shortcut; each semantic room owns one distinct branch. Broken Ring and Twin Wings may add their one recipe-specific optional edge. Candidate validation also requires the Start and Anchor transitions to leave the Hub at least approximately 65 degrees apart, preventing semantically different routes from collapsing into adjacent doors. The larger initial archetype table remains the expansion target beyond this vertical slice.

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

## Slice 3: room-shape grammar

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

## Slice 4: landmarks, districts, and presentation

Once graph and room geometry carry identity, add role- and shape-aware runtime presentation.

Planned landmark language:

- Start: unmistakable spawn marker and opening-area boundary.
- Hub: central machine anchor visible from multiple approaches.
- Combat: cover/perimeter features that reinforce the selected room shape.
- Connector: threshold frames, directional lighting, and chokepoint treatment.
- Reward: tucked-away focal object and distinct local symbol.
- Exit: large finale/extraction monument visible before activation.

The generator or level package should provide deterministic semantic anchors such as high-clearance focal cells, doorway-facing points, perimeter candidates, and central obstruction candidates. `PrototypeRenderer` may turn these into temporary primitive-based landmarks; gameplay code must not own graphics resources.

District palettes and symbols should reinforce topology clusters without replacing room-role readability. Color must be supplemented by silhouette, placement, scale, and symbols.

## Validation strategy

### Per-layout structural checks

- Archetype-specific graph constraints are satisfied.
- Every assigned cell remains reachable through authorized doorway pairs.
- Start and Exit remain distinct and meaningfully separated.
- Connectors and direct arena links match the chosen archetype.
- Substantial rooms satisfy their published shape grammar.
- Doorway width and actor-clearance guarantees remain intact.
- Landmark anchors belong to the advertised room and do not obstruct retained thresholds.

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

From the full-level overview, representative accepted layouts are distinguishable at a glance by silhouette, topology archetype, room-shape distribution, and landmark hierarchy. No representative layout is dominated by a repetitive arena/connector alternation. Each map contains multiple mechanically distinct substantial-room geometries, while exact walls, authorized doorways, navigation, spawn clearance, deterministic generation, and runtime reset remain correct.

Crowd navigation, local separation, deterministic spawn pacing, enemy roles, and the small-map round director are now complete. After the remaining identity gate passes, continue with puzzle-specific room grammar, authored clue families, and broader recipes rather than adding more seed-only variants.
