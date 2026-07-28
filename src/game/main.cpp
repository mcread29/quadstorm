#include "arena.hpp"
#include "combat_audio.hpp"
#include "encounter.hpp"
#include "game_camera.hpp"
#include "game_input.hpp"
#include "generated_level.hpp"
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

void placePlayerAtGeneratedSpawn(Player& player, const GeneratedLevel& level)
{
    player = Player {};
    const Vector2 spawn = level.playerSpawn();
    player.position = Vector3 { spawn.x, PLAYER_RADIUS, spawn.y };
}

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
        Player generatedPlayer;
        placePlayerAtGeneratedSpawn(generatedPlayer, level);
        Player previousGeneratedPlayer = generatedPlayer;
        Player previousCombatPlayer = encounter.player;
        bool combatArenaActive = false;

        Camera3D camera = makeGameCamera(generatedPlayer);
        Camera3D previousCamera = camera;
        Camera3D renderCamera = camera;
        Vector3 aimPoint {
            generatedPlayer.position.x - 3.0F,
            0.0F,
            generatedPlayer.position.z - 3.0F
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
                    previousGeneratedPlayer = generatedPlayer;
                    camera = makeGameCamera(generatedPlayer);
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
                        encounter, input, FIXED_STEP_TIME);
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
                    previousGeneratedPlayer = generatedPlayer;
                    if (input.restartPressed) {
                        placePlayerAtGeneratedSpawn(generatedPlayer, level);
                        previousGeneratedPlayer = generatedPlayer;
                        camera = makeGameCamera(generatedPlayer);
                        previousCamera = camera;
                        combatAudio.playRestart();
                    } else {
                        const Vector2 previousPosition {
                            generatedPlayer.position.x,
                            generatedPlayer.position.z
                        };
                        updatePlayer(generatedPlayer, input, FIXED_STEP_TIME);
                        resolvePlayerWallCollisions(generatedPlayer,
                            previousPosition, level.walls());
                        updateGameCamera(
                            camera, generatedPlayer, FIXED_STEP_TIME);
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
                    previousGeneratedPlayer, generatedPlayer,
                    interpolationAmount);
                renderer.drawGenerated(
                    renderCamera, renderPlayer, aimPoint, level);
            }
        }
    }

    CloseWindow();
}
