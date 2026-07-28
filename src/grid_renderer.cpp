#include "grid_renderer.hpp"

#include "geometry/quantized_geometry.hpp"
#include "grid/dual_grid.hpp"
#include "grid/stalberg_grid.hpp"
#include "rooms/room_layout.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
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
struct DualMesh {
    std::vector<Point> quadCenters;
    std::vector<std::vector<Vector2>> cells;
};

using PointKey = geometry::QuantizedPoint;
using OutlineEdge = geometry::QuantizedEdge;
using PointKeyHash = geometry::QuantizedPointHash;
using OutlineEdgeHash = geometry::QuantizedEdgeHash;

struct OutlineSegment {
    Vector2 first;
    Vector2 second;
    std::size_t useCount = 1;
};

struct BoundaryNode {
    PointKey key;
    std::vector<std::size_t> incident;
};

struct RoomRenderData {
    int id = EMPTY_CELL;
    std::vector<std::size_t> cells;
    std::vector<OutlineSegment> boundary;
    std::vector<BoundaryNode> nodes;
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

DualMesh buildRenderDualMesh(const DualGrid& source)
{
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
    return geometry::quantizePoint(point.x, point.y);
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

RoomRenderData buildRoomRenderData(
    int room, std::span<const int> assignments, const DualMesh& dual)
{
    RoomRenderData result;
    result.id = room;
    std::unordered_map<OutlineEdge, OutlineSegment, OutlineEdgeHash> outlines;
    for (std::size_t cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] != room || cell >= dual.cells.size()) {
            continue;
        }
        result.cells.push_back(cell);
        const auto& polygon = dual.cells[cell];
        for (std::size_t point = 0; point < polygon.size(); ++point) {
            const Vector2 first = polygon[point];
            const Vector2 second = polygon[(point + 1) % polygon.size()];
            const OutlineEdge edge(pointKey(first), pointKey(second));
            if (const auto found = outlines.find(edge); found != outlines.end()) {
                ++found->second.useCount;
            } else {
                outlines.emplace(edge, OutlineSegment { first, second });
            }
        }
    }

    std::unordered_map<PointKey, std::vector<std::size_t>, PointKeyHash> nodes;
    for (const auto& [edge, segment] : outlines) {
        static_cast<void>(edge);
        if (segment.useCount != 1) {
            continue;
        }
        const std::size_t index = result.boundary.size();
        result.boundary.push_back(segment);
        nodes[pointKey(segment.first)].push_back(index);
        nodes[pointKey(segment.second)].push_back(index);
    }
    result.nodes.reserve(nodes.size());
    for (auto& [key, incident] : nodes) {
        result.nodes.push_back(BoundaryNode { key, std::move(incident) });
    }
    return result;
}

