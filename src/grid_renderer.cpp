#include "grid_renderer.hpp"

#include "stalberg_grid.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <vector>

namespace stalberg {
namespace {

constexpr Color GRID_COLOR { 28, 83, 96, 225 };
constexpr Color CENTER_COLOR { 25, 91, 105, 175 };
constexpr Color GENERATED_FILL { 115, 205, 205, 105 };
constexpr Color GENERATED_OUTLINE { 175, 231, 226, 220 };
constexpr Color GHOST_FILL { 225, 244, 239, 70 };
constexpr Color GHOST_OUTLINE { 225, 244, 239, 205 };
constexpr double OUTLINE_KEY_SCALE = 1000.0;

struct EdgeHash {
    std::size_t operator()(const Edge& edge) const
    {
        return std::hash<std::size_t> {}(edge.a)
            ^ (std::hash<std::size_t> {}(edge.b) << 1U);
    }
};

struct DualMesh {
    std::vector<Point> quadCenters;
    std::vector<std::vector<Vector2>> cells;
};

struct PointKey {
    long long x;
    long long y;

    bool operator==(const PointKey&) const = default;
    bool operator<(const PointKey& other) const
    {
        return x < other.x || (x == other.x && y < other.y);
    }
};

struct OutlineEdge {
    PointKey a;
    PointKey b;

    OutlineEdge(PointKey first, PointKey second)
        : a(std::min(first, second)), b(std::max(first, second))
    {
    }

    bool operator==(const OutlineEdge&) const = default;
};

struct PointKeyHash {
    std::size_t operator()(PointKey point) const
    {
        return std::hash<long long> {}(point.x)
            ^ (std::hash<long long> {}(point.y) << 1U);
    }
};

struct OutlineEdgeHash {
    std::size_t operator()(const OutlineEdge& edge) const
    {
        const PointKeyHash hashPoint;
        return hashPoint(edge.a) ^ (hashPoint(edge.b) << 1U);
    }
};

struct OutlineSegment {
    Vector2 first;
    Vector2 second;
    std::size_t owner;
    std::size_t useCount = 1;
};

struct RoundedSegment {
    PointKey firstKey;
    PointKey secondKey;
    Vector2 first;
    Vector2 second;
    Vector2 firstTrimmed;
    Vector2 secondTrimmed;
    Color color;
    bool ghost;
};

Vector2 toVector2(Point point)
{
    return Vector2 { point.x, point.y };
}

Point quadCenter(const StalbergGrid& grid, const Quad& quad)
{
    Point center { 0.0F, 0.0F };
    const auto vertices = grid.getVertices();
    for (const VertexIndex index : quad) {
        center.x += vertices[index].position.x;
        center.y += vertices[index].position.y;
    }
    center.x *= 0.25F;
    center.y *= 0.25F;
    return center;
}

Vector2 reflectAcrossEdge(Vector2 point, Vector2 first, Vector2 second)
{
    const Vector2 edge { second.x - first.x, second.y - first.y };
    const float lengthSquared = edge.x * edge.x + edge.y * edge.y;
    if (lengthSquared <= 0.0001F) {
        return point;
    }

    const float projectionAmount = ((point.x - first.x) * edge.x
        + (point.y - first.y) * edge.y) / lengthSquared;
    const Vector2 projection {
        first.x + edge.x * projectionAmount,
        first.y + edge.y * projectionAmount
    };
    return Vector2 {
        2.0F * projection.x - point.x,
        2.0F * projection.y - point.y
    };
}

DualMesh buildDualMesh(const StalbergGrid& grid)
{
    const auto vertices = grid.getVertices();
    const auto quads = grid.getQuads();
    DualMesh dual;
    dual.quadCenters.reserve(quads.size());
    dual.cells.resize(vertices.size());

    std::vector<std::size_t> incidentQuadCounts(vertices.size(), 0);
    std::vector<std::size_t> boundaryEdgeCounts(vertices.size(), 0);
    std::unordered_map<Edge, std::vector<std::size_t>, EdgeHash> edgeQuads;

    for (std::size_t quadIndex = 0; quadIndex < quads.size(); ++quadIndex) {
        const Quad& quad = quads[quadIndex];
        const Point center = quadCenter(grid, quad);
        dual.quadCenters.push_back(center);

        for (const VertexIndex vertex : quad) {
            dual.cells[vertex].push_back(toVector2(center));
            ++incidentQuadCounts[vertex];
        }
        for (std::size_t edge = 0; edge < quad.size(); ++edge) {
            edgeQuads[Edge(quad[edge], quad[(edge + 1) % quad.size()])]
                .push_back(quadIndex);
        }
    }

    for (const auto& [edge, incidentQuads] : edgeQuads) {
        if (incidentQuads.size() != 1) {
            continue;
        }

        const Vector2 first = toVector2(vertices[edge.a].position);
        const Vector2 second = toVector2(vertices[edge.b].position);
        const Vector2 center = toVector2(dual.quadCenters[incidentQuads.front()]);
        const Vector2 ghost = reflectAcrossEdge(center, first, second);
        dual.cells[edge.a].push_back(ghost);
        dual.cells[edge.b].push_back(ghost);
        ++boundaryEdgeCounts[edge.a];
        ++boundaryEdgeCounts[edge.b];
    }

    for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
        const Vector2 center = toVector2(vertices[vertexIndex].position);
        auto& polygon = dual.cells[vertexIndex];

        // A corner with only one real quad needs the diagonal ghost point as well as
        // the two edge reflections, otherwise the dual cell ends at the corner.
        if (incidentQuadCounts[vertexIndex] == 1 && boundaryEdgeCounts[vertexIndex] >= 2) {
            const Vector2 realCenter = polygon.front();
            polygon.push_back(Vector2 {
                2.0F * center.x - realCenter.x,
                2.0F * center.y - realCenter.y
            });
        }

        std::ranges::sort(polygon, [center](Vector2 lhs, Vector2 rhs) {
            return std::atan2(lhs.y - center.y, lhs.x - center.x)
                < std::atan2(rhs.y - center.y, rhs.x - center.x);
        });
        polygon.erase(std::unique(polygon.begin(), polygon.end(), [](Vector2 lhs, Vector2 rhs) {
            const float x = lhs.x - rhs.x;
            const float y = lhs.y - rhs.y;
            return x * x + y * y < 0.0001F;
        }), polygon.end());
    }

