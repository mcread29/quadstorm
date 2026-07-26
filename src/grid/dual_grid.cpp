#include "grid/dual_grid.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>

namespace stalberg {
namespace {

struct EdgeHash {
    std::size_t operator()(const Edge& edge) const
    {
        return std::hash<std::size_t> {}(edge.a)
            ^ (std::hash<std::size_t> {}(edge.b) << 1U);
    }
};

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

Point reflectAcrossEdge(Point point, Point first, Point second)
{
    const Point edge { second.x - first.x, second.y - first.y };
    const float lengthSquared = edge.x * edge.x + edge.y * edge.y;
    if (lengthSquared <= 0.0001F) {
        return point;
    }

    const float projectionAmount = ((point.x - first.x) * edge.x
        + (point.y - first.y) * edge.y) / lengthSquared;
    const Point projection {
        first.x + edge.x * projectionAmount,
        first.y + edge.y * projectionAmount
    };
    return Point {
        2.0F * projection.x - point.x,
        2.0F * projection.y - point.y
    };
}

float distance(Point first, Point second)
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return std::sqrt(x * x + y * y);
}

float distanceToSegment(Point point, Point first, Point second)
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    const float lengthSquared = x * x + y * y;
    if (lengthSquared <= 0.0001F) {
        return distance(point, first);
    }

    const float amount = std::clamp(
        ((point.x - first.x) * x + (point.y - first.y) * y) / lengthSquared,
        0.0F,
        1.0F);
    return distance(point, Point { first.x + x * amount, first.y + y * amount });
}

void measureCell(DualCell& cell)
{
    if (cell.polygon.size() < 3) {
        return;
    }

    float twiceArea = 0.0F;
    cell.clearance = std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < cell.polygon.size(); ++index) {
        const Point first = cell.polygon[index];
        const Point second = cell.polygon[(index + 1) % cell.polygon.size()];
        twiceArea += first.x * second.y - second.x * first.y;
        cell.clearance = std::min(
            cell.clearance, distanceToSegment(cell.center, first, second));
    }
    cell.area = std::abs(twiceArea) * 0.5F;
    if (!std::isfinite(cell.clearance)) {
        cell.clearance = 0.0F;
    }
}

bool pointInPolygon(const std::vector<Point>& polygon, Point point)
{
    if (polygon.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t index = 0, previous = polygon.size() - 1;
         index < polygon.size(); previous = index++) {
        const Point first = polygon[index];
        const Point second = polygon[previous];
        const bool crossesScanline = (first.y > point.y) != (second.y > point.y);
        if (!crossesScanline) {
            continue;
        }
        const float crossingX = (second.x - first.x) * (point.y - first.y)
                / (second.y - first.y)
            + first.x;
        if (point.x < crossingX) {
            inside = !inside;
        }
    }
    return inside;
}

} // namespace

DualGrid buildDualGrid(const StalbergGrid& grid)
{
    const auto vertices = grid.getVertices();
    const auto quads = grid.getQuads();
    DualGrid dual;
    dual.quadCenters.reserve(quads.size());
    dual.cells.resize(vertices.size());

    for (VertexIndex cell = 0; cell < vertices.size(); ++cell) {
        dual.cells[cell].center = vertices[cell].position;
    }

    std::vector<std::size_t> incidentQuadCounts(vertices.size(), 0);
    std::vector<std::size_t> boundaryEdgeCounts(vertices.size(), 0);
    std::unordered_map<Edge, std::vector<std::size_t>, EdgeHash> edgeQuads;

    for (std::size_t quadIndex = 0; quadIndex < quads.size(); ++quadIndex) {
        const Quad& quad = quads[quadIndex];
        const Point center = quadCenter(grid, quad);
        dual.quadCenters.push_back(center);
        for (const VertexIndex vertex : quad) {
            dual.cells[vertex].polygon.push_back(center);
            ++incidentQuadCounts[vertex];
        }
        for (std::size_t edge = 0; edge < quad.size(); ++edge) {
            edgeQuads[Edge(quad[edge], quad[(edge + 1) % quad.size()])]
                .push_back(quadIndex);
        }
    }

    dual.connections.reserve(grid.getEdges().size());
    for (const Edge& edge : grid.getEdges()) {
        const auto found = edgeQuads.find(edge);
        if (found == edgeQuads.end() || found->second.empty()) {
            continue;
        }

        const auto& incidentQuads = found->second;
        Point first = dual.quadCenters[incidentQuads.front()];
        Point second = first;
        if (incidentQuads.size() >= 2) {
            second = dual.quadCenters[incidentQuads[1]];
        } else {
            second = reflectAcrossEdge(
                first, vertices[edge.a].position, vertices[edge.b].position);
            dual.cells[edge.a].polygon.push_back(second);
            dual.cells[edge.b].polygon.push_back(second);
            ++boundaryEdgeCounts[edge.a];
            ++boundaryEdgeCounts[edge.b];
        }
        dual.connections.push_back(
            DualConnection { edge, first, second, distance(first, second) });
    }

    for (VertexIndex cell = 0; cell < vertices.size(); ++cell) {
        DualCell& dualCell = dual.cells[cell];
        if (incidentQuadCounts[cell] == 1 && boundaryEdgeCounts[cell] >= 2
            && !dualCell.polygon.empty()) {
            const Point realCenter = dualCell.polygon.front();
            dualCell.polygon.push_back(Point {
                2.0F * dualCell.center.x - realCenter.x,
                2.0F * dualCell.center.y - realCenter.y
            });
        }

        std::ranges::sort(dualCell.polygon, [&](Point lhs, Point rhs) {
            return std::atan2(lhs.y - dualCell.center.y, lhs.x - dualCell.center.x)
                < std::atan2(rhs.y - dualCell.center.y, rhs.x - dualCell.center.x);
        });
        dualCell.polygon.erase(std::unique(dualCell.polygon.begin(),
                                       dualCell.polygon.end(),
                                       [](Point lhs, Point rhs) {
                                           const float x = lhs.x - rhs.x;
                                           const float y = lhs.y - rhs.y;
                                           return x * x + y * y < 0.0001F;
                                       }),
            dualCell.polygon.end());
        measureCell(dualCell);
    }

    return dual;
}

std::optional<VertexIndex> findDualCellAtPoint(const DualGrid& dual, Point point)
{
    for (VertexIndex cell = 0; cell < dual.cells.size(); ++cell) {
        if (pointInPolygon(dual.cells[cell].polygon, point)) {
            return cell;
        }
    }
    return std::nullopt;
}

} // namespace stalberg