void drawConnectedCells(const StalbergGrid& grid, const DualMesh& dual,
    const RoomRenderData& room, float cameraZoom,
    Color fillColor, Color outlineColor)
{
    for (const std::size_t cell : room.cells) {
        drawCellFill(grid, dual, cell, fillColor);
    }

    const float zoom = std::max(cameraZoom, 0.01F);
    const float lineWidth = 1.7F / zoom;
    const float maximumCornerRadius = 7.0F / zoom;
    std::vector<RoundedSegment> boundary;
    boundary.reserve(room.boundary.size());
    for (const OutlineSegment& segment : room.boundary) {
        const Vector2 direction {
            segment.second.x - segment.first.x,
            segment.second.y - segment.first.y
        };
        const float length = std::sqrt(
            direction.x * direction.x + direction.y * direction.y);
        if (length <= 0.0001F) {
            boundary.push_back(RoundedSegment {});
            continue;
        }
        const float cornerRadius = std::min(maximumCornerRadius, length * 0.28F);
        boundary.push_back(RoundedSegment {
            pointKey(segment.first),
            pointKey(segment.second),
            segment.first,
            segment.second,
            Vector2 {
                segment.first.x + direction.x / length * cornerRadius,
                segment.first.y + direction.y / length * cornerRadius
            },
            Vector2 {
                segment.second.x - direction.x / length * cornerRadius,
                segment.second.y - direction.y / length * cornerRadius
            },
            outlineColor
        });
    }

    for (const RoundedSegment& segment : boundary) {
        DrawLineEx(segment.firstTrimmed, segment.secondTrimmed,
            lineWidth, segment.color);
    }

    for (const BoundaryNode& node : room.nodes) {
        const auto endpoint = [&](const RoundedSegment& segment) {
            return segment.firstKey == node.key ? segment.first : segment.second;
        };
        const auto trimmedEndpoint = [&](const RoundedSegment& segment) {
            return segment.firstKey == node.key
                ? segment.firstTrimmed
                : segment.secondTrimmed;
        };
        if (node.incident.size() == 2) {
            const RoundedSegment& first = boundary[node.incident[0]];
            const RoundedSegment& second = boundary[node.incident[1]];
            drawQuadraticCurve(trimmedEndpoint(first), endpoint(first),
                trimmedEndpoint(second), lineWidth, outlineColor);
        } else if (!node.incident.empty()) {
            const Vector2 junction = endpoint(boundary[node.incident.front()]);
            for (const std::size_t segmentIndex : node.incident) {
                DrawLineEx(trimmedEndpoint(boundary[segmentIndex]), junction,
                    lineWidth, outlineColor);
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

struct GridRendererCache::Impl {
    DualGrid dual;
    DualMesh mesh;
    std::vector<RoomRenderData> rooms;
    bool gridReady = false;
};

GridRendererCache::GridRendererCache()
    : impl(std::make_unique<Impl>())
{
}

GridRendererCache::~GridRendererCache() = default;
GridRendererCache::GridRendererCache(GridRendererCache&&) noexcept = default;
GridRendererCache& GridRendererCache::operator=(GridRendererCache&&) noexcept = default;

void GridRendererCache::rebuildGrid(const StalbergGrid& grid)
{
    impl->dual = buildDualGrid(grid);
    impl->mesh = buildRenderDualMesh(impl->dual);
    impl->rooms.clear();
    impl->gridReady = true;
}

void GridRendererCache::rebuildRooms(const rooms::RoomLayout& layout)
{
    if (!impl->gridReady
        || layout.getCellAssignments().size() != impl->mesh.cells.size()) {
        throw std::invalid_argument(
            "room render cache requires aligned grid and room artifacts");
    }
    impl->rooms.clear();
    impl->rooms.reserve(layout.getRooms().size());
    for (const GeneratedRoom& room : layout.getRooms()) {
        impl->rooms.push_back(buildRoomRenderData(
            room.id, layout.getCellAssignments(), impl->mesh));
    }
}

const DualGrid& GridRendererCache::dualGrid() const
{
    if (!impl->gridReady) {
        throw std::logic_error("grid render cache has not been built");
    }
    return impl->dual;
}

std::optional<std::size_t> GridRendererCache::findCellAtPoint(Point point) const
{
    if (!impl->gridReady) {
        return std::nullopt;
    }
    return findDualCellAtPoint(impl->dual, point);
}

void GridRendererCache::draw(const StalbergGrid& grid,
    const rooms::RoomLayout& layout, bool drawCenters,
    float cameraZoom, std::optional<std::size_t> hoveredCell) const
{
    if (!impl->gridReady || grid.getVertexCount() != impl->mesh.cells.size()) {
        throw std::logic_error("stale grid render cache");
    }
    const float zoom = std::max(cameraZoom, 0.01F);
    const float lineWidth = 1.25F / zoom;
    const auto vertices = grid.getVertices();
    const auto quads = grid.getQuads();
    const auto assignments = layout.getCellAssignments();

    for (const RoomRenderData& room : impl->rooms) {
        const Color fill = ROOM_COLORS.at(static_cast<std::size_t>(room.id));
        drawConnectedCells(grid, impl->mesh, room, zoom, fill, ROOM_OUTLINE);
    }

    if (hoveredCell && *hoveredCell < assignments.size()) {
        const int hoveredRegion = assignments[*hoveredCell];
        const auto room = std::ranges::find(
            impl->rooms, hoveredRegion, &RoomRenderData::id);
        if (room != impl->rooms.end()) {
            for (const std::size_t cell : room->cells) {
                drawCellFill(grid, impl->mesh, cell, HOVER_FILL);
            }
        }
    }

    for (const Edge& edge : grid.getEdges()) {
        DrawLineEx(toVector2(vertices[edge.a].position),
            toVector2(vertices[edge.b].position), lineWidth, GRID_COLOR);
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
        drawOrientedCross(grid, quads[i], impl->mesh.quadCenters[i],
            centerSize, centerLineWidth, CENTER_COLOR);
    }
}

} // namespace stalberg
