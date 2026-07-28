#pragma once

#include "raylib.h"

#include <optional>
#include <span>

struct Segment2D {
    Vector2 start {};
    Vector2 end {};
};

struct ClosestSegmentPoint {
    Vector2 position {};
    float amount = 0.0F;
};

ClosestSegmentPoint closestPointOnSegment(
    Vector2 point, const Segment2D& segment);
std::optional<float> sweepCircleAgainstCircle(Vector2 start, Vector2 end,
    float movingRadius, Vector2 center, float stationaryRadius);
std::optional<float> sweepCircleAgainstSegment(Vector2 start, Vector2 end,
    float radius, const Segment2D& segment);
std::optional<float> earliestCircleSegmentHit(Vector2 start, Vector2 end,
    float radius, std::span<const Segment2D> segments);
bool pointIsOutsideClosedConvexLoop(
    Vector2 point, std::span<const Segment2D> segments);
