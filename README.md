# Stålberg grid and 2.5D game prototype (raylib)

A compact C++ implementation of the grid-generation technique associated with Oskar Stålberg's *Townscaper*:

The grid generator:

1. Generates a triangular lattice with a hexagonal boundary.
2. Randomly pairs adjacent triangles into four-sided faces.
3. Subdivides every triangle and four-sided face using shared edge midpoints and a face center.
4. Applies iterative Laplacian relaxation while pinning the hexagonal boundary.

As a separate pass, the room generator consumes a neutral cell graph with physical cell area, clearance, traversal distance, shared-boundary width, and explicit entrance candidates. Its default shooter method plans a mission graph first, embeds several combat arenas—including occasional dense two-to-three-room clusters or one larger landmark room when the map supports them—and joins them with explicit corridor regions where space permits, side branches, a meaningful start-to-exit route, and at most one deliberate loop. Legacy branching-shape and organic-growth methods remain available. Every method generates several deterministic candidates and keeps the highest-scoring valid layout.

The next generator pass addresses level identity rather than adding more combat systems. The current spatial-tree pipeline can still read as repetitive arena → connector → arena structure, most shooter arenas share similar compact growth, and the runtime follow camera hides the complete map. [`docs/level-identity-pass.md`](docs/level-identity-pass.md) specifies a full-level overview and seed browser, explicit topology archetypes, room-shape grammar, semantic landmark anchors, and structural-diversity acceptance tests.

The subdivision step guarantees that the final mesh consists entirely of quads, including where random pairing leaves unmatched triangles.

The repository also contains the first seven runtime milestones and the completed Milestone 8A combat foundation of a top-down 2.5D round-based horde shooter. The finished game is intended to take place on one persistent, learnable generated map per match: players survive escalating waves, earn currency, open routes, power strange machinery, solve a readable main quest, uncover optional Easter eggs, acquire geometry-driven wonder weapons, and reach a boss or extraction. Most enemies create crowd pressure, while ranged enemies, elites, objectives, and bosses introduce readable bullet-hell patterns. The current `stalberg_game` executable provides exact generated-floor traversal, always-available player firing, a short cooldown-based dash, and generated-room encounters containing up to three deterministic stable-ID enemies; the focused single-enemy regression arena remains available through **F1**.

## Documentation

- [`docs/game-roadmap.md`](docs/game-roadmap.md) — complete horde-shooter concept, match structure, and milestones.
- [`docs/game-handoff.md`](docs/game-handoff.md) — current runtime architecture, decisions, limitations, and immediate implementation slice.
- [`docs/level-identity-pass.md`](docs/level-identity-pass.md) — next full-map overview, topology-archetype, room-grammar, landmark, and diversity-validation pass.
- [`docs/demo-and-algorithm.md`](docs/demo-and-algorithm.md) — base mesh mathematics, topology, relaxation, rendering, and source map.
- [`docs/room-generation-model.md`](docs/room-generation-model.md) — neutral physical input, output API, roles, doorways, and gameplay integration contract.
- [`docs/shooter-level-generation.md`](docs/shooter-level-generation.md) — complete graph-first arena, route, corridor, entrance, doorway, and tactical-annotation pipeline.
- [`docs/layout-quality-and-testing.md`](docs/layout-quality-and-testing.md) — deterministic candidates, validation, scoring formulas, fallback behavior, and test coverage.

## Build

A system raylib installation is used when available. Otherwise CMake downloads raylib 5.5 automatically.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/stalberg_game       # generated 2.5D traversal + combat regression
./build/stalberg_grid       # procedural-generation diagnostic demo
```

### Web game build

Install and activate the Emscripten SDK, then configure through its CMake wrapper:

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

Open `http://localhost:8000/stalberg_game.html`. The web target keeps a fixed 16:10 framebuffer and scales it uniformly with CSS so letterboxing, rendering, and mouse coordinates remain aligned across browser sizes. It uses an Emscripten browser main loop and WebGL-compatible shaders; the native grid diagnostic and native test executables are intentionally excluded from the web configuration. Combat audio is currently disabled on web because raylib 5.5's
ScriptProcessor backend cannot be initialized safely before a browser user
gesture; native builds retain audio.

The runtime starts in generated traversal mode. `GeneratedLevel` retains the relaxed source grid, exact dual geometry, neutral room graph, shooter layout, and exact doorway threshold segments together. Assigned dual polygons become a cached floor mesh; floor/void edges and unauthorized cross-room contacts become walls; only exact published doorway cell pairs remain open. Mutable player, room-lifecycle, lock, and active-wall state lives separately in `LevelSession`. The player spawns at a high-clearance cell in Start and can move through the matching door-aware navigation graph without leaving the floor.

Use **WASD** to move, **Space** to dash, the **mouse** to aim, and hold the **left mouse button** to fire anywhere on the generated map. Press **F1** for the intentionally small combat regression arena, where the same player controls exercise a stationary target, a 20-health enemy, pooled projectiles, swept relative-motion collision, player health, invulnerability, defeat, victory, and deterministic restart. A 45-degree tilted orthographic camera follows the active player, simulation runs at a fixed 120 Hz, and rendering interpolates simulation state.