    return dual;
}

bool pointInPolygon(const std::vector<Vector2>& polygon, Point point)
{
    if (polygon.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, previous = polygon.size() - 1;
         i < polygon.size(); previous = i++) {
        const Vector2 a = polygon[i];
        const Vector2 b = polygon[previous];
        const bool crossesScanline = (a.y > point.y) != (b.y > point.y);
        if (!crossesScanline) {
            continue;
        }

        const float crossingX = (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
        if (point.x < crossingX) {
            inside = !inside;
        }
    }
    return inside;
}

PointKey pointKey(Vector2 point)
{
    return PointKey {
        std::llround(static_cast<double>(point.x) * OUTLINE_KEY_SCALE),
        std::llround(static_cast<double>(point.y) * OUTLINE_KEY_SCALE)
    };
}

Vector2 quadraticBezier(Vector2 start, Vector2 control, Vector2 end, float amount)
{
    const float inverse = 1.0F - amount;
    return Vector2 {
        inverse * inverse * start.x
            + 2.0F * inverse * amount * control.x
            + amount * amount * end.x,
        inverse * inverse * start.y
            + 2.0F * inverse * amount * control.y
            + amount * amount * end.y
    };
}

void drawQuadraticCurve(
    Vector2 start,
    Vector2 control,
    Vector2 end,
    float lineWidth,
    Color color)
{
    constexpr int steps = 7;
    Vector2 previous = start;
    for (int step = 1; step <= steps; ++step) {
        const float amount = static_cast<float>(step) / static_cast<float>(steps);
        const Vector2 next = quadraticBezier(start, control, end, amount);
        DrawLineEx(previous, next, lineWidth, color);
        previous = next;
    }
}

void drawCellFill(
    const StalbergGrid& grid,
    const DualMesh& dual,
    std::size_t vertexIndex,
    Color fill)
{
    if (vertexIndex >= dual.cells.size() || dual.cells[vertexIndex].size() < 3) {
        return;
    }

    const Vector2 center = toVector2(grid.getVertices()[vertexIndex].position);
    const auto& polygon = dual.cells[vertexIndex];
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        DrawTriangle(center, polygon[i], polygon[(i + 1) % polygon.size()], fill);
    }
}

void drawConnectedCells(
    const StalbergGrid& grid,
    const DualMesh& dual,
    const std::vector<bool>& generatedCells,
    std::optional<std::size_t> hoveredCell,
    float cameraZoom)
{
    const std::size_t cellCount = dual.cells.size();
    const auto isGenerated = [&](std::size_t cell) {
        return cell < generatedCells.size() && generatedCells[cell];
    };
    const auto isActive = [&](std::size_t cell) {
        return isGenerated(cell) || (hoveredCell && *hoveredCell == cell);
    };

    for (std::size_t cell = 0; cell < cellCount; ++cell) {
        if (isGenerated(cell)) {
            drawCellFill(grid, dual, cell, GENERATED_FILL);
        }
    }
    if (hoveredCell && *hoveredCell < cellCount) {
        drawCellFill(grid, dual, *hoveredCell, GHOST_FILL);
    }

    std::unordered_map<OutlineEdge, OutlineSegment, OutlineEdgeHash> outlines;
    for (std::size_t cell = 0; cell < cellCount; ++cell) {
        if (!isActive(cell)) {
            continue;
        }

        const auto& polygon = dual.cells[cell];
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const Vector2 first = polygon[i];
            const Vector2 second = polygon[(i + 1) % polygon.size()];
            const OutlineEdge edge(pointKey(first), pointKey(second));
            if (const auto found = outlines.find(edge); found != outlines.end()) {
                ++found->second.useCount;
            } else {
                outlines.emplace(edge, OutlineSegment { first, second, cell });
            }
        }
    }

