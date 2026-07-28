#include "target.hpp"

#include "collision_2d.hpp"

#include <algorithm>

namespace {

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

    for (Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const auto hitAmount = sweepCircleAgainstCircle(
            projectile.previousPosition, projectile.position,
            projectiles.profile().radius, target.position, TARGET_RADIUS);
        if (!hitAmount.has_value()) {
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
