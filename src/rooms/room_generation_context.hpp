#pragma once

#include "rooms/room_generation_validation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

namespace stalberg::rooms::detail {
namespace context_detail {

inline float distance(CellPoint first, CellPoint second)
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return std::sqrt(x * x + y * y);
}

inline std::vector<bool> makeBuildableMask(
    const RoomGrid& grid, bool topologyValid)
{
    std::vector<bool> result(grid.cells.size(), false);
    if (!topologyValid) {
        return result;
    }
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        result[cell] = grid.cells[cell].buildable;
    }
    return result;
}

inline std::vector<CellIndex> collectBuildableCells(
    const std::vector<bool>& buildable)
{
    std::vector<CellIndex> result;
    for (CellIndex cell = 0; cell < buildable.size(); ++cell) {
        if (buildable[cell]) {
            result.push_back(cell);
        }
    }
    return result;
}

inline std::vector<CellIndex> canonicalEntrances(const RoomGrid& grid)
{
    std::vector<CellIndex> result = grid.entranceCandidates;
    std::ranges::sort(result);
    return result;
}

inline CellIndex findCenter(
    const RoomGrid& grid, const std::vector<CellIndex>& buildableCells)
{
    if (buildableCells.empty()) {
        return grid.cells.size();
    }
    return *std::ranges::min_element(buildableCells,
        [&](CellIndex lhs, CellIndex rhs) {
            const auto distanceFromOrigin = [](CellPoint point) {
                return std::sqrt(point.x * point.x + point.y * point.y);
            };
            return distanceFromOrigin(grid.cells[lhs].position)
                < distanceFromOrigin(grid.cells[rhs].position);
        });
}

inline float estimateCellScale(const RoomGrid& grid,
    const CellAdjacency& adjacency,
    const std::vector<bool>& buildable)
{
    float total = 0.0F;
    std::size_t count = 0;
    for (CellIndex cell = 0; cell < adjacency.size(); ++cell) {
        if (!buildable[cell]) {
            continue;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor > cell && buildable[neighbor]) {
                total += distance(
                    grid.cells[cell].position, grid.cells[neighbor].position);
                ++count;
            }
        }
    }
    return count == 0 ? 24.0F : total / static_cast<float>(count);
}

inline float buildableArea(
    const RoomGrid& grid, const std::vector<bool>& buildable)
{
    float result = 0.0F;
    for (CellIndex cell = 0; cell < buildable.size(); ++cell) {
        if (buildable[cell]) {
            result += grid.cells[cell].area;
        }
    }
    return result;
}

inline float averageBuildablePortalWidth(
    const RoomGrid& grid,
    const CellAdjacency& adjacency,
    const std::vector<bool>& buildable)
{
    float total = 0.0F;
    std::size_t count = 0;
    for (CellIndex cell = 0; cell < adjacency.size(); ++cell) {
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor > cell && buildable[cell] && buildable[neighbor]) {
                total += sharedBoundaryLength(grid, cell, neighbor);
                ++count;
            }
        }
    }
    return count == 0 ? 1.0F : total / static_cast<float>(count);
}

} // namespace context_detail

struct PreparedGenerationContext {
    explicit PreparedGenerationContext(const RoomGrid& input)
        : grid(input)
        , valid(topologyIsValid(input))
        , adjacency(valid ? canonicalAdjacency(input) : CellAdjacency {})
        , buildable(context_detail::makeBuildableMask(input, valid))
        , buildableCells(context_detail::collectBuildableCells(buildable))
        , canonicalEntrances(context_detail::canonicalEntrances(input))
        , fingerprint(valid
                  ? detail::canonicalTopologyFingerprint(input)
                  : 0)
        , center(context_detail::findCenter(input, buildableCells))
        , cellScale(valid
                  ? context_detail::estimateCellScale(input, adjacency, buildable)
                  : 24.0F)
        , buildableArea(valid
                  ? context_detail::buildableArea(input, buildable)
                  : 0.0F)
        , averageBuildablePortalWidth(valid
                  ? context_detail::averageBuildablePortalWidth(
                        input, adjacency, buildable)
                  : 1.0F)
    {
    }

    const RoomGrid& grid;
    const bool valid;
    const CellAdjacency adjacency;
    const std::vector<bool> buildable;
    const std::vector<CellIndex> buildableCells;
    const std::vector<CellIndex> canonicalEntrances;
    const std::uint64_t fingerprint;
    const CellIndex center;
    const float cellScale;
    const float buildableArea;
    const float averageBuildablePortalWidth;
};

} // namespace stalberg::rooms::detail
