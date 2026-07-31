# Stålberg grid and 2.5D game (raylib)

This file uses ASD-STE100 Simplified Technical English for its prose.

This repository contains a compact C++ implementation of the grid-generation method from Oskar Stålberg's *Townscaper*.

The grid generator does these steps:

1. It generates a triangular lattice with a hexagonal boundary.
2. It randomly pairs adjacent triangles into four-sided faces.
3. It subdivides each triangle and four-sided face.
4. It uses shared edge midpoints and one face center for subdivision.
5. It applies iterative Laplacian relaxation.
6. It keeps the hexagonal boundary fixed during relaxation.

The subdivision step makes only quads in the final mesh.
This rule also applies to unmatched triangles.

The room generator runs as a separate pass.
It uses a neutral cell graph.
The graph contains physical cell area, clearance, traversal distance, shared-boundary width, and explicit entrance candidates.
The default shooter method plans a mission graph before it embeds rooms.
It embeds several combat arenas.
It can make a dense cluster of two or three rooms.
It can make one larger landmark room when the map has sufficient space.
It adds explicit corridor regions where space permits.
It also adds side branches and a connected Start-to-Exit route.
Production radius-8 maps now require exactly two useful cycles.
Each production Shortcut must save at least two arena transitions.
The legacy branching-shape and organic-growth methods are still available.
Each method generates deterministic candidates and keeps the valid candidate with the highest score.

`MatchGenerator` supplies the procedural map for normal game startup.
It validates and ranks generated candidates.
Each new match starts from one replayable match seed.
Each generator attempt derives fresh grid and room seeds from this seed.
`MatchGenerationRequest` selects the physical profile.
The request defaults to **Fortress V1**.
`MatchGenerationRequest::attemptBudget` sets the generator-attempt limit.
Its default value is eight.
The generator publishes the validated Systems fallback after attempt exhaustion.
Fortress V1 uses radius 8 and `worldScale = 0.22`.
It publishes Hub and optional Reward semantics on larger layouts.
Match admission rejects maps that fail geometry, route, objective, ingress, spawn-capacity, or progression-stage requirements.
Match admission also applies the static post-gate spawn-packing check.
Fixed representative seeds are regression fixtures and an emergency fallback.
They are not the normal content model.
Use the F2 overview and seed browser to inspect maps.

The latest audit sent 100 production requests to the match generator.
Six of the 100 requests used fallback.
The generator used a mean of 3.29 attempts across all 100 requests.
The other audit values describe the 94 accepted Fortress maps.
These maps had `mean score=58.751` and `useful cycles=2.00`.
They had `multi-entry substantial rooms=66.7%` and `progression safe=100%`.
All 94 maps passed the static post-gate spawn-packing check (`100%`).

The repository also contains a complete small-map **systems** vertical slice.
It is a top-down 2.5D round-based horde shooter.
The `stalberg_game` executable starts with a fresh replayable match seed.
The generator returns an accepted Fortress V1 map or the validated Systems fallback.
It then runs a persistent automatic endless match.
The match has bounded pressure, points, gates, hordes, objectives, upgrades, and voluntary extraction.
`R` restarts mutable state on the current map.
`N` creates a new match.
The three radius-5 presets use `worldScale = 0.16`.
They remain regression and fallback fixtures.

Fortress V1 increases the map extent and physical geometry.
It does not change player and enemy body sizes.
Validation measures doorway width and room area.
It measures substantial-room and Anchor-room properties.
It also measures objective clearance and Anchor-room span.
It measures Start-to-Exit route distance and usable cross-room ingress separation.
It measures usable spawn candidates and spawn-bearing rooms.
Progression-stage checks reject gate sequences that block required progress.
The static post-gate spawn-packing check requires 18 packed spawn slots in at least two rooms for each checked post-gate state.
This check does not prove runtime spawn placement.
Runtime placement also uses player distance, active walls, occupancy, and a larger separation distance.
Radius-only growth fails validation.
Fortress V1 uses a wider gameplay-camera framing profile.
Camera framing is independent of `worldScale`.

The remaining generation work includes bounded ordinary Combat leaves and limited dead-end depth.
It includes physical route separation and room-shape and combat grammar.
It includes semantic anchors and a quest compiler.
It includes initial-lock spawn, visibility-band, and spawn-role compatibility checks.
It includes runtime spawn recovery, endurance, and navigation work.
It also includes a portable project RNG.
Movement and projectile ranges still need large-map playtests.
Interaction ranges and economy pacing still need large-map playtests.
Lighting, detail density, and navigation performance also need large-map playtests.

## Documentation

