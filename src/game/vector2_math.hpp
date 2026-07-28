#pragma once

#include "raylib.h"

#include <cmath>

namespace vector2 {

inline float dot(Vector2 left, Vector2 right)
{
    return left.x * right.x + left.y * right.y;
}

inline float lengthSquared(Vector2 vector)
{
    return dot(vector, vector);
}

inline float length(Vector2 vector)
{
    return std::sqrt(lengthSquared(vector));
}

inline Vector2 normalized(Vector2 vector, float epsilon = 0.0001F)
{
    const float magnitude = length(vector);
    if (magnitude <= epsilon) {
        return Vector2 {};
    }
    return Vector2 { vector.x / magnitude, vector.y / magnitude };
}

inline Vector2 subtract(Vector2 left, Vector2 right)
{
    return Vector2 { left.x - right.x, left.y - right.y };
}

inline float distanceSquared(Vector2 first, Vector2 second)
{
    return lengthSquared(subtract(first, second));
}

inline float lerp(float start, float end, float amount)
{
    return start + (end - start) * amount;
}

} // namespace vector2
