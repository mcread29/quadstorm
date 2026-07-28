#include "arena.hpp"

#include "vector2_math.hpp"

#include <cmath>
#include <optional>

namespace {

constexpr float COLLISION_EPSILON = 0.000001F;
constexpr int CIRCLE_COLLISION_PASSES = 4;

using vector2::dot;
using vector2::lengthSquared;
using vector2::subtract;

Vector2 faceNormal(const WallSegment& wall, Vector2 previousPosition,
    Vector2 currentPosition, Vector2 velocity)
{
    const Vector2 segment = subtract(wall.end, wall.start);
    const float segmentLength = std::sqrt(lengthSquared(segment));
    if (segmentLength <= COLLISION_EPSILON) {
        return Vector2 {};
    }

    Vector2 normal { -segment.y / segmentLength, segment.x / segmentLength };
    float side = dot(subtract(previousPosition, wall.start), normal);
    if (std::abs(side) <= COLLISION_EPSILON) {
        side = dot(subtract(currentPosition, wall.start), normal);
    }
    if (std::abs(side) <= COLLISION_EPSILON) {
        side = -dot(velocity, normal);
    }
    if (side < 0.0F) {
        normal.x = -normal.x;
        normal.y = -normal.y;
    }
    return normal;
}

void removeVelocityIntoContact(Vector2& velocity, Vector2 normal)
{
    const float velocityIntoContact = dot(velocity, normal);
    if (velocityIntoContact >= 0.0F) {
        return;
    }
    velocity.x -= normal.x * velocityIntoContact;
    velocity.y -= normal.y * velocityIntoContact;
}

} // namespace

void resolveCircleWallCollisions(Vector2& position, Vector2& velocity,
    float radius, Vector2 previousPosition,
    std::span<const WallSegment> walls)
{
    if (!std::isfinite(radius) || radius < 0.0F) {
        return;
    }

    for (int pass = 0; pass < CIRCLE_COLLISION_PASSES; ++pass) {
        bool foundContact = false;
        for (const WallSegment& wall : walls) {
            const ClosestSegmentPoint closestPoint = closestPointOnSegment(
                position, wall);
            const Vector2 contactOffset = subtract(
                position, closestPoint.position);
            const float distanceSquared = lengthSquared(contactOffset);
            if (distanceSquared > radius * radius) {
                continue;
            }

            Vector2 normal {};
            const float distance = std::sqrt(distanceSquared);
            if (closestPoint.amount > COLLISION_EPSILON
                && closestPoint.amount < 1.0F - COLLISION_EPSILON) {
                normal = faceNormal(
                    wall, previousPosition, position, velocity);
            } else if (distance > COLLISION_EPSILON) {
                normal = Vector2 {
                    contactOffset.x / distance,
                    contactOffset.y / distance
                };
            } else {
                normal = faceNormal(
                    wall, previousPosition, position, velocity);
            }
            if (lengthSquared(normal) <= COLLISION_EPSILON) {
                continue;
            }

            const float penetration = radius - dot(contactOffset, normal);
            if (penetration > 0.0F) {
                position.x += normal.x * penetration;
                position.y += normal.y * penetration;
            }
            removeVelocityIntoContact(velocity, normal);
            foundContact = true;
        }
        if (!foundContact) {
            break;
        }
    }
}

void resolvePlayerWallCollisions(Player& player, Vector2 previousPosition,
    std::span<const WallSegment> walls)
{
    Vector2 position { player.position.x, player.position.z };
    resolveCircleWallCollisions(position, player.velocity, PLAYER_RADIUS,
        previousPosition, walls);
    player.position.x = position.x;
    player.position.z = position.y;
}

void resolveProjectileWallCollisions(ProjectilePool& projectiles,
    std::span<const WallSegment> walls)
{
    for (Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }

        if (pointIsOutsideClosedConvexLoop(
                projectile.previousPosition, walls)) {
            projectile.active = false;
            continue;
        }

        const std::optional<float> hitAmount = earliestCircleSegmentHit(
            projectile.previousPosition, projectile.position,
            projectiles.profile().radius, walls);
        if (!hitAmount.has_value()) {
            continue;
        }

        const float amount = *hitAmount;
        projectile.position = Vector2 {
            projectile.previousPosition.x
                + (projectile.position.x - projectile.previousPosition.x)
                    * amount,
            projectile.previousPosition.y
                + (projectile.position.y - projectile.previousPosition.y)
                    * amount
        };
        projectile.active = false;
    }
}
