#include "combat_audio.hpp"
#include "encounter.hpp"
#include "game_camera.hpp"
#include "game_input.hpp"
#include "prototype_renderer.hpp"

#include "raylib.h"

#include <algorithm>

namespace {

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float FIXED_STEP_TIME = 1.0F / 120.0F;
constexpr float MAX_FRAME_TIME = 0.05F;

} // namespace

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Stalberg game prototype");

    {
        PrototypeRenderer renderer;
        CombatAudio combatAudio;
        Encounter encounter;
        Player previousPlayer = encounter.player;
        Camera3D camera = makeGameCamera(encounter.player);
        Camera3D previousCamera = camera;
        Camera3D renderCamera = camera;
        Vector3 aimPoint { -3.0F, 0.0F, -3.0F };
        float accumulatedTime = 0.0F;
        bool restartQueued = false;

        while (!WindowShouldClose()) {
            const float frameTime = std::min(GetFrameTime(), MAX_FRAME_TIME);
            accumulatedTime += frameTime;

            PlayerInput input = readPlayerInput(renderCamera);
            restartQueued |= input.restartPressed;
            input.restartPressed = restartQueued;
            if (input.hasAimPoint) {
                aimPoint = input.aimPoint;
            }

            while (accumulatedTime >= FIXED_STEP_TIME) {
                previousPlayer = encounter.player;
                previousCamera = camera;
                const EncounterStepResult result = updateEncounter(
                    encounter, input, FIXED_STEP_TIME);
                restartQueued = false;
                input.restartPressed = false;

                if (result.restarted) {
                    previousPlayer = encounter.player;
                    camera = makeGameCamera(encounter.player);
                    previousCamera = camera;
                    combatAudio.playRestart();
                } else {
                    updateGameCamera(
                        camera, encounter.player, FIXED_STEP_TIME);
                }
                if (result.enemyFired) {
                    combatAudio.playEnemyShot();
                }
                combatAudio.playPlayerDamage(result.playerDamage);
                combatAudio.playEnemyDamage(result.enemyDamage);
                accumulatedTime -= FIXED_STEP_TIME;
            }

            const float interpolationAmount = accumulatedTime / FIXED_STEP_TIME;
            const Player renderPlayer = interpolatePlayer(
                previousPlayer, encounter.player, interpolationAmount);
            renderCamera = interpolateGameCamera(
                previousCamera, camera, interpolationAmount);
            renderer.draw(renderCamera, renderPlayer, aimPoint,
                encounter.playerProjectiles, encounter.target,
                encounter.enemy, encounter.enemyProjectiles,
                interpolationAmount);
        }
    }

    CloseWindow();
}
