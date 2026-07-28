#include "arena.hpp"

#include <cmath>
#include <optional>

namespace {

constexpr float COLLISION_EPSILON = 0.000001F;
constexpr int PLAYER_COLLISION_PASSES = 4;

float dot(Vector2 left, Vector2 right)
{
    return left.x * right.x + left.y * right.y;
}

float lengthSquared(Vector2 vector)
{
    return dot(vector, vector);
}

Vector2 subtract(Vector2 left, Vector2 right)
{
    return Vector2 { left.x - right.x, left.y - right.y };
}

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

void removeVelocityIntoContact(Player& player, Vector2 normal)
{
    const float velocityIntoContact = dot(player.velocity, normal);
    if (velocityIntoContact >= 0.0F) {
        return;
    }
    player.velocity.x -= normal.x * velocityIntoContact;
    player.velocity.y -= normal.y * velocityIntoContact;
}

} // namespace

void resolvePlayerWallCollisions(Player& player, Vector2 previousPosition,
    std::span<const WallSegment> walls)
{
    for (int pass = 0; pass < PLAYER_COLLISION_PASSES; ++pass) {
        bool foundContact = false;
        for (const WallSegment& wall : walls) {
            const Vector2 currentPosition { player.position.x, player.position.z };
            const ClosestSegmentPoint closestPoint = closestPointOnSegment(
                currentPosition, wall);
            const Vector2 contactOffset = subtract(
                currentPosition, closestPoint.position);
            const float distanceSquared = lengthSquared(contactOffset);
            if (distanceSquared > PLAYER_RADIUS * PLAYER_RADIUS) {
                continue;
            }

            Vector2 normal {};
            const float distance = std::sqrt(distanceSquared);
            if (closestPoint.amount > COLLISION_EPSILON
                && closestPoint.amount < 1.0F - COLLISION_EPSILON) {
                normal = faceNormal(
                    wall, previousPosition, currentPosition, player.velocity);
            } else if (distance > COLLISION_EPSILON) {
                normal = Vector2 {
                    contactOffset.x / distance,
                    contactOffset.y / distance
                };
            } else {
                normal = faceNormal(
                    wall, previousPosition, currentPosition, player.velocity);
            }
            if (lengthSquared(normal) <= COLLISION_EPSILON) {
                continue;
            }

            const float penetration = PLAYER_RADIUS
                - dot(contactOffset, normal);
            if (penetration > 0.0F) {
                player.position.x += normal.x * penetration;
                player.position.z += normal.y * penetration;
            }
            removeVelocityIntoContact(player, normal);
            foundContact = true;
        }
        if (!foundContact) {
            break;
        }
    }
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
