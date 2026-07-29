#include "player.hpp"

#include "collision_2d.hpp"
#include "projectile_pool.hpp"
#include "vector2_math.hpp"

#include <algorithm>

namespace {

constexpr float PLAYER_SPEED = 8.0F;
constexpr float PLAYER_ACCELERATION = 38.0F;
constexpr float PLAYER_DECELERATION = 46.0F;

using vector2::length;
using vector2::normalized;

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

using vector2::lerp;

} // namespace

void updatePlayer(Player& player, const PlayerInput& input, float stepTime)
{
    if (!isPlayerAlive(player)) {
        player.velocity = Vector2 {};
        player.dashRemaining = 0.0F;
        return;
    }

    if (input.hasAimPoint) {
        const Vector2 aimDirection {
            input.aimPoint.x - player.position.x,
            input.aimPoint.z - player.position.z
        };
        if (length(aimDirection) > 0.05F) {
            player.facing = normalized(aimDirection);
        }
    }

    player.dashCooldownRemaining = std::max(
        0.0F, player.dashCooldownRemaining - stepTime);
    if (input.dashPressed && player.dashRemaining <= 0.0F
        && player.dashCooldownRemaining <= 0.0F) {
        const Vector2 movementDirection = normalized(input.movement);
        player.dashDirection = length(movementDirection) > 0.0F
            ? movementDirection
            : player.facing;
        if (length(player.dashDirection) > 0.0F) {
            player.dashRemaining = PLAYER_DASH_DURATION;
            player.dashCooldownRemaining
                = PLAYER_DASH_COOLDOWN * player.dashCooldownScale;
        }
    }

    if (player.dashRemaining > 0.0F) {
        player.velocity = Vector2 {
            player.dashDirection.x * PLAYER_DASH_SPEED,
            player.dashDirection.y * PLAYER_DASH_SPEED
        };
        player.position.x += player.velocity.x * stepTime;
        player.position.z += player.velocity.y * stepTime;
        player.dashRemaining = std::max(
            0.0F, player.dashRemaining - stepTime);
        return;
    }

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
}

void updatePlayerEffects(Player& player, float stepTime)
{
    player.invulnerabilityRemaining = std::max(
        0.0F, player.invulnerabilityRemaining - stepTime);
    player.hitFlashRemaining = std::max(
        0.0F, player.hitFlashRemaining - stepTime);
}

PlayerDamageResult updatePlayerDamage(Player& player,
    ProjectilePool& enemyProjectiles, Vector2 previousPlayerPosition,
    float stepTime)
{
    return updatePlayerDamage(player, enemyProjectiles,
        previousPlayerPosition, stepTime, 1);
}

PlayerDamageResult updatePlayerDamage(Player& player,
    ProjectilePool& enemyProjectiles, Vector2 previousPlayerPosition,
    float stepTime, int damage)
{
    static_cast<void>(stepTime);
    damage = std::max(1, damage);
    if (!isPlayerAlive(player)) {
        return PlayerDamageResult::none;
    }

    PlayerDamageResult result = PlayerDamageResult::none;
    const Vector2 playerPosition { player.position.x, player.position.z };
    for (Projectile& projectile : enemyProjectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const Vector2 relativeStart {
            projectile.previousPosition.x - previousPlayerPosition.x,
            projectile.previousPosition.y - previousPlayerPosition.y
        };
        const Vector2 relativeEnd {
            projectile.position.x - playerPosition.x,
            projectile.position.y - playerPosition.y
        };
        const auto hitAmount = sweepCircleAgainstCircle(
            relativeStart, relativeEnd, enemyProjectiles.profile().radius,
            Vector2 {}, PLAYER_RADIUS);
        if (!hitAmount.has_value()) {
            continue;
        }

        projectile.active = false;
        if (player.invulnerabilityRemaining > 0.0F) {
            continue;
        }

        player.health -= damage;
        player.hitFlashRemaining = PLAYER_HIT_FLASH_DURATION;
        player.invulnerabilityRemaining = PLAYER_INVULNERABILITY_DURATION;
        result = PlayerDamageResult::hit;
        if (player.health <= 0) {
            player.health = 0;
            player.velocity = Vector2 {};
            player.dashRemaining = 0.0F;
            result = PlayerDamageResult::died;
            break;
        }
    }
    return result;
}

bool isPlayerAlive(const Player& player)
{
    return player.health > 0;
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
