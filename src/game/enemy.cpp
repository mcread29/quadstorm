#include "enemy.hpp"

#include "collision_2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr float DIRECTION_EPSILON = 0.0001F;
constexpr float ENEMY_DISTANCE_CORRECTION = 0.72F;
constexpr float ENEMY_SPREAD_ANGLE = 14.0F * DEG2RAD;
constexpr float ENEMY_MUZZLE_DISTANCE = ENEMY_RADIUS + 0.32F;

float length(Vector2 vector)
{
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

Vector2 normalized(Vector2 vector)
{
    const float magnitude = length(vector);
    if (magnitude <= DIRECTION_EPSILON) {
        return Vector2 {};
    }
    return Vector2 { vector.x / magnitude, vector.y / magnitude };
}

Vector2 rotated(Vector2 vector, float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return Vector2 {
        vector.x * cosine - vector.y * sine,
        vector.x * sine + vector.y * cosine
    };
}

} // namespace

void updateEnemyMovement(Enemy& enemy, Vector2 playerPosition, float stepTime)
{
    enemy.previousPosition = enemy.position;
    if (!isEnemyAlive(enemy)) {
        enemy.velocity = Vector2 {};
        return;
    }

    const Vector2 toPlayer {
        playerPosition.x - enemy.position.x,
        playerPosition.y - enemy.position.y
    };
    const float playerDistance = length(toPlayer);
    if (playerDistance <= DIRECTION_EPSILON) {
        enemy.velocity = Vector2 {};
        return;
    }

    const Vector2 radial {
        toPlayer.x / playerDistance,
        toPlayer.y / playerDistance
    };
    enemy.facing = radial;
    const Vector2 tangent { -radial.y, radial.x };
    const float distanceError = std::clamp(
        (playerDistance - ENEMY_PREFERRED_DISTANCE)
            * ENEMY_DISTANCE_CORRECTION,
        -1.0F, 1.0F);
    const Vector2 desiredDirection = normalized(Vector2 {
        tangent.x + radial.x * distanceError,
        tangent.y + radial.y * distanceError
    });
    enemy.velocity = Vector2 {
        desiredDirection.x * ENEMY_MOVE_SPEED,
        desiredDirection.y * ENEMY_MOVE_SPEED
    };
    enemy.position.x += enemy.velocity.x * stepTime;
    enemy.position.y += enemy.velocity.y * stepTime;
}

bool updateEnemyPattern(Enemy& enemy, ProjectilePool& projectiles,
    Vector2 playerPosition, float stepTime)
{
    if (!isEnemyAlive(enemy)) {
        return false;
    }

    enemy.shotCooldownRemaining = std::max(
        0.0F, enemy.shotCooldownRemaining - stepTime);
    if (enemy.shotCooldownRemaining > 0.0F) {
        return false;
    }

    const Vector2 aimDirection = normalized(Vector2 {
        playerPosition.x - enemy.position.x,
        playerPosition.y - enemy.position.y
    });
    if (length(aimDirection) <= DIRECTION_EPSILON) {
        enemy.shotCooldownRemaining = ENEMY_SHOT_INTERVAL;
        return false;
    }

    enemy.facing = aimDirection;
    const Vector2 muzzle {
        enemy.position.x + aimDirection.x * ENEMY_MUZZLE_DISTANCE,
        enemy.position.y + aimDirection.y * ENEMY_MUZZLE_DISTANCE
    };
    bool spawnedAny = false;
    for (const float angle : std::array {
             -ENEMY_SPREAD_ANGLE, 0.0F, ENEMY_SPREAD_ANGLE }) {
        spawnedAny |= projectiles.spawn(muzzle, rotated(aimDirection, angle));
    }
    enemy.shotCooldownRemaining = ENEMY_SHOT_INTERVAL;
    return spawnedAny;
}

EnemyDamageResult updateEnemyDamage(Enemy& enemy,
    ProjectilePool& playerProjectiles, float stepTime)
{
    enemy.hitFlashRemaining = std::max(
        0.0F, enemy.hitFlashRemaining - stepTime);
    if (!isEnemyAlive(enemy)) {
        return EnemyDamageResult::none;
    }

    EnemyDamageResult result = EnemyDamageResult::none;
    for (Projectile& projectile : playerProjectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }

        const Vector2 relativeStart {
            projectile.previousPosition.x - enemy.previousPosition.x,
            projectile.previousPosition.y - enemy.previousPosition.y
        };
        const Vector2 relativeEnd {
            projectile.position.x - enemy.position.x,
            projectile.position.y - enemy.position.y
        };
        const auto hitAmount = sweepCircleAgainstCircle(relativeStart,
            relativeEnd, playerProjectiles.profile().radius,
            Vector2 {}, ENEMY_RADIUS);
        if (!hitAmount.has_value()) {
            continue;
        }

        projectile.active = false;
        --enemy.health;
        enemy.hitFlashRemaining = ENEMY_HIT_FLASH_DURATION;
        result = EnemyDamageResult::hit;
        if (enemy.health <= 0) {
            enemy.health = 0;
            enemy.velocity = Vector2 {};
            result = EnemyDamageResult::died;
            break;
        }
    }
    return result;
}

bool isEnemyAlive(const Enemy& enemy)
{
    return enemy.health > 0;
}

Vector2 interpolateEnemyPosition(
    const Enemy& enemy, float interpolationAmount)
{
    return Vector2 {
        enemy.previousPosition.x
            + (enemy.position.x - enemy.previousPosition.x)
                * interpolationAmount,
        enemy.previousPosition.y
            + (enemy.position.y - enemy.previousPosition.y)
                * interpolationAmount
    };
}
