#pragma once

#include "rooms/room_generation_context.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ranges>
#include <vector>

namespace stalberg::rooms::detail {

inline float candidateScore(const PreparedGenerationContext& prepared,
    const RoomLayout& layout,
    RoomGenerationMethod method)
{
    const RoomGrid& grid = prepared.grid;
    const auto rooms = layout.getRooms();
    const auto assignments = layout.getCellAssignments();
    float floorArea = 0.0F;
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        if (assignments[cell] != EMPTY_CELL) {
            floorArea += grid.cells[cell].area;
        }
    }
    const float coverage = prepared.buildableArea <= 0.0F
        ? 0.0F
        : floorArea / prepared.buildableArea;
    const float coverageScore = std::clamp(
        1.0F - std::abs(coverage - 0.48F) / 0.16F, 0.0F, 1.0F);

    float doorwayQuality = 0.0F;
    float minimumWidthRatio = 1.0F;
    for (const Doorway& doorway : layout.getDoorways()) {
        const float width = sharedBoundaryLength(
            grid, doorway.firstCell, doorway.secondCell);
        const float ratio = std::clamp(width
                / std::max(prepared.averageBuildablePortalWidth, 0.001F),
            0.0F,
            1.5F);
        doorwayQuality += ratio / 1.5F;
        minimumWidthRatio = std::min(minimumWidthRatio, std::min(ratio, 1.0F));
    }
    if (!layout.getDoorways().empty()) {
        doorwayQuality /= static_cast<float>(layout.getDoorways().size());
    }

    float meanArea = 0.0F;
    for (const GeneratedRoom& room : rooms) {
        meanArea += room.area;
    }
    meanArea /= static_cast<float>(rooms.size());
    float areaVariance = 0.0F;
    for (const GeneratedRoom& room : rooms) {
        const float difference = room.area - meanArea;
        areaVariance += difference * difference;
    }
    areaVariance /= static_cast<float>(rooms.size());
    const float variation = meanArea <= 0.0F
        ? 0.0F
        : std::sqrt(areaVariance) / meanArea;
    const float variationScore = std::clamp(
        1.0F - std::abs(variation - 0.55F) / 0.55F, 0.0F, 1.0F);

    const std::size_t loopCount = layout.getDoorways().size() >= rooms.size() - 1
        ? layout.getDoorways().size() - (rooms.size() - 1)
        : 0;
    const std::size_t arenaCount = std::ranges::count_if(rooms,
        [](const GeneratedRoom& room) {
            return room.role != RoomRole::Connector;
        });
    const std::size_t desiredLoops = method == RoomGenerationMethod::ShooterLayout
        ? (arenaCount >= 5 ? 1 : 0)
        : (rooms.size() < 4
                ? 0
                : std::min<std::size_t>(
                    4, std::max<std::size_t>(1, rooms.size() / 6)));
    const float loopScore = desiredLoops == 0
        ? 1.0F
        : std::min(1.0F,
              static_cast<float>(loopCount) / static_cast<float>(desiredLoops));

    const float entranceScore = std::clamp(
        static_cast<float>(layout.getConnectedEntrances().size()) / 6.0F,
        0.0F,
        1.0F);

    std::vector<std::vector<int>> roomGraph(rooms.size());
    for (const Doorway& doorway : layout.getDoorways()) {
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }
    const auto start = std::ranges::find(
        rooms, RoomRole::Start, &GeneratedRoom::role);
    const auto exit = std::ranges::find(
        rooms, RoomRole::Exit, &GeneratedRoom::role);
    const std::vector<int> distances
        = breadthFirstDistances(roomGraph, start->id);
    const float normalizedPath = static_cast<float>(
        distances[static_cast<std::size_t>(exit->id)])
        / static_cast<float>(std::max<std::size_t>(rooms.size() - 1, 1));
    const float pathScore = std::clamp(
        1.0F - std::abs(normalizedPath - 0.65F) / 0.65F, 0.0F, 1.0F);
    const std::size_t deadEnds = std::ranges::count_if(roomGraph,
        [](const auto& connections) { return connections.size() == 1; });
    const float deadEndRatio
        = static_cast<float>(deadEnds) / static_cast<float>(rooms.size());
    const float branchScore = std::clamp(
        1.0F - std::abs(deadEndRatio - 0.25F) / 0.35F, 0.0F, 1.0F);
    const std::size_t maximumDegree = std::ranges::max(
        roomGraph | std::views::transform([](const auto& connections) {
            return connections.size();
        }));
    const float degreeScore = maximumDegree <= 4
        ? 1.0F
        : 1.0F / static_cast<float>(maximumDegree - 3);
    const float circulationScore
        = pathScore * 0.5F + branchScore * 0.3F + degreeScore * 0.2F;

    return coverageScore * 20.0F
        + doorwayQuality * 20.0F
        + minimumWidthRatio * 16.0F
        + variationScore * 12.0F
        + loopScore * 10.0F
        + circulationScore * 16.0F
        + entranceScore * 6.0F;
}

} // namespace stalberg::rooms::detail
