#include "player.hpp"

#include <cmath>

namespace {

constexpr float PLAYER_SPEED = 8.0F;
constexpr float PLAYER_ACCELERATION = 38.0F;
constexpr float PLAYER_DECELERATION = 46.0F;

float length(Vector2 vector)
{
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

Vector2 normalized(Vector2 vector)
{
    const float magnitude = length(vector);
    if (magnitude <= 0.0001F) {
        return Vector2 {};
    }
    return Vector2 { vector.x / magnitude, vector.y / magnitude };
}

Vector2 moveTowards(Vector2 current, Vector2 target, float maximumChange)
{
    const Vector2 difference { target.x - current.x, target.y - current.y };
    const float distance = length(difference);
    if (distance <= maximumChange || distance <= 0.0001F) {
        return target;
    }

    const float scale = maximumChange / distance;
    return Vector2 {
        current.x + difference.x * scale,
        current.y + difference.y * scale
    };
}

float lerp(float start, float end, float amount)
{
    return start + (end - start) * amount;
}

} // namespace

void updatePlayer(Player& player, const PlayerInput& input, float stepTime)
{
    const Vector2 targetVelocity {
        input.movement.x * PLAYER_SPEED,
        input.movement.y * PLAYER_SPEED
    };
    const float acceleration = length(input.movement) > 0.0F
        ? PLAYER_ACCELERATION
        : PLAYER_DECELERATION;
    player.velocity = moveTowards(
        player.velocity, targetVelocity, acceleration * stepTime);

    player.position.x += player.velocity.x * stepTime;
    player.position.z += player.velocity.y * stepTime;

    if (!input.hasAimPoint) {
        return;
    }

    const Vector2 aimDirection {
        input.aimPoint.x - player.position.x,
        input.aimPoint.z - player.position.z
    };
    if (length(aimDirection) > 0.05F) {
        player.facing = normalized(aimDirection);
    }
}

Player interpolatePlayer(const Player& previous, const Player& current, float amount)
{
    Player result = current;
    result.position.x = lerp(previous.position.x, current.position.x, amount);
    result.position.y = lerp(previous.position.y, current.position.y, amount);
    result.position.z = lerp(previous.position.z, current.position.z, amount);
    result.velocity.x = lerp(previous.velocity.x, current.velocity.x, amount);
    result.velocity.y = lerp(previous.velocity.y, current.velocity.y, amount);

    const Vector2 facing {
        lerp(previous.facing.x, current.facing.x, amount),
        lerp(previous.facing.y, current.facing.y, amount)
    };
    const Vector2 interpolatedFacing = normalized(facing);
    if (length(interpolatedFacing) > 0.0F) {
        result.facing = interpolatedFacing;
    }
    return result;
}
