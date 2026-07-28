#pragma once

#include "rooms/room_graph_operations.hpp"
#include "rooms/room_layout.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

namespace stalberg::rooms::detail {

inline std::uint64_t mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

inline bool topologyIsValid(const RoomGrid& grid)
{
    if (grid.connections.size() != grid.cells.size()) {
        return false;
    }
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        const Cell& geometry = grid.cells[cell];
        if (!std::isfinite(geometry.position.x) || !std::isfinite(geometry.position.y)
            || !std::isfinite(geometry.area) || geometry.area <= 0.0F
            || !std::isfinite(geometry.clearance) || geometry.clearance < 0.0F) {
            return false;
        }

        std::vector<CellIndex> neighbors;
        neighbors.reserve(grid.connections[cell].size());
        for (const CellConnection& connection : grid.connections[cell]) {
            neighbors.push_back(connection.cell);
        }
        std::ranges::sort(neighbors);
        if (std::ranges::adjacent_find(neighbors) != neighbors.end()) {
            return false;
        }
        for (const CellConnection& connection : grid.connections[cell]) {
            if (connection.cell >= grid.cells.size() || connection.cell == cell
                || !std::isfinite(connection.distance) || connection.distance <= 0.0F
                || !std::isfinite(connection.sharedBoundaryLength)
                || connection.sharedBoundaryLength <= 0.0F) {
                return false;
            }
            const auto reverse = std::ranges::find(grid.connections[connection.cell],
                cell,
                &CellConnection::cell);
            if (reverse == grid.connections[connection.cell].end()) {
                return false;
            }
            const float distanceTolerance = std::max(connection.distance, 1.0F) * 0.0001F;
            const float widthTolerance
                = std::max(connection.sharedBoundaryLength, 1.0F) * 0.0001F;
            if (std::abs(reverse->distance - connection.distance) > distanceTolerance
                || std::abs(reverse->sharedBoundaryLength
                       - connection.sharedBoundaryLength)
                    > widthTolerance) {
                return false;
            }
        }
    }

    std::vector<CellIndex> entrances = grid.entranceCandidates;
    std::ranges::sort(entrances);
    return std::ranges::all_of(entrances, [&](CellIndex entrance) {
               return entrance < grid.cells.size();
           })
        && std::ranges::adjacent_find(entrances) == entrances.end();
}

inline std::uint64_t canonicalTopologyFingerprint(const RoomGrid& grid)
{
    std::uint64_t fingerprint = mix(grid.cells.size());
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        const Cell& value = grid.cells[cell];
        const bool blocked = !value.buildable;
        fingerprint = mix(fingerprint
            ^ (static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(
                    value.position.x))
                << 32U)
            ^ std::bit_cast<std::uint32_t>(value.position.y)
            ^ static_cast<std::uint64_t>(blocked));
        fingerprint = mix(fingerprint
            ^ std::bit_cast<std::uint32_t>(value.area)
            ^ (static_cast<std::uint64_t>(
                   std::bit_cast<std::uint32_t>(value.clearance))
                << 32U));

        std::vector<CellConnection> connections = grid.connections[cell];
        std::ranges::sort(connections, {}, &CellConnection::cell);
        for (const CellConnection& connection : connections) {
            fingerprint = mix(fingerprint ^ mix(cell)
                ^ (mix(connection.cell) << 1U)
                ^ std::bit_cast<std::uint32_t>(connection.sharedBoundaryLength)
                ^ (static_cast<std::uint64_t>(
                       std::bit_cast<std::uint32_t>(connection.distance))
                    << 32U));
        }
    }

    std::vector<CellIndex> entrances = grid.entranceCandidates;
    std::ranges::sort(entrances);
    for (const CellIndex entrance : entrances) {
        fingerprint = mix(fingerprint ^ mix(entrance));
    }
    return fingerprint;
}

