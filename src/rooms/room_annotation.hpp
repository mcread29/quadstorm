#pragma once

#include "rooms/room_generation_random.hpp"
#include "rooms/room_graph_operations.hpp"
#include "rooms/room_layout.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <queue>
#include <ranges>
#include <vector>

namespace stalberg::rooms::detail {

void annotateRooms(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<int>& assignments,
    const std::vector<Doorway>& doorways,
    const std::vector<CellIndex>& connectedEntrances,
    CellIndex center,
    std::uint32_t generationSeed,
    std::vector<GeneratedRoom>& rooms,
    std::optional<CellIndex> preferredExit = std::nullopt)
{
    if (rooms.empty()) {
        return;
    }

    for (GeneratedRoom& room : rooms) {
        room.area = 0.0F;
        room.role = RoomRole::Combat;
        room.coverCandidates.clear();
        room.enemySpawnCandidates.clear();
    }
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int room = assignments[cell];
        if (room >= 0 && static_cast<std::size_t>(room) < rooms.size()) {
            rooms[static_cast<std::size_t>(room)].area += grid.cells[cell].area;
        }
    }

    std::vector<std::vector<int>> roomGraph(rooms.size());
    for (const Doorway& doorway : doorways) {
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }

    float totalArea = 0.0F;
    for (const GeneratedRoom& room : rooms) {
        totalArea += room.area;
    }
    const float averageArea = totalArea / static_cast<float>(rooms.size());
    for (GeneratedRoom& room : rooms) {
        const std::size_t degree = roomGraph[static_cast<std::size_t>(room.id)].size();
        if (degree >= 3) {
            room.role = RoomRole::Hub;
        } else if (degree == 2 && room.area < averageArea * 0.72F) {
            room.role = RoomRole::Connector;
        } else if (degree == 1
            && unitNoise(generationSeed, static_cast<std::uint64_t>(room.id) + 991U)
                < 0.55F) {
            room.role = RoomRole::Reward;
        }
    }

    const int startRoom = center < assignments.size() ? assignments[center] : EMPTY_CELL;
    if (startRoom < 0 || static_cast<std::size_t>(startRoom) >= rooms.size()) {
        return;
    }
    const std::vector<int> distances
        = breadthFirstDistances(roomGraph, startRoom);

    int exitRoom = preferredExit && *preferredExit < assignments.size()
        && assignments[*preferredExit] >= 0
        ? assignments[*preferredExit]
        : startRoom;
    if (!preferredExit) {
        for (const CellIndex entrance : connectedEntrances) {
            if (entrance >= assignments.size()) {
                continue;
            }
            const int room = assignments[entrance];
            if (room >= 0 && distances[static_cast<std::size_t>(room)]
                    > distances[static_cast<std::size_t>(exitRoom)]) {
                exitRoom = room;
            }
        }
    }
    if (exitRoom == startRoom && rooms.size() > 1) {
        exitRoom = static_cast<int>(std::ranges::max_element(distances) - distances.begin());
    }

    rooms[static_cast<std::size_t>(startRoom)].role = RoomRole::Start;
    if (exitRoom != startRoom) {
        rooms[static_cast<std::size_t>(exitRoom)].role = RoomRole::Exit;
    }

    std::vector<int> distanceFromDoor(assignments.size(), -1);
    std::queue<CellIndex> cellQueue;
    const auto seedDoorCell = [&](CellIndex cell) {
        if (cell < assignments.size() && distanceFromDoor[cell] < 0) {
            distanceFromDoor[cell] = 0;
            cellQueue.push(cell);
        }
    };
    for (const Doorway& doorway : doorways) {
        seedDoorCell(doorway.firstCell);
        seedDoorCell(doorway.secondCell);
    }
    while (!cellQueue.empty()) {
        const CellIndex cell = cellQueue.front();
        cellQueue.pop();
        for (const CellIndex neighbor : adjacency[cell]) {
            if (assignments[neighbor] == assignments[cell]
                && distanceFromDoor[neighbor] < 0) {
                distanceFromDoor[neighbor] = distanceFromDoor[cell] + 1;
                cellQueue.push(neighbor);
            }
        }
    }

    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int roomId = assignments[cell];
        if (roomId < 0 || distanceFromDoor[cell] < 1) {
            continue;
        }
        GeneratedRoom& room = rooms[static_cast<std::size_t>(roomId)];
        const bool touchesBoundary = std::ranges::any_of(
            adjacency[cell], [&](CellIndex neighbor) {
                return assignments[neighbor] != roomId;
            });
        if (touchesBoundary) {
            room.coverCandidates.push_back(cell);
        }
        if (distanceFromDoor[cell] >= 2 && grid.cells[cell].clearance > 0.0F) {
            room.enemySpawnCandidates.push_back(cell);
        }
    }
}

} // namespace stalberg::rooms::detail
