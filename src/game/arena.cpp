#include "arena.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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

Vector2 closestPointOnSegment(
    Vector2 point, const WallSegment& wall, float* segmentAmount = nullptr)
{
    const Vector2 segment = subtract(wall.end, wall.start);
    const float segmentLengthSquared = lengthSquared(segment);
    float amount = 0.0F;
    if (segmentLengthSquared > COLLISION_EPSILON) {
        amount = std::clamp(
            dot(subtract(point, wall.start), segment) / segmentLengthSquared,
            0.0F, 1.0F);
    }
    if (segmentAmount != nullptr) {
        *segmentAmount = amount;
    }
    return Vector2 {
        wall.start.x + segment.x * amount,
        wall.start.y + segment.y * amount
    };
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

bool wallsFormClosedLoop(std::span<const WallSegment> walls)
{
    if (walls.size() < 3) {
        return false;
    }
    for (std::size_t index = 0; index < walls.size(); ++index) {
        const Vector2 gap = subtract(
            walls[index].end, walls[(index + 1) % walls.size()].start);
        if (lengthSquared(gap) > COLLISION_EPSILON) {
            return false;
        }
    }
    return true;
}

bool pointIsOutsideClosedArena(
    Vector2 point, std::span<const WallSegment> walls)
{
    if (!wallsFormClosedLoop(walls)) {
        return false;
    }

    float signedAreaTwice = 0.0F;
    for (const WallSegment& wall : walls) {
        signedAreaTwice += wall.start.x * wall.end.y
            - wall.end.x * wall.start.y;
    }
    const float winding = signedAreaTwice >= 0.0F ? 1.0F : -1.0F;
    for (const WallSegment& wall : walls) {
        const Vector2 segment = subtract(wall.end, wall.start);
        const Vector2 startToPoint = subtract(point, wall.start);
        const float side = segment.x * startToPoint.y
            - segment.y * startToPoint.x;
        if (side * winding < -COLLISION_EPSILON) {
            return true;
        }
    }
    return false;
}

std::optional<float> movingPointCircleHitAmount(
    Vector2 start, Vector2 motion, Vector2 center, float radius)
{
    const Vector2 centerToStart = subtract(start, center);
    const float quadraticA = lengthSquared(motion);
    const float quadraticC = lengthSquared(centerToStart) - radius * radius;
    if (quadraticC <= 0.0F) {
        return 0.0F;
    }
    if (quadraticA <= COLLISION_EPSILON) {
        return std::nullopt;
    }

    const float quadraticB = 2.0F * dot(centerToStart, motion);
    const float discriminant = quadraticB * quadraticB
        - 4.0F * quadraticA * quadraticC;
    if (discriminant < 0.0F) {
        return std::nullopt;
    }

    const float hitAmount = (-quadraticB - std::sqrt(discriminant))
        / (2.0F * quadraticA);
    if (hitAmount < 0.0F || hitAmount > 1.0F) {
        return std::nullopt;
    }
    return hitAmount;
}

std::optional<float> movingCircleWallHitAmount(
    Vector2 start, Vector2 end, const WallSegment& wall, float radius)
{
    if (lengthSquared(subtract(start, closestPointOnSegment(start, wall)))
        <= radius * radius) {
        return 0.0F;
    }

    const Vector2 motion = subtract(end, start);
    const Vector2 segment = subtract(wall.end, wall.start);
    const float segmentLength = std::sqrt(lengthSquared(segment));
    float firstHit = std::numeric_limits<float>::infinity();

    if (segmentLength > COLLISION_EPSILON) {
        const Vector2 tangent {
            segment.x / segmentLength,
            segment.y / segmentLength
        };
        const Vector2 normal { -tangent.y, tangent.x };
        const float normalMotion = dot(motion, normal);
        if (std::abs(normalMotion) > COLLISION_EPSILON) {
            const float startingDistance = dot(subtract(start, wall.start), normal);
            for (const float side : { -radius, radius }) {
                const float hitAmount = (side - startingDistance) / normalMotion;
                if (hitAmount < 0.0F || hitAmount > 1.0F) {
                    continue;
                }
                const Vector2 hitPosition {
                    start.x + motion.x * hitAmount,
                    start.y + motion.y * hitAmount
                };
                const float alongSegment = dot(
                    subtract(hitPosition, wall.start), tangent);
                if (alongSegment >= 0.0F && alongSegment <= segmentLength) {
                    firstHit = std::min(firstHit, hitAmount);
                }
            }
        }
    }

    for (const Vector2 endpoint : { wall.start, wall.end }) {
        const std::optional<float> endpointHit = movingPointCircleHitAmount(
            start, motion, endpoint, radius);
        if (endpointHit.has_value()) {
            firstHit = std::min(firstHit, *endpointHit);
        }
    }

    if (std::isfinite(firstHit)) {
        return firstHit;
    }
    return std::nullopt;
}

} // namespace

void resolvePlayerWallCollisions(Player& player, Vector2 previousPosition,
    std::span<const WallSegment> walls)
{
    for (int pass = 0; pass < PLAYER_COLLISION_PASSES; ++pass) {
        bool foundContact = false;
        for (const WallSegment& wall : walls) {
            const Vector2 currentPosition { player.position.x, player.position.z };
            float segmentAmount = 0.0F;
            const Vector2 closestPoint = closestPointOnSegment(
                currentPosition, wall, &segmentAmount);
            const Vector2 contactOffset = subtract(currentPosition, closestPoint);
            const float distanceSquared = lengthSquared(contactOffset);
            if (distanceSquared > PLAYER_RADIUS * PLAYER_RADIUS) {
                continue;
            }

            Vector2 normal {};
            const float distance = std::sqrt(distanceSquared);
            if (segmentAmount > COLLISION_EPSILON
                && segmentAmount < 1.0F - COLLISION_EPSILON) {
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

        if (pointIsOutsideClosedArena(projectile.previousPosition, walls)) {
            projectile.active = false;
            continue;
        }

        float firstHit = std::numeric_limits<float>::infinity();
        for (const WallSegment& wall : walls) {
            const std::optional<float> hitAmount = movingCircleWallHitAmount(
                projectile.previousPosition, projectile.position,
                wall, PROJECTILE_RADIUS);
            if (hitAmount.has_value()) {
                firstHit = std::min(firstHit, *hitAmount);
            }
        }
        if (!std::isfinite(firstHit)) {
            continue;
        }

        projectile.position = Vector2 {
            projectile.previousPosition.x
                + (projectile.position.x - projectile.previousPosition.x) * firstHit,
            projectile.previousPosition.y
                + (projectile.position.y - projectile.previousPosition.y) * firstHit
        };
        projectile.active = false;
    }
}