inline bool candidateIsValid(const RoomGrid& grid,
    const CellAdjacency& adjacency,
    const RoomLayout& layout,
    std::size_t minimumRoomSize,
    std::size_t maximumRoomCount)
{
    const auto assignments = layout.getCellAssignments();
    const auto rooms = layout.getRooms();
    if (assignments.size() != grid.cells.size() || rooms.size() < 2
        || rooms.size() > maximumRoomCount) {
        return false;
    }

    std::vector<std::size_t> measuredRoomSizes(rooms.size(), 0);
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int room = assignments[cell];
        if (room != EMPTY_CELL
            && (room < 0 || static_cast<std::size_t>(room) >= rooms.size())) {
            return false;
        }
        if (room >= 0) {
            ++measuredRoomSizes[static_cast<std::size_t>(room)];
        }
    }
    for (std::size_t room = 0; room < rooms.size(); ++room) {
        if (rooms[room].id != static_cast<int>(room)
            || measuredRoomSizes[room] < minimumRoomSize
            || measuredRoomSizes[room] != rooms[room].cellCount) {
            return false;
        }
        const auto firstCell = std::ranges::find(assignments, static_cast<int>(room));
        const CellIndex start
            = static_cast<CellIndex>(firstCell - assignments.begin());
        const std::size_t reachedCount = reachableCellCount(adjacency,
            start,
            [&](CellIndex cell) {
                return assignments[cell] == static_cast<int>(room);
            });
        if (reachedCount != measuredRoomSizes[room]) {
            return false;
        }
    }
    if (rooms.size() > 1 && layout.getDoorways().size() < rooms.size() - 1) {
        return false;
    }

    std::vector<std::vector<int>> graph(rooms.size());
    for (const Doorway& doorway : layout.getDoorways()) {
        if (doorway.firstRegion < 0 || doorway.secondRegion < 0
            || static_cast<std::size_t>(doorway.firstRegion) >= rooms.size()
            || static_cast<std::size_t>(doorway.secondRegion) >= rooms.size()
            || doorway.firstCell >= assignments.size()
            || doorway.secondCell >= assignments.size()
            || assignments[doorway.firstCell] != doorway.firstRegion
            || assignments[doorway.secondCell] != doorway.secondRegion
            || std::ranges::find(adjacency[doorway.firstCell], doorway.secondCell)
                == adjacency[doorway.firstCell].end()) {
            return false;
        }
        graph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        graph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }

    const std::size_t startRooms = std::ranges::count(
        rooms, RoomRole::Start, &GeneratedRoom::role);
    const std::size_t exitRooms = std::ranges::count(
        rooms, RoomRole::Exit, &GeneratedRoom::role);
    if (startRooms != 1 || exitRooms != 1) {
        return false;
    }

    const std::vector<int> distances = breadthFirstDistances(graph, 0);
    return std::ranges::all_of(
        distances, [](int value) { return value >= 0; });
}

inline bool shooterCandidateIsValid(const RoomLayout& layout)
{
    const auto rooms = layout.getRooms();
    std::size_t arenaCount = 0;
    std::size_t connectorCount = 0;
    int startRoom = EMPTY_CELL;
    int exitRoom = EMPTY_CELL;
    std::vector<std::size_t> doorwayDegrees(rooms.size(), 0);
    std::vector<std::vector<int>> roomGraph(rooms.size());
    for (const GeneratedRoom& room : rooms) {
        if (room.role == RoomRole::Start) {
            startRoom = room.id;
        } else if (room.role == RoomRole::Exit) {
            exitRoom = room.id;
        }
        if (room.role == RoomRole::Connector) {
            ++connectorCount;
        } else {
            ++arenaCount;
        }
    }
    for (const Doorway& doorway : layout.getDoorways()) {
        const bool firstConnector
            = rooms[static_cast<std::size_t>(doorway.firstRegion)].role
            == RoomRole::Connector;
        const bool secondConnector
            = rooms[static_cast<std::size_t>(doorway.secondRegion)].role
            == RoomRole::Connector;
        if (firstConnector && secondConnector) {
            return false;
        }
        ++doorwayDegrees[static_cast<std::size_t>(doorway.firstRegion)];
        ++doorwayDegrees[static_cast<std::size_t>(doorway.secondRegion)];
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }
    for (const GeneratedRoom& room : rooms) {
        if (room.role == RoomRole::Connector
            && (doorwayDegrees[static_cast<std::size_t>(room.id)] < 1
                || doorwayDegrees[static_cast<std::size_t>(room.id)] > 2)) {
            return false;
        }
    }
    if (arenaCount < 3 || connectorCount < 1
        || startRoom == EMPTY_CELL || exitRoom == EMPTY_CELL) {
        return false;
    }

    const std::vector<int> distances
        = breadthFirstDistances(roomGraph, startRoom);
    return distances[static_cast<std::size_t>(exitRoom)] >= 3;
}

} // namespace stalberg::rooms::detail
