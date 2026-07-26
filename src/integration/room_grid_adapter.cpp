#include "integration/room_grid_adapter.hpp"

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
    rooms::RoomGrid result;
    result.cells.reserve(grid.getVertexCount());
    for (const Vertex& vertex : grid.getVertices()) {
        result.cells.push_back(rooms::Cell {
            rooms::CellPoint { vertex.position.x, vertex.position.y },
            !vertex.fixed
        });
    }

    result.neighbors.reserve(grid.getNeighbors().size());
    for (const auto& neighbors : grid.getNeighbors()) {
        result.neighbors.emplace_back(neighbors.begin(), neighbors.end());
    }
    result.entranceCandidates = boundarySideCenters(grid);
    return result;
}

} // namespace stalberg
