#include "integration/room_grid_adapter.hpp"

#include "grid/dual_grid.hpp"
#include "grid/stalberg_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
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

} // namespace

rooms::RoomGrid makeRoomGrid(const StalbergGrid& grid)
{
    const DualGrid dual = buildDualGrid(grid);
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

    result.connections.resize(grid.getVertexCount());
    const auto vertices = grid.getVertices();
    for (const DualConnection& connection : dual.connections) {
        const rooms::CellIndex first = connection.cells.a;
        const rooms::CellIndex second = connection.cells.b;
        const float x = vertices[second].position.x - vertices[first].position.x;
        const float y = vertices[second].position.y - vertices[first].position.y;
        const float centerDistance = std::sqrt(x * x + y * y);
        result.connections[first].push_back(
            rooms::CellConnection { second, centerDistance, connection.length });
        result.connections[second].push_back(
            rooms::CellConnection { first, centerDistance, connection.length });
    }
    result.neighbors.resize(result.connections.size());
    for (std::size_t cell = 0; cell < result.connections.size(); ++cell) {
        auto& connections = result.connections[cell];
        std::ranges::sort(connections, {}, &rooms::CellConnection::cell);
        result.neighbors[cell].reserve(connections.size());
        for (const rooms::CellConnection& connection : connections) {
            result.neighbors[cell].push_back(connection.cell);
        }
    }
    result.entranceCandidates = boundarySideCenters(grid);
    return result;
}

} // namespace stalberg
