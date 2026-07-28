#include "collision_2d.hpp"

#include "vector2_math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float COLLISION_EPSILON = 0.000001F;

using vector2::dot;
using vector2::lengthSquared;
using vector2::subtract;

bool isFinite(Vector2 vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y);
}

bool isFinite(const Segment2D& segment)
{
    return isFinite(segment.start) && isFinite(segment.end);
}

bool segmentsFormClosedLoop(std::span<const Segment2D> segments)
{
    if (segments.size() < 3) {
        return false;
    }
    for (std::size_t index = 0; index < segments.size(); ++index) {
        if (!isFinite(segments[index])) {
            return false;
        }
        const Vector2 gap = subtract(
            segments[index].end,
            segments[(index + 1) % segments.size()].start);
        if (lengthSquared(gap) > COLLISION_EPSILON) {
            return false;
        }
    }
    return true;
}

} // namespace

ClosestSegmentPoint closestPointOnSegment(
    Vector2 point, const Segment2D& segment)
{
    const Vector2 direction = subtract(segment.end, segment.start);
    const float segmentLengthSquared = lengthSquared(direction);
    float amount = 0.0F;
    if (segmentLengthSquared > COLLISION_EPSILON) {
        amount = std::clamp(
            dot(subtract(point, segment.start), direction)
                / segmentLengthSquared,
            0.0F, 1.0F);
    }
    return ClosestSegmentPoint {
        Vector2 {
            segment.start.x + direction.x * amount,
            segment.start.y + direction.y * amount
        },
        amount
    };
}

std::optional<float> sweepCircleAgainstCircle(Vector2 start, Vector2 end,
    float movingRadius, Vector2 center, float stationaryRadius)
{
    if (!isFinite(start) || !isFinite(end) || !isFinite(center)
        || !std::isfinite(movingRadius) || !std::isfinite(stationaryRadius)
        || movingRadius < 0.0F || stationaryRadius < 0.0F) {
        return std::nullopt;
    }

    const double motionX = static_cast<double>(end.x) - start.x;
    const double motionY = static_cast<double>(end.y) - start.y;
    const double centerToStartX = static_cast<double>(start.x) - center.x;
    const double centerToStartY = static_cast<double>(start.y) - center.y;
    const double combinedRadius = static_cast<double>(movingRadius)
        + stationaryRadius;
    const double radiusSquared = combinedRadius * combinedRadius;
    const double startingDistanceSquared
        = centerToStartX * centerToStartX
        + centerToStartY * centerToStartY;
    if (startingDistanceSquared <= radiusSquared) {
        return 0.0F;
    }

    const double motionLengthSquared
        = motionX * motionX + motionY * motionY;
    if (motionLengthSquared <= COLLISION_EPSILON) {
        return std::nullopt;
    }

    const double closestAmount = std::clamp(
        -(centerToStartX * motionX + centerToStartY * motionY)
            / motionLengthSquared,
        0.0, 1.0);
    const double closestX = centerToStartX + motionX * closestAmount;
    const double closestY = centerToStartY + motionY * closestAmount;
    const double closestDistanceSquared
        = closestX * closestX + closestY * closestY;
    if (closestDistanceSquared > radiusSquared) {
        return std::nullopt;
    }

    const double entryOffset = std::sqrt(std::max(
        0.0, (radiusSquared - closestDistanceSquared)
            / motionLengthSquared));
    const double hitAmount = closestAmount - entryOffset;
    if (hitAmount < 0.0 || hitAmount > 1.0) {
        return std::nullopt;
    }
    return static_cast<float>(hitAmount);
}

std::optional<float> sweepCircleAgainstSegment(Vector2 start, Vector2 end,
    float radius, const Segment2D& segment)
{
    if (!isFinite(start) || !isFinite(end) || !isFinite(segment)
        || !std::isfinite(radius) || radius < 0.0F) {
        return std::nullopt;
    }

    const ClosestSegmentPoint startingContact = closestPointOnSegment(
        start, segment);
    if (lengthSquared(subtract(start, startingContact.position))
        <= radius * radius) {
        return 0.0F;
    }

    const Vector2 motion = subtract(end, start);
    const Vector2 segmentDirection = subtract(segment.end, segment.start);
    const float segmentLength = std::sqrt(lengthSquared(segmentDirection));
    float firstHit = std::numeric_limits<float>::infinity();

    if (segmentLength > COLLISION_EPSILON) {
        const Vector2 tangent {
            segmentDirection.x / segmentLength,
            segmentDirection.y / segmentLength
        };
        const Vector2 normal { -tangent.y, tangent.x };
        const float normalMotion = dot(motion, normal);
        if (std::abs(normalMotion) > COLLISION_EPSILON) {
            const float startingDistance = dot(
                subtract(start, segment.start), normal);
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
                    subtract(hitPosition, segment.start), tangent);
                if (alongSegment >= 0.0F && alongSegment <= segmentLength) {
                    firstHit = std::min(firstHit, hitAmount);
                }
            }
        }
    }

    for (const Vector2 endpoint : { segment.start, segment.end }) {
        const std::optional<float> endpointHit = sweepCircleAgainstCircle(
            start, end, radius, endpoint, 0.0F);
        if (endpointHit.has_value()) {
            firstHit = std::min(firstHit, *endpointHit);
        }
    }

    if (std::isfinite(firstHit)) {
        return firstHit;
    }
    return std::nullopt;
}

std::optional<float> earliestCircleSegmentHit(Vector2 start, Vector2 end,
    float radius, std::span<const Segment2D> segments)
{
    float firstHit = std::numeric_limits<float>::infinity();
    for (const Segment2D& segment : segments) {
        const std::optional<float> hitAmount = sweepCircleAgainstSegment(
            start, end, radius, segment);
        if (hitAmount.has_value()) {
            firstHit = std::min(firstHit, *hitAmount);
        }
    }
    if (std::isfinite(firstHit)) {
        return firstHit;
    }
    return std::nullopt;
}

bool pointIsOutsideClosedConvexLoop(
    Vector2 point, std::span<const Segment2D> segments)
{
    if (!segmentsFormClosedLoop(segments)) {
        return false;
    }

    float signedAreaTwice = 0.0F;
    for (const Segment2D& segment : segments) {
        signedAreaTwice += segment.start.x * segment.end.y
            - segment.end.x * segment.start.y;
    }
    const float winding = signedAreaTwice >= 0.0F ? 1.0F : -1.0F;
    for (const Segment2D& segment : segments) {
        const Vector2 direction = subtract(segment.end, segment.start);
        const Vector2 startToPoint = subtract(point, segment.start);
        const float side = direction.x * startToPoint.y
            - direction.y * startToPoint.x;
        if (side * winding < -COLLISION_EPSILON) {
            return true;
        }
    }
    return false;
}