- [`docs/level-generation-lock-in.md`](docs/level-generation-lock-in.md) is the only active roadmap.
- [`docs/game-roadmap.md`](docs/game-roadmap.md) is a summary of the active roadmap and redirects readers to it.
- [`docs/game-handoff.md`](docs/game-handoff.md) gives the current runtime architecture, decisions, limits, and next work.
- [`docs/level-identity-pass.md`](docs/level-identity-pass.md) records the first large-map identity pass.
- [`docs/small-puzzle-horde-slice.md`](docs/small-puzzle-horde-slice.md) gives interactive and headless acceptance steps for recipes, economy, rounds, puzzles, enemies, and upgrades.
- [`docs/demo-and-algorithm.md`](docs/demo-and-algorithm.md) gives the mesh mathematics, topology, relaxation, rendering, and source map.
- [`docs/room-generation-model.md`](docs/room-generation-model.md) gives the neutral physical input, output API, roles, doorways, and gameplay contract.
- [`docs/shooter-level-generation.md`](docs/shooter-level-generation.md) gives the graph-first arena, route, corridor, entrance, doorway, and tactical-annotation pipeline.
- [`docs/layout-quality-and-testing.md`](docs/layout-quality-and-testing.md) gives candidate validation, scoring formulas, fallback behavior, and test coverage.

## Build

The build uses a system raylib installation when one is available.
Otherwise, CMake downloads raylib 5.5.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/stalberg_game       # generated 2.5D traversal + combat regression
./build/stalberg_grid       # procedural-generation diagnostic demo
```

### Web game build

Install and activate the Emscripten SDK.
Then use its CMake wrapper.

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd /path/to/stalberg-grid

emcmake cmake -S . -B build-web \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF
cmake --build build-web --target stalberg_game -j
python3 -m http.server 8000 --directory build-web
```

Open `http://localhost:8000/stalberg_game.html`.
The web target uses a fixed 16:10 framebuffer.
CSS scales the framebuffer uniformly.
This keeps letterboxing, rendering, and mouse coordinates aligned at different browser sizes.
The target uses an Emscripten browser main loop and WebGL-compatible shaders.
The web configuration does not build the native grid diagnostic or native test executables.
The web build disables combat audio.
The raylib 5.5 ScriptProcessor backend cannot start safely before a browser user gesture.
Native builds keep combat audio.
The HUD uses the bundled ComicShannsMono Nerd Font Mono.
The MIT license for the font is in `assets/fonts/`.

The runtime starts in the generated horde match.
`GeneratedLevel` keeps the relaxed source grid, exact dual geometry, neutral room graph, shooter layout, and exact doorway threshold segments together.
Assigned dual polygons make a cached floor mesh.
Floor edges, void edges, and unauthorized cross-room contacts make walls.
Only published doorway cell pairs stay open.
`LevelSession` keeps mutable player, room-lifecycle, lock, and active-wall state separate.
The player starts in a high-clearance cell in Start.
The player uses the matching door-aware navigation graph.
The player cannot leave the floor.

Use **WASD** to move.
Use **Space** to dash.
Use the **mouse** to aim.
Hold the **left mouse button** to fire.
Round 1 starts after a three-second countdown.
Each cleared round starts a five-second intermission.
The next round starts automatically.
Press **E** to buy a nearby gate, activate a device, or repair at the powered Hub.
Gate prompts and gate purchases use the same 2.2-world-unit range.
Use **1/2/3** at the Hub to buy upgrades.
Press **R** to restart the current map.
Press **N** to generate a new match.
Press **F1** to open the combat regression arena.
Press **F2** to open the fitted full-level overview.
Use Left/Right to browse six fixed regression layouts.
Press Home to return to the active session.
`--seed=<unsigned decimal>` reproduces a match with the same game build and toolchain.
Replay across different standard libraries is not guaranteed.
Lower-level generation still uses standard-library shuffle and distribution algorithms.
`--recipe=hub|ring|wings` starts a fixed fixture for inspection.
Do not use `--recipe` with `--seed`.
Simulation runs at a fixed 120 Hz.
Rendering interpolates simulation state.

Debug builds use debugger-friendly optimization for the game runtime and bundled raylib.
This keeps frame pacing representative while symbols and assertions stay enabled.
Configure with `-DSTALBERG_OPTIMIZE_DEBUG_RUNTIME=OFF` for fully unoptimized stepping.

