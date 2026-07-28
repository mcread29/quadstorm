#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

namespace stalberg::geometry {

constexpr double DEFAULT_KEY_SCALE = 1000.0;

struct QuantizedPoint {
    long long x = 0;
    long long y = 0;

    bool operator==(const QuantizedPoint&) const = default;
    auto operator<=>(const QuantizedPoint&) const = default;
};

struct QuantizedEdge {
    QuantizedPoint first;
    QuantizedPoint second;

    QuantizedEdge() = default;
    QuantizedEdge(QuantizedPoint a, QuantizedPoint b)
        : first(std::min(a, b)), second(std::max(a, b))
    {
    }

    bool operator==(const QuantizedEdge&) const = default;
    auto operator<=>(const QuantizedEdge&) const = default;
};

inline QuantizedPoint quantizePoint(
    float x, float y, double scale = DEFAULT_KEY_SCALE)
{
    return QuantizedPoint {
        std::llround(static_cast<double>(x) * scale),
        std::llround(static_cast<double>(y) * scale)
    };
}

struct QuantizedPointHash {
    std::size_t operator()(QuantizedPoint point) const
    {
        return std::hash<long long> {}(point.x)
            ^ (std::hash<long long> {}(point.y) << 1U);
    }
};

struct QuantizedEdgeHash {
    std::size_t operator()(const QuantizedEdge& edge) const
    {
        const QuantizedPointHash hashPoint;
        return hashPoint(edge.first) ^ (hashPoint(edge.second) << 1U);
    }
};

} // namespace stalberg::geometry
