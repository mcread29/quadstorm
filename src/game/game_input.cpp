#include "game_input.hpp"

#include "game_camera.hpp"

PlayerInput readPlayerInput(const Camera3D& camera)
{
    const Vector2 screenMovement {
        static_cast<float>(IsKeyDown(KEY_D)) - static_cast<float>(IsKeyDown(KEY_A)),
        static_cast<float>(IsKeyDown(KEY_W)) - static_cast<float>(IsKeyDown(KEY_S))
    };

    PlayerInput input;
    input.movement = cameraRelativeMovement(camera, screenMovement);
    input.hasAimPoint = groundPointAtScreenPosition(
        camera, GetMousePosition(), input.aimPoint);
    input.fireHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.dashPressed = IsKeyPressed(KEY_SPACE);
    input.restartPressed = IsKeyPressed(KEY_R);
    input.toggleViewPressed = IsKeyPressed(KEY_F1);
    input.interactPressed = IsKeyPressed(KEY_E);
    input.startRoundPressed = IsKeyPressed(KEY_N);
    input.buyDamagePressed = IsKeyPressed(KEY_ONE);
    input.buyFireRatePressed = IsKeyPressed(KEY_TWO);
    input.buyDashPressed = IsKeyPressed(KEY_THREE);
    return input;
}
