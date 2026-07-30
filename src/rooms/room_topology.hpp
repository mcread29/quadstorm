#pragma once

#include "rooms/room_graph_operations.hpp"
#include "rooms/room_layout.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

namespace stalberg::rooms::detail {

inline TopologySignature measureTopology(const RoomLayout& layout)
{
    TopologySignature result;
    const auto rooms = layout.getRooms();
    if (rooms.empty()) {
        return result;
    }

    std::vector<int> arenaIndexByRoom(rooms.size(), EMPTY_CELL);
    std::vector<int> arenaRooms;
    std::vector<std::vector<int>> roomGraph(rooms.size());
    std::vector<std::vector<int>> connectorNeighbors(rooms.size());
    std::set<std::pair<int, int>> contractedEdges;
    for (const GeneratedRoom& room : rooms) {
        if (room.role == RoomRole::Connector) {
            ++result.connectorCount;
        } else {
            arenaIndexByRoom[static_cast<std::size_t>(room.id)]
                = static_cast<int>(arenaRooms.size());
            arenaRooms.push_back(room.id);
        }
    }
    result.substantialRoomCount = arenaRooms.size();

    for (const Doorway& doorway : layout.getDoorways()) {
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
        const bool firstConnector = rooms[static_cast<std::size_t>(
            doorway.firstRegion)].role == RoomRole::Connector;
        const bool secondConnector = rooms[static_cast<std::size_t>(
            doorway.secondRegion)].role == RoomRole::Connector;
        if (!firstConnector && !secondConnector) {
            contractedEdges.insert(std::minmax(
                doorway.firstRegion, doorway.secondRegion));
            ++result.directArenaEdgeCount;
        } else if (firstConnector != secondConnector) {
            const int connector = firstConnector
                ? doorway.firstRegion : doorway.secondRegion;
            const int arena = firstConnector
                ? doorway.secondRegion : doorway.firstRegion;
            connectorNeighbors[static_cast<std::size_t>(connector)]
                .push_back(arena);
        }
    }
    for (const GeneratedRoom& room : rooms) {
        if (room.role != RoomRole::Connector) {
            continue;
        }
        auto& neighbors = connectorNeighbors[static_cast<std::size_t>(room.id)];
        std::ranges::sort(neighbors);
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()),
            neighbors.end());
        if (neighbors.size() == 2) {
            contractedEdges.insert(std::minmax(neighbors[0], neighbors[1]));
        }
    }

    result.contractedEdgeCount = contractedEdges.size();
    result.directArenaEdgeRatio = result.contractedEdgeCount == 0
        ? 0.0F
        : static_cast<float>(result.directArenaEdgeCount)
            / static_cast<float>(result.contractedEdgeCount);
    result.cycleRank = result.contractedEdgeCount + 1
            > result.substantialRoomCount
        ? result.contractedEdgeCount - result.substantialRoomCount + 1
        : 0;

    std::vector<std::vector<int>> arenaGraph(arenaRooms.size());
    for (const auto& [firstRoom, secondRoom] : contractedEdges) {
        const int first = arenaIndexByRoom[static_cast<std::size_t>(firstRoom)];
        const int second = arenaIndexByRoom[static_cast<std::size_t>(secondRoom)];
        if (first < 0 || second < 0) {
            continue;
        }
        arenaGraph[static_cast<std::size_t>(first)].push_back(second);
        arenaGraph[static_cast<std::size_t>(second)].push_back(first);
    }

    std::size_t minimumShortcutSavings
        = std::numeric_limits<std::size_t>::max();
    for (const MissionEdgeBrief& edge : layout.getMissionEdges()) {
        if (edge.purpose == MissionEdgePurpose::Primary
            || edge.firstArena < 0 || edge.secondArena < 0
            || static_cast<std::size_t>(edge.firstArena) >= arenaGraph.size()
            || static_cast<std::size_t>(edge.secondArena) >= arenaGraph.size()) {
            continue;
        }
        std::vector<int> distances(arenaGraph.size(), -1);
        std::queue<int> frontier;
        distances[static_cast<std::size_t>(edge.firstArena)] = 0;
        frontier.push(edge.firstArena);
        while (!frontier.empty()
            && distances[static_cast<std::size_t>(edge.secondArena)] < 0) {
            const int arena = frontier.front();
            frontier.pop();
            for (const int neighbor
                : arenaGraph[static_cast<std::size_t>(arena)]) {
                const bool isMeasuredEdge
                    = (arena == edge.firstArena
                          && neighbor == edge.secondArena)
                    || (arena == edge.secondArena
                        && neighbor == edge.firstArena);
                if (isMeasuredEdge
                    || distances[static_cast<std::size_t>(neighbor)] >= 0) {
                    continue;
                }
                distances[static_cast<std::size_t>(neighbor)]
                    = distances[static_cast<std::size_t>(arena)] + 1;
                frontier.push(neighbor);
            }
        }
        const int alternateDistance
            = distances[static_cast<std::size_t>(edge.secondArena)];
        if (alternateDistance < 3) {
            continue;
        }
        ++result.usefulCycleCount;
        if (edge.purpose == MissionEdgePurpose::Shortcut) {
            minimumShortcutSavings = std::min(minimumShortcutSavings,
                static_cast<std::size_t>(alternateDistance - 1));
        }
    }
    if (minimumShortcutSavings
        != std::numeric_limits<std::size_t>::max()) {
        result.minimumShortcutSavingsTransitions = minimumShortcutSavings;
    }

    std::vector<int> junctions;
    int startArena = EMPTY_CELL;
    int exitArena = EMPTY_CELL;
    for (std::size_t arena = 0; arena < arenaGraph.size(); ++arena) {
        const std::size_t degree = arenaGraph[arena].size();
        result.maximumDegree = std::max(result.maximumDegree, degree);
        ++result.degreeHistogram[std::min(
            degree, TopologySignature::DEGREE_BUCKETS - 1)];
        if (degree >= 2) {
            ++result.multiDoorSubstantialRoomCount;
        } else if (degree == 1
            && rooms[static_cast<std::size_t>(arenaRooms[arena])].role
                == RoomRole::Combat) {
            ++result.ordinaryCombatLeafCount;
        }
        if (degree >= 3) {
            ++result.meaningfulJunctionCount;
            junctions.push_back(static_cast<int>(arena));
        }
        const RoomRole role
            = rooms[static_cast<std::size_t>(arenaRooms[arena])].role;
        if (role == RoomRole::Start) {
            startArena = static_cast<int>(arena);
        } else if (role == RoomRole::Exit) {
            exitArena = static_cast<int>(arena);
        }
    }
    if (startArena >= 0 && exitArena >= 0) {
        const std::vector<int> distances
            = breadthFirstDistances(arenaGraph, startArena);
        const int distance = distances[static_cast<std::size_t>(exitArena)];
        result.startExitDistance
            = distance < 0 ? 0 : static_cast<std::size_t>(distance);
    }

    if (!arenaGraph.empty()) {
        std::vector<int> nearestJunction(arenaGraph.size(), -1);
        std::queue<int> frontier;
        for (const int junction : junctions) {
            nearestJunction[static_cast<std::size_t>(junction)] = 0;
            frontier.push(junction);
        }
        if (frontier.empty()) {
            const int root = startArena >= 0 ? startArena : 0;
            nearestJunction[static_cast<std::size_t>(root)] = 0;
            frontier.push(root);
        }
        while (!frontier.empty()) {
            const int room = frontier.front();
            frontier.pop();
            for (const int neighbor : arenaGraph[static_cast<std::size_t>(room)]) {
                if (nearestJunction[static_cast<std::size_t>(neighbor)] >= 0) {
                    continue;
                }
                nearestJunction[static_cast<std::size_t>(neighbor)]
                    = nearestJunction[static_cast<std::size_t>(room)] + 1;
                frontier.push(neighbor);
            }
        }
        result.maximumBranchDepth = static_cast<std::size_t>(std::max(
            0, *std::ranges::max_element(nearestJunction)));
    }

    // Measure the longest shortest chain whose room types alternate. This makes
    // repeated arena/passage/arena sequences visible without an exponential
    // longest-simple-path search.
    for (std::size_t source = 0; source < roomGraph.size(); ++source) {
        std::vector<int> distances(roomGraph.size(), -1);
        std::queue<int> frontier;
        distances[source] = 0;
        frontier.push(static_cast<int>(source));
        while (!frontier.empty()) {
            const int room = frontier.front();
            frontier.pop();
            const bool roomIsConnector
                = rooms[static_cast<std::size_t>(room)].role
                == RoomRole::Connector;
            for (const int neighbor : roomGraph[static_cast<std::size_t>(room)]) {
                const bool neighborIsConnector
                    = rooms[static_cast<std::size_t>(neighbor)].role
                    == RoomRole::Connector;
                if (roomIsConnector == neighborIsConnector
                    || distances[static_cast<std::size_t>(neighbor)] >= 0) {
                    continue;
                }
                distances[static_cast<std::size_t>(neighbor)]
                    = distances[static_cast<std::size_t>(room)] + 1;
                frontier.push(neighbor);
            }
        }
        result.longestAlternatingChain = std::max(
            result.longestAlternatingChain,
            static_cast<std::size_t>(std::max(
                0, *std::ranges::max_element(distances))));
    }

    return result;
}

} // namespace stalberg::rooms::detail