Debug builds apply debugger-friendly optimization to the game runtime and bundled raylib so interactive frame pacing remains representative while symbols and assertions stay enabled. Configure with `-DSTALBERG_OPTIMIZE_DEBUG_RUNTIME=OFF` when fully unoptimized stepping is required.

Grid generation and room generation are independent libraries. The room library has no dependency on `StalbergGrid`; `src/integration/room_grid_adapter.cpp` is the translation layer between the generated mesh and the room module's owned `RoomGrid` snapshot. Grid-specific policy, including dual-cell measurement and selecting centers from the six-sided boundary as entrance candidates, stays in the grid and adapter layers. The demo completes relaxation before creating that snapshot so visual geometry, room scoring, and physical metrics agree. Generation, generated-level runtime/session behavior, and combat simulation have dedicated headless test executables. Runtime tests include dash cooldown and wall collision, traversal firing and in-flight-shot preservation, deterministic multi-spawn identities and order, earliest enemy-hit selection, all-enemies-clear doorway transitions, hostile-shot cleanup, closed-wall containment, reset behavior, and custom encounter wall injection.

## Game prototype controls

| Input | Action |
|---|---|
| WASD | Move relative to the camera |
| Space | Dash in the movement direction, or facing direction while stationary |
| Mouse | Aim on the ground plane |
| Hold left mouse button | Fire on the generated map or in the combat regression arena |
| R | Reset to Start, or restart after combat defeat/victory |
| F1 | Toggle generated traversal / combat regression arena |
| F3 | Toggle rendering/gameplay diagnostics |
| Escape/window close | Exit |

Generated traversal and generated-room encounters share the reusable game core. `LevelSession` owns the authoritative player and room lifecycle, while `GeneratedEncounterCoordinator` keeps the generated weapon and player projectiles active during traversal and preserves them across encounter activation and clearing. Its stable enemy collection updates up to three enemies in identity order and keeps room thresholds locked until all are defeated. Atomic encounter transitions keep room state, doorway locks, and active collision walls synchronized through combat, clearing, and progression toward Exit. The intended horde mode will reinterpret the same exact doorway thresholds as persistent gates, the room roles as map landmarks, and the navigation graph as the shared movement contract for crowds crossing every opened part of the map.

## Generator demo controls

| Input | Action |
|---|---|
| `R` | Generate the next random grid |
| `G` | Generate a new room layout |
| `M` | Cycle shooter, branching-shape, and organic-growth generation |
| Left / Right | Change grid seed |
| Up / Down | Change hex radius |
| `P` | Toggle quad-center markers |
| `F` | Fit grid to the window |
| Mouse wheel | Zoom around cursor |
| Middle/right drag | Pan |

Each solid-line junction is one logical floor cell. The default **shooter layout** chooses start and exit anchors near well-separated selected entrances, usually distributes additional arena seeds with farthest-point sampling, occasionally concentrates two or three substantial rooms into a local cluster or gives one site a larger landmark arena, and plans a spatial mission graph between them. Every planned arena connection is routed with clearance- and portal-width-aware pathfinding. Longer routes become independently identified corridor regions and are widened laterally where geometry permits. Routes shorter than an arena's approximate diameter are folded into an endpoint arena and become direct arena doorways instead of tiny connector rooms; separate connector identities are reserved for long passages. A tree supplies the main route and side branches; larger maps may receive one intentional loop. Only planned room contacts become doorways, so incidental touching does not create unwanted shortcuts.

The legacy branching method builds geometric room masks, while organic generation grows and partitions a connected noisy footprint. Every method preserves exterior negative space, connects the selected three-to-six boundary entrances, and combines the room seed with a fingerprint of the neutral topology and physical metrics. Layout metadata identifies start, exit, hub, connector, reward, and combat rooms, plus cover and enemy-spawn candidate cells kept away from published internal doorway thresholds. Games should separately filter around connected exterior entrances. Region boundaries remain continuous in this 2D diagnostic renderer, and hovering highlights the complete room under the pointer.

## Linux: missing `DISPLAY`

raylib needs access to a graphical desktop. If startup reports that `DISPLAY` is missing, run the application from a terminal inside your desktop session rather than a text console or ordinary SSH session.

For a remote machine, use one of these options:

- Connect with X11 forwarding: `ssh -X user@host`, then run the application. The local machine must have an X server.
- Use a VNC/RDP desktop session and launch it from a terminal there.
- For a non-visible CI smoke test only: `xvfb-run -a ./build/stalberg_game` or `xvfb-run -a ./build/stalberg_grid`.

`xvfb-run` supplies a virtual display, so it verifies that an application starts but does not show an interactive window. The development workstation used for the prototype can launch interactively with `DISPLAY=:0 ./build/stalberg_game`.
