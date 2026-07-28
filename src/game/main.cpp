#include "arena.hpp"
#include "combat_audio.hpp"
#include "encounter.hpp"
#include "game_camera.hpp"
#include "game_input.hpp"
#include "generated_encounter.hpp"
#include "generated_level.hpp"
#include "level_session.hpp"
#include "prototype_renderer.hpp"

#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>

namespace {

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float FIXED_STEP_TIME = 1.0F / 120.0F;
constexpr float MAX_FRAME_TIME = 0.05F;

class GameApplication {
public:
    GameApplication()
        : renderer(level)
        , levelSession(level)
        , previousGeneratedPlayer(levelSession.player())
        , previousCombatPlayer(encounter.player)
        , camera(makeGameCamera(levelSession.player()))
        , previousCamera(camera)
        , renderCamera(camera)
        , aimPoint {
            levelSession.player().position.x - 3.0F,
            0.0F,
            levelSession.player().position.z - 3.0F
        }
    {
    }

    void frame()
    {
        const float frameTime = std::min(GetFrameTime(), MAX_FRAME_TIME);
        accumulatedTime += frameTime;

        PlayerInput input = readPlayerInput(renderCamera);
        if (input.toggleViewPressed) {
            combatArenaActive = !combatArenaActive;
            restartQueued = false;
            accumulatedTime = 0.0F;
            input.hasAimPoint = false;
            if (combatArenaActive) {
                previousCombatPlayer = encounter.player;
                camera = makeGameCamera(encounter.player);
            } else {
                previousGeneratedPlayer = levelSession.player();
                camera = makeGameCamera(levelSession.player());
            }
            previousCamera = camera;
            renderCamera = camera;
        }

        restartQueued |= input.restartPressed;
        input.restartPressed = restartQueued;
        if (input.hasAimPoint) {
            aimPoint = input.aimPoint;
        }

        while (accumulatedTime >= FIXED_STEP_TIME) {
            previousCamera = camera;
            if (combatArenaActive) {
                updateCombatArena(input);
            } else {
                updateGeneratedLevel(input);
            }
            restartQueued = false;
            input.restartPressed = false;
            accumulatedTime -= FIXED_STEP_TIME;
        }

        const float interpolationAmount = accumulatedTime / FIXED_STEP_TIME;
        renderCamera = interpolateGameCamera(
            previousCamera, camera, interpolationAmount);
        if (combatArenaActive) {
            const Player renderPlayer = interpolatePlayer(
                previousCombatPlayer, encounter.player, interpolationAmount);
            renderer.drawCombat(renderCamera, renderPlayer, aimPoint,
                encounter.combat.playerProjectiles, encounter.target,
                encounter.combat.enemy, encounter.combat.enemyProjectiles,
                interpolationAmount);
        } else {
            const Player renderPlayer = interpolatePlayer(
                previousGeneratedPlayer, levelSession.player(),
                interpolationAmount);
            renderer.drawGenerated(renderCamera, renderPlayer, aimPoint,
                level, levelSession,
                generatedEncounter.combatForRoom(levelSession.currentRoom()),
                generatedEncounter.floorIsComplete(), interpolationAmount);
        }
    }

private:
    GeneratedLevel level;
    PrototypeRenderer renderer;
    CombatAudio combatAudio;
    Encounter encounter;
    LevelSession levelSession;
    GeneratedEncounterCoordinator generatedEncounter;
    Player previousGeneratedPlayer;
    Player previousCombatPlayer;
    bool combatArenaActive = false;
    Camera3D camera;
    Camera3D previousCamera;
    Camera3D renderCamera;
    Vector3 aimPoint;
    float accumulatedTime = 0.0F;
    bool restartQueued = false;

    void updateCombatArena(const PlayerInput& input)
    {
        previousCombatPlayer = encounter.player;
        const EncounterStepResult result = updateEncounter(
            encounter, input, FIXED_STEP_TIME, ARENA_WALLS);
        if (result.restarted) {
            previousCombatPlayer = encounter.player;
            camera = makeGameCamera(encounter.player);
            previousCamera = camera;
            combatAudio.playRestart();
        } else {
            updateGameCamera(camera, encounter.player, FIXED_STEP_TIME);
        }
        if (result.enemyFired) {
            combatAudio.playEnemyShot();
        }
        combatAudio.playPlayerDamage(result.playerDamage);
        combatAudio.playEnemyDamage(result.enemyDamage);
    }

    void updateGeneratedLevel(const PlayerInput& input)
    {
        previousGeneratedPlayer = levelSession.player();
        const GeneratedEncounterStepResult result
            = updateGeneratedEncounter(generatedEncounter,
                levelSession, level, input, FIXED_STEP_TIME);
        if (result.levelSession.reset) {
            previousGeneratedPlayer = levelSession.player();
            camera = makeGameCamera(levelSession.player());
            previousCamera = camera;
            combatAudio.playRestart();
        } else {
            updateGameCamera(camera, levelSession.player(), FIXED_STEP_TIME);
        }
        if (result.combat.enemyFired) {
            combatAudio.playEnemyShot();
        }
        combatAudio.playPlayerDamage(result.combat.playerDamage);
        combatAudio.playEnemyDamage(result.combat.enemyDamage);
    }
};

#if defined(PLATFORM_WEB)
void runWebFrame(void* context)
{
    auto* application = static_cast<GameApplication*>(context);
    if (WindowShouldClose()) {
        emscripten_cancel_main_loop();
        delete application;
        CloseWindow();
        return;
    }
    application->frame();
}
#endif

} // namespace

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Stalberg game prototype");

    std::unique_ptr<GameApplication> application;
    try {
        application = std::make_unique<GameApplication>();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unable to initialize game: %s\n", error.what());
        CloseWindow();
        return 1;
    }

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(
        runWebFrame, application.release(), 0, true);
#else
    while (!WindowShouldClose()) {
        application->frame();
    }
    application.reset();
    CloseWindow();
#endif
    return 0;
}
