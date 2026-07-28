#include "arena.hpp"
#include "combat_audio.hpp"
#include "encounter.hpp"
#include "game_camera.hpp"
#include "game_input.hpp"
#include "generated_level.hpp"
#include "level_session.hpp"
#include "prototype_renderer.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <optional>

namespace {

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float FIXED_STEP_TIME = 1.0F / 120.0F;
constexpr float MAX_FRAME_TIME = 0.05F;

} // namespace

int main()
{
    std::optional<GeneratedLevel> generatedLevel;
    try {
        generatedLevel.emplace(GeneratedLevelConfig {});
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unable to build generated level: %s\n", error.what());
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Stalberg game prototype");

    {
        const GeneratedLevel& level = *generatedLevel;
        PrototypeRenderer renderer(level);
        CombatAudio combatAudio;
        Encounter encounter;
        LevelSession levelSession(level);
        Player previousGeneratedPlayer = levelSession.player();
        Player previousCombatPlayer = encounter.player;
        bool combatArenaActive = false;

        Camera3D camera = makeGameCamera(levelSession.player());
        Camera3D previousCamera = camera;
        Camera3D renderCamera = camera;
        Vector3 aimPoint {
            levelSession.player().position.x - 3.0F,
            0.0F,
            levelSession.player().position.z - 3.0F
        };
        float accumulatedTime = 0.0F;
        bool restartQueued = false;

        while (!WindowShouldClose()) {
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
                } else {
                    previousGeneratedPlayer = levelSession.player();
                    const LevelSessionStepResult result = updateLevelSession(
                        levelSession, level, input, FIXED_STEP_TIME);
                    if (result.reset) {
                        previousGeneratedPlayer = levelSession.player();
                        camera = makeGameCamera(levelSession.player());
                        previousCamera = camera;
                        combatAudio.playRestart();
                    } else {
                        updateGameCamera(
                            camera, levelSession.player(), FIXED_STEP_TIME);
                    }
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
                    previousCombatPlayer, encounter.player,
                    interpolationAmount);
                renderer.drawCombat(renderCamera, renderPlayer, aimPoint,
                    encounter.playerProjectiles, encounter.target,
                    encounter.enemy, encounter.enemyProjectiles,
                    interpolationAmount);
            } else {
                const Player renderPlayer = interpolatePlayer(
                    previousGeneratedPlayer, levelSession.player(),
                    interpolationAmount);
                renderer.drawGenerated(renderCamera, renderPlayer, aimPoint,
                    level, levelSession);
            }
        }
    }

    CloseWindow();
}
