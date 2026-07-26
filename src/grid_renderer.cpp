#include "grid_renderer.hpp"

#include "grid/dual_grid.hpp"
#include "grid/stalberg_grid.hpp"
#include "rooms/room_layout.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <vector>

namespace stalberg {
namespace {

using rooms::EMPTY_CELL;
using rooms::GeneratedRoom;
using rooms::MAX_GENERATED_ROOMS;

constexpr Color GRID_COLOR { 23, 69, 80, 125 };
constexpr Color CENTER_COLOR { 24, 74, 84, 135 };
constexpr Color ROOM_OUTLINE { 226, 240, 224, 225 };
constexpr Color HOVER_FILL { 244, 250, 236, 42 };
constexpr std::array<Color, MAX_GENERATED_ROOMS> ROOM_COLORS {
    Color { 222, 112, 94, 190 },
    Color { 232, 166, 82, 190 },
    Color { 218, 199, 91, 190 },
    Color { 129, 190, 111, 190 },
    Color { 82, 181, 151, 190 },
    Color { 76, 164, 191, 190 },
    Color { 103, 140, 211, 190 },
    Color { 145, 119, 207, 190 },
    Color { 190, 113, 188, 190 },
    Color { 210, 118, 149, 190 },
    Color { 159, 180, 103, 190 },
    Color { 106, 187, 196, 190 },
    Color { 239, 135, 72, 190 },
    Color { 183, 203, 78, 190 },
    Color { 73, 190, 183, 190 },
    Color { 129, 155, 225, 190 },
    Color { 234, 102, 121, 190 },
    Color { 196, 133, 73, 190 },
    Color { 225, 185, 117, 190 },
    Color { 154, 198, 74, 190 },
    Color { 67, 164, 108, 190 },
    Color { 75, 189, 189, 190 },
    Color { 80, 136, 186, 190 },
    Color { 122, 109, 187, 190 },
    Color { 171, 113, 202, 190 },
    Color { 218, 127, 190, 190 },
    Color { 178, 150, 116, 190 },
    Color { 111, 176, 137, 190 },
    Color { 93, 151, 166, 190 },
    Color { 151, 164, 219, 190 },
    Color { 203, 144, 166, 190 },
    Color { 197, 187, 129, 190 },
    Color { 184, 235, 127, 190 },
    Color { 103, 132, 224, 190 },
    Color { 209, 87, 79, 190 },
    Color { 117, 235, 158, 190 },
    Color { 187, 121, 224, 190 },
    Color { 209, 201, 96, 190 },
    Color { 89, 202, 235, 190 },
    Color { 224, 112, 167, 190 },
    Color { 132, 209, 113, 190 },
    Color { 120, 108, 235, 190 },
    Color { 224, 139, 85, 190 },
    Color { 105, 209, 176, 190 },
    Color { 232, 127, 235, 190 },
    Color { 192, 224, 103, 190 },
    Color { 79, 137, 209, 190 },
    Color { 235, 117, 135, 190 },
    Color { 121, 224, 136, 190 },
    Color { 145, 96, 209, 190 },
    Color { 235, 194, 89, 190 },
    Color { 112, 223, 224, 190 },
    Color { 209, 113, 180, 190 },
    Color { 159, 235, 108, 190 },
    Color { 85, 101, 224, 190 },
    Color { 209, 124, 105, 190 },
    Color { 127, 235, 178, 190 },
    Color { 196, 103, 224, 190 },
    Color { 202, 209, 79, 190 },
    Color { 117, 194, 235, 190 },
    Color { 224, 121, 158, 190 },
    Color { 104, 209, 96, 190 },
    Color { 122, 89, 235, 190 },
    Color { 224, 170, 112, 190 }
};

consteval bool roomColorsAreUnique()
{
    for (std::size_t first = 0; first < ROOM_COLORS.size(); ++first) {
        for (std::size_t second = first + 1; second < ROOM_COLORS.size(); ++second) {
            const Color a = ROOM_COLORS[first];
            const Color b = ROOM_COLORS[second];
            if (a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a) {
                return false;
            }
        }
    }
    return true;
}

static_assert(ROOM_COLORS.size() == MAX_GENERATED_ROOMS);
static_assert(roomColorsAreUnique());
constexpr double OUTLINE_KEY_SCALE = 1000.0;

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
};

Vector2 toVector2(Point point)
{
    return Vector2 { point.x, point.y };
}

DualMesh buildRenderDualMesh(const StalbergGrid& grid)
{
    const DualGrid source = buildDualGrid(grid);
    DualMesh result;
    result.quadCenters = source.quadCenters;
    result.cells.resize(source.cells.size());
    for (std::size_t cell = 0; cell < source.cells.size(); ++cell) {
        result.cells[cell].reserve(source.cells[cell].polygon.size());
        for (const Point point : source.cells[cell].polygon) {
            result.cells[cell].push_back(toVector2(point));
        }
    }
    return result;
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
        DrawTriangle(center, polygon[(i + 1) % polygon.size()], polygon[i], fill);
    }
}

void drawConnectedCells(
    const StalbergGrid& grid,
    const DualMesh& dual,
    const std::vector<bool>& generatedCells,
    float cameraZoom,
    Color fillColor,
    Color outlineColor)
{
    const std::size_t cellCount = dual.cells.size();
    const auto isGenerated = [&](std::size_t cell) {
        return cell < generatedCells.size() && generatedCells[cell];
    };

    for (std::size_t cell = 0; cell < cellCount; ++cell) {
        if (isGenerated(cell)) {
            drawCellFill(grid, dual, cell, fillColor);
        }
    }

    std::unordered_map<OutlineEdge, OutlineSegment, OutlineEdgeHash> outlines;
    for (std::size_t cell = 0; cell < cellCount; ++cell) {
        if (!isGenerated(cell)) {
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

        const Color color = outlineColor;
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
            color
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
            drawQuadraticCurve(
                trimmedEndpoint(first), corner, trimmedEndpoint(second), lineWidth, outlineColor);
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
            DrawCircleV(junction, lineWidth * 0.5F, outlineColor);
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
    return findDualCellAtPoint(buildDualGrid(grid), point);
}

void drawGrid(
    const StalbergGrid& grid,
    const rooms::RoomLayout& rooms,
    bool drawCenters,
    float cameraZoom,
    std::optional<std::size_t> hoveredCell)
{
    const float zoom = std::max(cameraZoom, 0.01F);
    const float lineWidth = 1.25F / zoom;
    const auto vertices = grid.getVertices();
    const auto quads = grid.getQuads();
    const auto assignments = rooms.getCellAssignments();
    const DualMesh dual = buildRenderDualMesh(grid);
    std::vector<bool> regionMask(vertices.size(), false);

    for (const GeneratedRoom& room : rooms.getRooms()) {
        for (std::size_t cell = 0; cell < assignments.size(); ++cell) {
            regionMask[cell] = assignments[cell] == room.id;
        }
        const Color fill = ROOM_COLORS.at(static_cast<std::size_t>(room.id));
        drawConnectedCells(
            grid, dual, regionMask, zoom, fill, ROOM_OUTLINE);
    }

    if (hoveredCell && *hoveredCell < assignments.size()) {
        const int hoveredRegion = assignments[*hoveredCell];
        if (hoveredRegion != EMPTY_CELL) {
            for (std::size_t cell = 0; cell < assignments.size(); ++cell) {
                if (assignments[cell] == hoveredRegion) {
                    drawCellFill(grid, dual, cell, HOVER_FILL);
                }
            }
        }
    }

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
