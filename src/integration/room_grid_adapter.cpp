#include "integration/room_grid_adapter.hpp"

#include "grid/dual_grid.hpp"
#include "grid/stalberg_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stalberg {
namespace {

std::vector<rooms::CellIndex> boundarySideCenters(const StalbergGrid& grid)
{
    constexpr std::size_t sideCount = 6;
    const auto vertices = grid.getVertices();
    std::vector<rooms::CellIndex> centers;
    centers.reserve(sideCount);

    for (std::size_t side = 0; side < sideCount; ++side) {
        const float angle = -std::numbers::pi_v<float> * 0.5F
            + static_cast<float>(side) * std::numbers::pi_v<float> / 3.0F;
        const float directionX = std::cos(angle);
        const float directionY = std::sin(angle);
        rooms::CellIndex selected = vertices.size();
        float bestProjection = -std::numeric_limits<float>::infinity();
        float bestTangentDistance = std::numeric_limits<float>::infinity();

        for (rooms::CellIndex cell = 0; cell < vertices.size(); ++cell) {
            if (!vertices[cell].fixed) {
                continue;
            }
            const Point point = vertices[cell].position;
            const float projection = point.x * directionX + point.y * directionY;
            const float tangentDistance = std::abs(
                -point.x * directionY + point.y * directionX);
            if (projection > bestProjection + 0.001F
                || (std::abs(projection - bestProjection) <= 0.001F
                    && (tangentDistance < bestTangentDistance - 0.001F
                        || (std::abs(tangentDistance - bestTangentDistance) <= 0.001F
                            && cell < selected)))) {
                selected = cell;
                bestProjection = projection;
                bestTangentDistance = tangentDistance;
            }
        }

        if (selected != vertices.size()
            && std::ranges::find(centers, selected) == centers.end()) {
            centers.push_back(selected);
        }
    }
    return centers;
}

void validateDualAlignment(const StalbergGrid& grid, const DualGrid& dual)
{
    const auto vertices = grid.getVertices();
    if (dual.cells.size() != vertices.size()) {
        throw std::invalid_argument(
            "dual grid cell count does not match the Stalberg grid");
    }
    for (std::size_t cell = 0; cell < vertices.size(); ++cell) {
        if (dual.cells[cell].center != vertices[cell].position) {
            throw std::invalid_argument(
                "dual grid cell centers do not align with the Stalberg grid");
        }
    }

    std::vector<std::pair<std::size_t, std::size_t>> expectedEdges;
    expectedEdges.reserve(grid.getEdges().size());
    for (const Edge& edge : grid.getEdges()) {
        expectedEdges.emplace_back(edge.a, edge.b);
    }
    std::ranges::sort(expectedEdges);

    std::vector<std::pair<std::size_t, std::size_t>> actualEdges;
    actualEdges.reserve(dual.connections.size());
    for (const DualConnection& connection : dual.connections) {
        if (connection.cells.a >= vertices.size()
            || connection.cells.b >= vertices.size()
            || connection.cells.a == connection.cells.b) {
            throw std::invalid_argument(
                "dual grid connection endpoints do not align with the Stalberg grid");
        }
        actualEdges.emplace_back(
            std::min(connection.cells.a, connection.cells.b),
            std::max(connection.cells.a, connection.cells.b));
    }
    std::ranges::sort(actualEdges);
    if (!std::ranges::equal(expectedEdges, actualEdges)) {
        throw std::invalid_argument(
            "dual grid connections do not match the Stalberg grid topology");
    }
}

} // namespace

rooms::RoomGrid makeRoomGrid(const StalbergGrid& grid)
{
    return makeRoomGrid(grid, buildDualGrid(grid));
}

rooms::RoomGrid makeRoomGrid(const StalbergGrid& grid, const DualGrid& dual)
{
    validateDualAlignment(grid, dual);

    rooms::RoomGrid result;
    result.cells.reserve(grid.getVertexCount());
    for (rooms::CellIndex cell = 0; cell < grid.getVertexCount(); ++cell) {
        const Vertex& vertex = grid.getVertices()[cell];
        const DualCell& dualCell = dual.cells[cell];
        result.cells.push_back(rooms::Cell {
            rooms::CellPoint { vertex.position.x, vertex.position.y },
            dualCell.area,
            dualCell.clearance,
            !vertex.fixed
        });
    }

    std::vector<std::vector<rooms::CellConnection>> connections(
        grid.getVertexCount());
    const auto vertices = grid.getVertices();
    for (const DualConnection& connection : dual.connections) {
        const rooms::CellIndex first = connection.cells.a;
        const rooms::CellIndex second = connection.cells.b;
        const float x = vertices[second].position.x - vertices[first].position.x;
        const float y = vertices[second].position.y - vertices[first].position.y;
        const float centerDistance = std::sqrt(x * x + y * y);
        connections[first].push_back(
            rooms::CellConnection { second, centerDistance, connection.length });
        connections[second].push_back(
            rooms::CellConnection { first, centerDistance, connection.length });
    }
    result.setConnections(std::move(connections));
    result.entranceCandidates = boundarySideCenters(grid);
    return result;
}

} // namespace stalberg
