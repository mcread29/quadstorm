#include "game_camera.hpp"
#include "game_input.hpp"
#include "player.hpp"
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
        Player player;
        Player previousPlayer = player;
        Camera3D camera = makeGameCamera(player);
        Camera3D previousCamera = camera;
        Camera3D renderCamera = camera;
        Vector3 aimPoint { -3.0F, 0.0F, -3.0F };
        float accumulatedTime = 0.0F;

        while (!WindowShouldClose()) {
            const float frameTime = std::min(GetFrameTime(), MAX_FRAME_TIME);
            accumulatedTime += frameTime;

            PlayerInput input = readPlayerInput(renderCamera);
            if (input.hasAimPoint) {
                aimPoint = input.aimPoint;
            }

            while (accumulatedTime >= FIXED_STEP_TIME) {
                previousPlayer = player;
                previousCamera = camera;
                updatePlayer(player, input, FIXED_STEP_TIME);
                updateGameCamera(camera, player, FIXED_STEP_TIME);
                accumulatedTime -= FIXED_STEP_TIME;
            }

            const float interpolationAmount = accumulatedTime / FIXED_STEP_TIME;
            const Player renderPlayer = interpolatePlayer(
                previousPlayer, player, interpolationAmount);
            renderCamera = interpolateGameCamera(
                previousCamera, camera, interpolationAmount);
            renderer.draw(renderCamera, renderPlayer, aimPoint);
        }
    }

    CloseWindow();
}
