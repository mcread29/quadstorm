#include "target.hpp"

#include <algorithm>

namespace {

constexpr float MINIMUM_SEGMENT_LENGTH_SQUARED = 0.000001F;

float lengthSquared(Vector2 vector)
{
    return vector.x * vector.x + vector.y * vector.y;
}

bool segmentIntersectsCircle(
    Vector2 start, Vector2 end, Vector2 center, float radius)
{
    const Vector2 segment { end.x - start.x, end.y - start.y };
    const float segmentLengthSquared = lengthSquared(segment);
    float segmentAmount = 0.0F;
    if (segmentLengthSquared > MINIMUM_SEGMENT_LENGTH_SQUARED) {
        const Vector2 startToCenter {
            center.x - start.x,
            center.y - start.y
        };
        segmentAmount = std::clamp(
            (startToCenter.x * segment.x + startToCenter.y * segment.y)
                / segmentLengthSquared,
            0.0F, 1.0F);
    }

    const Vector2 closestPoint {
        start.x + segment.x * segmentAmount,
        start.y + segment.y * segmentAmount
    };
    const Vector2 closestToCenter {
        center.x - closestPoint.x,
        center.y - closestPoint.y
    };
    return lengthSquared(closestToCenter) <= radius * radius;
}

void resetTarget(Target& target)
{
    target.health = TARGET_MAX_HEALTH;
    target.hitFlashRemaining = 0.0F;
    target.resetRemaining = 0.0F;
}

} // namespace

void updateTarget(Target& target, ProjectilePool& projectiles, float stepTime)
{
    target.hitFlashRemaining = std::max(
        0.0F, target.hitFlashRemaining - stepTime);

    if (target.health <= 0) {
        target.resetRemaining = std::max(
            0.0F, target.resetRemaining - stepTime);
        if (target.resetRemaining <= 0.0F) {
            resetTarget(target);
        }
        return;
    }

    constexpr float collisionRadius = TARGET_RADIUS + PROJECTILE_RADIUS;
    for (Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active
            || !segmentIntersectsCircle(projectile.previousPosition,
                projectile.position, target.position, collisionRadius)) {
            continue;
        }

        projectile.active = false;
        --target.health;
        target.hitFlashRemaining = TARGET_HIT_FLASH_DURATION;
        if (target.health <= 0) {
            target.health = 0;
            target.resetRemaining = TARGET_RESET_DELAY;
            break;
        }
    }
}
