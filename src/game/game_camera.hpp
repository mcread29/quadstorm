#pragma once

#include "player.hpp"

#include "raylib.h"

Camera3D makeGameCamera(const Player& player);
void updateGameCamera(Camera3D& camera, const Player& player, float stepTime);
Camera3D interpolateGameCamera(
    const Camera3D& previous, const Camera3D& current, float amount);
Vector2 cameraRelativeMovement(const Camera3D& camera, Vector2 screenMovement);
bool groundPointAtScreenPosition(
    const Camera3D& camera, Vector2 screenPosition, Vector3& point);
