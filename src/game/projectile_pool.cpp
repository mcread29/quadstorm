#include "projectile_pool.hpp"

#include "vector2_math.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float MINIMUM_DIRECTION_LENGTH = 0.0001F;

using vector2::length;
using vector2::lerp;

bool isValid(ProjectileProfile profile)
{
    return std::isfinite(profile.speed)
        && std::isfinite(profile.lifetime)
        && std::isfinite(profile.radius)
        && profile.speed > 0.0F
        && profile.lifetime > 0.0F
        && profile.radius >= 0.0F;
}

} // namespace

ProjectilePool::ProjectilePool(ProjectileProfile profile)
    : projectileProfile(profile)
{
}

bool ProjectilePool::spawn(Vector2 position, Vector2 direction)
{
    const float directionLength = length(direction);
    if (!std::isfinite(position.x) || !std::isfinite(position.y)
        || !std::isfinite(directionLength)
        || directionLength <= MINIMUM_DIRECTION_LENGTH
        || !isValid(projectileProfile)) {
        return false;
    }

    for (Projectile& projectile : slots) {
        if (projectile.active) {
            continue;
        }

        projectile.position = position;
        projectile.previousPosition = position;
        projectile.velocity = Vector2 {
            direction.x / directionLength * projectileProfile.speed,
            direction.y / directionLength * projectileProfile.speed
        };
        projectile.remainingLifetime = projectileProfile.lifetime;
        projectile.active = true;
        return true;
    }
    return false;
}

void ProjectilePool::update(float stepTime)
{
    for (Projectile& projectile : slots) {
        if (!projectile.active) {
            continue;
        }

        if (projectile.remainingLifetime <= 0.0F) {
            projectile.active = false;
            continue;
        }

        projectile.previousPosition = projectile.position;
        const float integrationTime
            = std::min(stepTime, projectile.remainingLifetime);
        projectile.position.x += projectile.velocity.x * integrationTime;
        projectile.position.y += projectile.velocity.y * integrationTime;
        projectile.remainingLifetime -= integrationTime;
    }
}

void ProjectilePool::retireExpired()
{
    for (Projectile& projectile : slots) {
        if (projectile.active && projectile.remainingLifetime <= 0.0F) {
            projectile.active = false;
        }
    }
}

std::size_t ProjectilePool::activeCount() const
{
    std::size_t count = 0;
    for (const Projectile& projectile : slots) {
        count += static_cast<std::size_t>(projectile.active);
    }
    return count;
}

Vector2 interpolateProjectilePosition(const Projectile& projectile, float amount)
{
    return Vector2 {
        lerp(projectile.previousPosition.x, projectile.position.x, amount),
        lerp(projectile.previousPosition.y, projectile.position.y, amount)
    };
}
