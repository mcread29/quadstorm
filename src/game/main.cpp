#include "arena.hpp"
#include "combat_audio.hpp"
#include "encounter.hpp"
#include "game_camera.hpp"
#include "game_input.hpp"
#include "generated_level.hpp"
#include "horde_match.hpp"
#include "level_session.hpp"
#include "prototype_renderer.hpp"

#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>

namespace {

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float FIXED_STEP_TIME = 1.0F / 120.0F;
constexpr float MAX_FRAME_TIME = 0.05F;

class GameApplication {
public:
    explicit GameApplication(GeneratedLevelConfig config)
        : level(config)
        , renderer(level)
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
        if (IsKeyPressed(KEY_F2)) {
            overviewActive = !overviewActive;
            clearQueuedInputs();
            accumulatedTime = 0.0F;
        }
        if (overviewActive) {
            if (IsKeyPressed(KEY_HOME)) {
                selectOverviewConfiguration(0);
            } else if (IsKeyPressed(KEY_LEFT)) {
                const std::size_t previous = overviewConfiguration == 0
                    ? REPRESENTATIVE_LEVEL_CONFIGS.size() - 1
                    : overviewConfiguration - 1;
                selectOverviewConfiguration(previous);
            } else if (IsKeyPressed(KEY_RIGHT)) {
                selectOverviewConfiguration((overviewConfiguration + 1)
                    % REPRESENTATIVE_LEVEL_CONFIGS.size());
            }
            const GeneratedLevel& overviewLevel = selectedOverviewLevel();
            renderer.drawGeneratedOverview(overviewLevel,
                overviewConfiguration == 0 ? &levelSession : nullptr,
                overviewConfiguration == 0 ? &hordeMatch : nullptr,
                overviewConfiguration, REPRESENTATIVE_LEVEL_CONFIGS.size());
            return;
        }

        const float frameTime = std::min(GetFrameTime(), MAX_FRAME_TIME);
        accumulatedTime += frameTime;

        PlayerInput input = readPlayerInput(renderCamera);
        if (IsKeyPressed(KEY_F3)) {
            showDebug = !showDebug;
        }
        if (input.toggleViewPressed) {
            combatArenaActive = !combatArenaActive;
            clearQueuedInputs();
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
        dashQueued |= input.dashPressed;
        interactQueued |= input.interactPressed;
        startRoundQueued |= input.startRoundPressed;
        buyDamageQueued |= input.buyDamagePressed;
        buyFireRateQueued |= input.buyFireRatePressed;
        buyDashQueued |= input.buyDashPressed;
        input.restartPressed = restartQueued;
        input.dashPressed = dashQueued;
        input.interactPressed = interactQueued;
        input.startRoundPressed = startRoundQueued;
        input.buyDamagePressed = buyDamageQueued;
        input.buyFireRatePressed = buyFireRateQueued;
        input.buyDashPressed = buyDashQueued;
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
            clearQueuedInputs();
            input.restartPressed = false;
            input.dashPressed = false;
            input.interactPressed = false;
            input.startRoundPressed = false;
            input.buyDamagePressed = false;
            input.buyFireRatePressed = false;
            input.buyDashPressed = false;
            accumulatedTime -= FIXED_STEP_TIME;
        }

        const float interpolationAmount = accumulatedTime / FIXED_STEP_TIME;
        renderCamera = interpolateGameCamera(
            previousCamera, camera, interpolationAmount);
        if (combatArenaActive) {
            const Player renderPlayer = interpolatePlayer(
                previousCombatPlayer, encounter.player, interpolationAmount);
            renderer.drawCombat(renderCamera, renderPlayer, aimPoint,
                encounter.combat.playerAttack.projectiles, encounter.target,
                encounter.combat.enemy, encounter.combat.enemyProjectiles,
                interpolationAmount, showDebug);
        } else {
            const Player renderPlayer = interpolatePlayer(
                previousGeneratedPlayer, levelSession.player(),
                interpolationAmount);
            renderer.drawGenerated(renderCamera, renderPlayer, aimPoint,
                level, levelSession, hordeMatch, interpolationAmount,
                showDebug);
        }
    }

private:
    GeneratedLevel level;
    PrototypeRenderer renderer;
    CombatAudio combatAudio;
    Encounter encounter;
    LevelSession levelSession;
    HordeMatch hordeMatch { level, levelSession };
    Player previousGeneratedPlayer;
    Player previousCombatPlayer;
    bool combatArenaActive = false;
    bool showDebug = false;
    bool overviewActive = false;
    std::size_t overviewConfiguration = 0;
    std::unique_ptr<GeneratedLevel> overviewPreview;
    Camera3D camera;
    Camera3D previousCamera;
    Camera3D renderCamera;
    Vector3 aimPoint;
    float accumulatedTime = 0.0F;
    bool restartQueued = false;
    bool dashQueued = false;
    bool interactQueued = false;
    bool startRoundQueued = false;
    bool buyDamageQueued = false;
    bool buyFireRateQueued = false;
    bool buyDashQueued = false;

    void clearQueuedInputs()
    {
        restartQueued = false;
        dashQueued = false;
        interactQueued = false;
        startRoundQueued = false;
        buyDamageQueued = false;
        buyFireRateQueued = false;
        buyDashQueued = false;
    }

    void selectOverviewConfiguration(std::size_t configuration)
    {
        overviewConfiguration = configuration;
        if (overviewConfiguration == 0) {
            overviewPreview.reset();
            return;
        }
        overviewPreview = std::make_unique<GeneratedLevel>(
            REPRESENTATIVE_LEVEL_CONFIGS[overviewConfiguration]);
    }

    const GeneratedLevel& selectedOverviewLevel() const
    {
        return overviewPreview != nullptr ? *overviewPreview : level;
    }

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
        const HordeMatchStepResult result = updateHordeMatch(
            hordeMatch, levelSession, input, FIXED_STEP_TIME);
        if (result.reset) {
            previousGeneratedPlayer = levelSession.player();
            camera = makeGameCamera(levelSession.player());
            previousCamera = camera;
            combatAudio.playRestart();
        } else {
            updateGameCamera(camera, levelSession.player(), FIXED_STEP_TIME);
        }
        if (result.enemyFired) {
            combatAudio.playEnemyShot();
        }
        combatAudio.playPlayerDamage(result.playerDamage);
        combatAudio.playEnemyDamage(result.enemyDamage);
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

int main(int argumentCount, char** arguments)
{
    GeneratedLevelConfig levelConfig = REPRESENTATIVE_LEVEL_CONFIGS.front();
    for (int argument = 1; argument < argumentCount; ++argument) {
        const std::string_view value(arguments[argument]);
        if (value == "--recipe=hub") {
            levelConfig.roomSeed = 7;
        } else if (value == "--recipe=ring") {
            levelConfig.roomSeed = 2;
        } else if (value == "--recipe=wings") {
            levelConfig.roomSeed = 3;
        }
    }
    auto windowFlags = FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
#if !defined(PLATFORM_WEB)
    windowFlags |= FLAG_WINDOW_RESIZABLE;
#endif
    // Web keeps a fixed 16:10 framebuffer and lets CSS scale it uniformly.
    // Raylib's resizable web path instead adopts the browser aspect ratio,
    // which diverges from the letterboxed canvas and offsets mouse input.
    SetConfigFlags(windowFlags);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Stalberg game prototype");
#if !defined(PLATFORM_WEB)
    SetWindowMinSize(900, 600);
#endif

    std::unique_ptr<GameApplication> application;
    try {
        application = std::make_unique<GameApplication>(levelConfig);
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