Grid generation and room generation are independent libraries.
The room library does not depend on `StalbergGrid`.
`src/integration/room_grid_adapter.cpp` converts the generated mesh to the room module's owned `RoomGrid` snapshot.
Grid-specific policy stays in the grid and adapter layers.
This policy includes dual-cell measurement and selection of entrance candidates from the six-sided boundary.
The demo completes relaxation before it creates the snapshot.
This keeps visual geometry, room scores, and physical metrics in agreement.
Dedicated headless tests cover generation, generated-level runtime and session behavior, and combat simulation.
Runtime tests cover dash cooldown and wall collision.
They cover traversal firing and preservation of shots in flight.
They cover deterministic multi-spawn identity and order.
They cover earliest enemy-hit selection and all-enemies-clear doorway transitions.
They cover hostile-shot cleanup, closed-wall containment, reset behavior, and custom encounter walls.

## Game controls

| Input | Action |
|---|---|
| WASD | Move relative to the camera |
| Space | Dash in the movement direction, or facing direction while stationary |
| Mouse | Aim on the ground plane |
| Hold left mouse button | Fire on the generated map or in the combat regression arena |
| E | Buy a gate within the displayed 2.2-unit range, activate Anchor/Hub/Exit, or repair one missing health at a powered Hub |
| 1 / 2 / 3 at Hub | Buy the next damage, fire-rate, or dash tier |
| R | Restart mutable match state on the same generated map, or restart regression combat |
| N | Generate and enter a new seeded match |
| F1 | Toggle generated horde match / combat regression arena |
| F2 | Toggle the full-level developer overview |
| Left / Right in overview | Browse fixed representative configurations read-only |
| Home in overview | Return to the active generated layout |
| F3 | Toggle rendering/gameplay diagnostics |
| Escape/window close | Exit |

`LevelSession` owns the authoritative generated player, doorway collision, and traversal state.
`HordeMatch` owns the persistent weapon and projectile state.
It also owns points, gates, tiered upgrades, the automatic endless director, and bounded recipe-aware difficulty.
It owns the map-wide enemy collection, Anchor/Hub/relay/Exit objective state, and terminal progression.
Gate purchases update collision and player and enemy navigation through the same published thresholds.
Drifters and Runners pursue through open cells.
Casters and periodic Elites use ranged fan patterns.
The optional relay sequence grants the next persistent fire-rate tier.

## Generator demo controls

| Input | Action |
|---|---|
| `R` | Generate the next random grid in the diagnostic demo (unrelated to gameplay restart) |
| `G` | Generate a new room layout |
| `M` | Cycle shooter, branching-shape, and organic-growth generation |
| Left / Right | Change grid seed |
| Up / Down | Change hex radius |
| `P` | Toggle quad-center markers |
| `F` | Fit grid to the window |
| Mouse wheel | Zoom around cursor |
| Middle/right drag | Pan |

Each solid-line junction is one logical floor cell.
The default **shooter layout** selects Start and Exit sites near separated entrances.
It usually distributes more arena seeds with farthest-point sampling.
It can put two or three substantial rooms in one local cluster.
It can also make one larger landmark arena.
It plans a spatial mission graph between these arenas.
Pathfinding uses clearance and portal width to route each planned connection.
Long routes become separate corridor regions.
The generator widens these routes where geometry permits.
Short routes become direct arena doorways.
This prevents small connector rooms.
The generator uses separate connector identities only for long passages.
Production maps use two required useful cycles.
Typed Shortcut edges must save at least two arena transitions.
Only planned room contacts become doorways.
Incidental contacts do not make shortcuts.

The legacy branching method builds geometric room masks.
Organic generation grows and partitions a connected irregular footprint.
Each method keeps exterior negative space.
Each method connects the selected three-to-six boundary entrances.
Each method combines the room seed with a fingerprint of neutral topology and physical metrics.
Layout metadata identifies Start, Exit, Hub, Connector, Reward, and Combat rooms.
It also identifies cover and enemy-spawn candidate cells.
These cells stay away from published internal doorway thresholds.
Games must also filter cells near connected exterior entrances.
Region boundaries stay continuous in the 2D diagnostic renderer.
Hovering highlights the complete room under the pointer.

## Linux: missing `DISPLAY`

raylib needs a graphical desktop.
If startup reports that `DISPLAY` is missing, run the application in a terminal in your desktop session.
Do not run it from a text console or a normal SSH session.

For a remote machine, use one of these options:

- Use X11 forwarding with `ssh -X user@host`.
- Run the application after the connection starts.
- Make sure that the local machine has an X server.
- Use a VNC/RDP desktop session and start the application from a terminal in that session.
- For a non-visible CI smoke test, use `xvfb-run -a ./build/stalberg_game` or `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display.
It verifies that an application starts.
It does not show an interactive window.
The development workstation can start the game with `DISPLAY=:0 ./build/stalberg_game`.
