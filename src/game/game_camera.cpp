#include "game_camera.hpp"

#include "vector2_math.hpp"

#include <cmath>

namespace {

constexpr float CAMERA_FOLLOW_SPEED = 10.0F;
constexpr float CAMERA_HEIGHT = 17.0F;
constexpr float CAMERA_HORIZONTAL_OFFSET = 12.0F;

using vector2::lerp;
using vector2::normalized;

} // namespace

Camera3D makeGameCamera(const Player& player)
{
    Camera3D camera {};
    camera.position = Vector3 {
        player.position.x + CAMERA_HORIZONTAL_OFFSET,
        CAMERA_HEIGHT,
        player.position.z + CAMERA_HORIZONTAL_OFFSET
    };
    camera.target = Vector3 { player.position.x, 0.0F, player.position.z };
    camera.up = Vector3 { 0.0F, 1.0F, 0.0F };
    camera.fovy = 24.0F;
    camera.projection = CAMERA_ORTHOGRAPHIC;
    return camera;
}

void updateGameCamera(Camera3D& camera, const Player& player, float stepTime)
{
    const float followAmount = 1.0F - std::exp(-CAMERA_FOLLOW_SPEED * stepTime);
    camera.target.x += (player.position.x - camera.target.x) * followAmount;
    camera.target.z += (player.position.z - camera.target.z) * followAmount;

    camera.position.x = camera.target.x + CAMERA_HORIZONTAL_OFFSET;
    camera.position.y = CAMERA_HEIGHT;
    camera.position.z = camera.target.z + CAMERA_HORIZONTAL_OFFSET;
}

Camera3D interpolateGameCamera(
    const Camera3D& previous, const Camera3D& current, float amount)
{
    Camera3D result = current;
    result.position.x = lerp(previous.position.x, current.position.x, amount);
    result.position.y = lerp(previous.position.y, current.position.y, amount);
    result.position.z = lerp(previous.position.z, current.position.z, amount);
    result.target.x = lerp(previous.target.x, current.target.x, amount);
    result.target.y = lerp(previous.target.y, current.target.y, amount);
    result.target.z = lerp(previous.target.z, current.target.z, amount);
    return result;
}

Vector2 cameraRelativeMovement(const Camera3D& camera, Vector2 screenMovement)
{
    const Vector2 forward = normalized(Vector2 {
        camera.target.x - camera.position.x,
        camera.target.z - camera.position.z
    });
    const Vector2 right { -forward.y, forward.x };
    return normalized(Vector2 {
        right.x * screenMovement.x + forward.x * screenMovement.y,
        right.y * screenMovement.x + forward.y * screenMovement.y
    });
}

bool groundPointAtScreenPosition(
    const Camera3D& camera, Vector2 screenPosition, Vector3& point)
{
    const Ray ray = GetScreenToWorldRay(screenPosition, camera);
    if (std::abs(ray.direction.y) <= 0.0001F) {
        return false;
    }

    const float distance = -ray.position.y / ray.direction.y;
    if (distance < 0.0F) {
        return false;
    }

    point = Vector3 {
        ray.position.x + ray.direction.x * distance,
        0.0F,
        ray.position.z + ray.direction.z * distance
    };
    return true;
}