    const float zoom = std::max(cameraZoom, 0.01F);
    const float lineWidth = 1.7F / zoom;
    const float maximumCornerRadius = 7.0F / zoom;
    std::vector<RoundedSegment> boundary;
    std::unordered_map<PointKey, std::vector<std::size_t>, PointKeyHash> nodeEdges;

    for (const auto& [edge, segment] : outlines) {
        static_cast<void>(edge);
        if (segment.useCount != 1) {
            continue;
        }

        const bool isGhostEdge = hoveredCell && segment.owner == *hoveredCell
            && !isGenerated(segment.owner);
        const Color color = isGhostEdge ? GHOST_OUTLINE : GENERATED_OUTLINE;
        const Vector2 direction {
            segment.second.x - segment.first.x,
            segment.second.y - segment.first.y
        };
        const float length = std::sqrt(
            direction.x * direction.x + direction.y * direction.y);
        if (length <= 0.0001F) {
            continue;
        }

        const float cornerRadius = std::min(maximumCornerRadius, length * 0.28F);
        const Vector2 firstTrimmed {
            segment.first.x + direction.x / length * cornerRadius,
            segment.first.y + direction.y / length * cornerRadius
        };
        const Vector2 secondTrimmed {
            segment.second.x - direction.x / length * cornerRadius,
            segment.second.y - direction.y / length * cornerRadius
        };
        const PointKey firstKey = pointKey(segment.first);
        const PointKey secondKey = pointKey(segment.second);
        const std::size_t boundaryIndex = boundary.size();
        boundary.push_back(RoundedSegment {
            firstKey,
            secondKey,
            segment.first,
            segment.second,
            firstTrimmed,
            secondTrimmed,
            color,
            isGhostEdge
        });
        nodeEdges[firstKey].push_back(boundaryIndex);
        nodeEdges[secondKey].push_back(boundaryIndex);
    }

    for (const RoundedSegment& segment : boundary) {
        DrawLineEx(
            segment.firstTrimmed, segment.secondTrimmed, lineWidth, segment.color);
    }

    for (const auto& [node, incident] : nodeEdges) {
        const auto endpoint = [&](const RoundedSegment& segment) {
            return segment.firstKey == node ? segment.first : segment.second;
        };
        const auto trimmedEndpoint = [&](const RoundedSegment& segment) {
            return segment.firstKey == node
                ? segment.firstTrimmed
                : segment.secondTrimmed;
        };

        if (incident.size() == 2) {
            const RoundedSegment& first = boundary[incident[0]];
            const RoundedSegment& second = boundary[incident[1]];
            const Vector2 corner = endpoint(first);
            const Color color = first.ghost || second.ghost
                ? GHOST_OUTLINE
                : GENERATED_OUTLINE;
            drawQuadraticCurve(
                trimmedEndpoint(first), corner, trimmedEndpoint(second), lineWidth, color);
            continue;
        }

        // Point contacts and open boundaries cannot be paired unambiguously. Keep
        // those rare junctions connected and use a round cap instead.
        if (!incident.empty()) {
            const Vector2 junction = endpoint(boundary[incident.front()]);
            for (const std::size_t segmentIndex : incident) {
                const RoundedSegment& segment = boundary[segmentIndex];
                DrawLineEx(
                    trimmedEndpoint(segment), junction, lineWidth, segment.color);
            }
            DrawCircleV(junction, lineWidth * 0.5F, GENERATED_OUTLINE);
        }
    }
}

void drawOrientedCross(
    const StalbergGrid& grid,
    const Quad& quad,
    Point centerPoint,
    float size,
    float lineWidth,
    Color color)
{
    const auto vertices = grid.getVertices();
    const Vector2 center = toVector2(centerPoint);
    for (std::size_t i = 0; i < quad.size(); ++i) {
        const Vector2 first = toVector2(vertices[quad[i]].position);
        const Vector2 second = toVector2(vertices[quad[(i + 1) % quad.size()]].position);
        const Vector2 edgeCenter {
            (first.x + second.x) * 0.5F,
            (first.y + second.y) * 0.5F
        };
        const Vector2 direction { edgeCenter.x - center.x, edgeCenter.y - center.y };
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length <= 0.0001F) {
            continue;
        }

        DrawLineEx(center,
            Vector2 {
                center.x + direction.x / length * size,
                center.y + direction.y / length * size
            },
            lineWidth,
            color);
    }
}

} // namespace

std::optional<std::size_t> findDualCellAtPoint(const StalbergGrid& grid, Point point)
{
    const DualMesh dual = buildDualMesh(grid);
    for (std::size_t i = 0; i < dual.cells.size(); ++i) {
        if (pointInPolygon(dual.cells[i], point)) {
            return i;
        }
    }
    return std::nullopt;
}

void drawGrid(
    const StalbergGrid& grid,
    bool drawCenters,
    float cameraZoom,
    const std::vector<bool>& generatedCells,
    std::optional<std::size_t> hoveredCell)
{
    const float zoom = std::max(cameraZoom, 0.01F);
    const float lineWidth = 1.6F / zoom;
    const auto vertices = grid.getVertices();
    const auto quads = grid.getQuads();
    const DualMesh dual = buildDualMesh(grid);

    drawConnectedCells(grid, dual, generatedCells, hoveredCell, zoom);

    for (const Edge& edge : grid.getEdges()) {
        DrawLineEx(
            toVector2(vertices[edge.a].position),
            toVector2(vertices[edge.b].position),
            lineWidth,
            GRID_COLOR);
    }
    for (const Vertex& vertex : vertices) {
        DrawCircleV(toVector2(vertex.position), lineWidth * 0.5F, GRID_COLOR);
    }

    if (!drawCenters) {
        return;
    }

    const float centerSize = 4.2F / zoom;
    const float centerLineWidth = 1.1F / zoom;
    for (std::size_t i = 0; i < quads.size(); ++i) {
        drawOrientedCross(
            grid, quads[i], dual.quadCenters[i], centerSize, centerLineWidth, CENTER_COLOR);
    }
}

} // namespace stalberg
