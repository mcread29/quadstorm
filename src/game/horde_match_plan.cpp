#include "horde_match.hpp"

#include "generated_level_queries.hpp"

#include <algorithm>
#include <queue>
#include <ranges>
#include <set>

namespace {

using CellIndex = stalberg::rooms::CellIndex;

CellIndex highestClearanceCell(const GeneratedLevel& level, int room)
{
    const auto assignments = level.roomLayout().getCellAssignments();
    CellIndex selected = assignments.size();
    float selectedClearance = -1.0F;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] != room) {
            continue;
        }
        const float clearance = level.dualGrid().cells[cell].clearance;
        if (clearance > selectedClearance
            || (clearance == selectedClearance && cell < selected)) {
            selected = cell;
            selectedClearance = clearance;
        }
    }
    return selected == assignments.size() ? level.playerSpawnCell() : selected;
}

struct RoomGraphEdge {
    int room = stalberg::rooms::EMPTY_CELL;
    std::size_t doorway = 0;
};

std::vector<std::size_t> doorwayPath(
    const GeneratedLevel& level, int start, int destination)
{
    const std::size_t roomCount = level.roomLayout().getRoomCount();
    if (start < 0 || destination < 0
        || static_cast<std::size_t>(start) >= roomCount
        || static_cast<std::size_t>(destination) >= roomCount) {
        return {};
    }

    std::vector<std::vector<RoomGraphEdge>> graph(roomCount);
    const auto doorways = level.roomLayout().getDoorways();
    for (std::size_t index = 0; index < doorways.size(); ++index) {
        const auto& doorway = doorways[index];
        graph[static_cast<std::size_t>(doorway.firstRegion)].push_back(
            RoomGraphEdge { doorway.secondRegion, index });
        graph[static_cast<std::size_t>(doorway.secondRegion)].push_back(
            RoomGraphEdge { doorway.firstRegion, index });
    }
    for (auto& edges : graph) {
        std::ranges::sort(edges, {}, &RoomGraphEdge::room);
    }

    std::vector<int> parent(roomCount, stalberg::rooms::EMPTY_CELL);
    std::vector<std::size_t> parentDoor(roomCount, doorways.size());
    std::queue<int> frontier;
    parent[static_cast<std::size_t>(start)] = start;
    frontier.push(start);
    while (!frontier.empty() && parent[static_cast<std::size_t>(destination)] < 0) {
        const int room = frontier.front();
        frontier.pop();
        for (const RoomGraphEdge edge : graph[static_cast<std::size_t>(room)]) {
            if (parent[static_cast<std::size_t>(edge.room)] >= 0) {
                continue;
            }
            parent[static_cast<std::size_t>(edge.room)] = room;
            parentDoor[static_cast<std::size_t>(edge.room)] = edge.doorway;
            frontier.push(edge.room);
        }
    }
    if (parent[static_cast<std::size_t>(destination)] < 0) {
        return {};
    }

    std::vector<std::size_t> result;
    for (int room = destination; room != start;
         room = parent[static_cast<std::size_t>(room)]) {
        result.push_back(parentDoor[static_cast<std::size_t>(room)]);
    }
    std::ranges::reverse(result);
    return result;
}

bool roomsRemainConnectedWithoutDoorway(const GeneratedLevel& level,
    int start, int destination, std::size_t blockedDoorway)
{
    const std::size_t roomCount = level.roomLayout().getRoomCount();
    if (start < 0 || destination < 0
        || static_cast<std::size_t>(start) >= roomCount
        || static_cast<std::size_t>(destination) >= roomCount) {
        return false;
    }
    std::vector<bool> reached(roomCount, false);
    std::queue<int> frontier;
    reached[static_cast<std::size_t>(start)] = true;
    frontier.push(start);
    const auto doorways = level.roomLayout().getDoorways();
    while (!frontier.empty()) {
        const int room = frontier.front();
        frontier.pop();
        for (std::size_t doorway = 0; doorway < doorways.size(); ++doorway) {
            if (doorway == blockedDoorway) {
                continue;
            }
            const auto& edge = doorways[doorway];
            int neighbor = stalberg::rooms::EMPTY_CELL;
            if (edge.firstRegion == room) {
                neighbor = edge.secondRegion;
            } else if (edge.secondRegion == room) {
                neighbor = edge.firstRegion;
            }
            if (neighbor < 0 || reached[static_cast<std::size_t>(neighbor)]) {
                continue;
            }
            reached[static_cast<std::size_t>(neighbor)] = true;
            frontier.push(neighbor);
        }
    }
    return reached[static_cast<std::size_t>(destination)];
}

void addGate(std::vector<MapGate>& gates, std::set<std::size_t>& used,
    std::span<const std::size_t> path, bool fromFront,
    GatePurpose purpose, int cost)
{
    if (path.empty()) {
        return;
    }
    if (fromFront) {
        for (const std::size_t doorway : path) {
            if (used.insert(doorway).second) {
                gates.push_back(MapGate { doorway, purpose, cost, false });
                return;
            }
        }
    } else {
        for (auto doorway = path.rbegin(); doorway != path.rend(); ++doorway) {
            if (used.insert(*doorway).second) {
                gates.push_back(MapGate { *doorway, purpose, cost, false });
                return;
            }
        }
    }
}

void addSafeRewardGate(const GeneratedLevel& level,
    std::vector<MapGate>& gates, std::set<std::size_t>& used,
    std::span<const std::size_t> rewardPath, int startRoom,
    int rewardRoom, int exitRoom)
{
    for (auto doorway = rewardPath.rbegin(); doorway != rewardPath.rend();
         ++doorway) {
        if (used.contains(*doorway)
            || !roomsRemainConnectedWithoutDoorway(
                level, startRoom, exitRoom, *doorway)
            || roomsRemainConnectedWithoutDoorway(
                level, startRoom, rewardRoom, *doorway)) {
            continue;
        }
        used.insert(*doorway);
        gates.push_back(MapGate {
            *doorway, GatePurpose::Reward, 800, false
        });
        return;
    }
}

} // namespace

SmallMapPlan buildSmallMapPlan(const GeneratedLevel& level)
{
    SmallMapPlan plan;
    plan.recipe = level.roomLayout().getSmallMapRecipe();
    const auto* start = generated_level::findRoomByRole(
        level, stalberg::rooms::RoomRole::Start);
    const auto* hub = generated_level::findRoomByRole(
        level, stalberg::rooms::RoomRole::Hub);
    const auto* reward = generated_level::findRoomByRole(
        level, stalberg::rooms::RoomRole::Reward);
    const auto* exit = generated_level::findRoomByRole(
        level, stalberg::rooms::RoomRole::Exit);
    plan.startRoom = start == nullptr ? stalberg::rooms::EMPTY_CELL : start->id;
    plan.hubRoom = hub == nullptr ? plan.startRoom : hub->id;
    plan.rewardRoom = reward == nullptr ? stalberg::rooms::EMPTY_CELL : reward->id;
    plan.exitRoom = exit == nullptr ? stalberg::rooms::EMPTY_CELL : exit->id;

    for (const auto& room : level.roomLayout().getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Combat
            && room.id != plan.rewardRoom) {
            plan.anchorRoom = room.id;
            break;
        }
    }
    if (plan.anchorRoom == stalberg::rooms::EMPTY_CELL) {
        plan.anchorRoom = plan.hubRoom;
    }

    plan.hubCell = highestClearanceCell(level, plan.hubRoom);
    plan.anchorCell = highestClearanceCell(level, plan.anchorRoom);
    plan.exitCell = highestClearanceCell(level, plan.exitRoom);
    plan.hubPosition = generated_level::worldCellCenter(level, plan.hubCell);
    plan.anchorPosition
        = generated_level::worldCellCenter(level, plan.anchorCell);
    plan.exitPosition = generated_level::worldCellCenter(level, plan.exitCell);

    std::set<std::size_t> usedDoorways;
    const std::vector<std::size_t> startToHub
        = doorwayPath(level, plan.startRoom, plan.hubRoom);
    const std::vector<std::size_t> hubToAnchor
        = doorwayPath(level, plan.hubRoom, plan.anchorRoom);
    const std::vector<std::size_t> hubToReward
        = doorwayPath(level, plan.hubRoom, plan.rewardRoom);
    const std::vector<std::size_t> hubToExit
        = doorwayPath(level, plan.hubRoom, plan.exitRoom);
    addGate(plan.gates, usedDoorways, startToHub, true,
        GatePurpose::Expansion, 500);
    addGate(plan.gates, usedDoorways, hubToAnchor, false,
        GatePurpose::Anchor, 750);
    addGate(plan.gates, usedDoorways, hubToExit, false,
        GatePurpose::Exit, 0);
    addSafeRewardGate(level, plan.gates, usedDoorways, hubToReward,
        plan.startRoom, plan.rewardRoom, plan.exitRoom);

    if (reward != nullptr) {
        std::vector<CellIndex> relayCells(
            reward->coverCandidates.begin(), reward->coverCandidates.end());
        if (relayCells.size() < 3) {
            const auto assignments = level.roomLayout().getCellAssignments();
            for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
                if (assignments[cell] == reward->id) {
                    relayCells.push_back(cell);
                }
            }
        }
        std::ranges::sort(relayCells);
        relayCells.erase(std::unique(relayCells.begin(), relayCells.end()),
            relayCells.end());
        const std::size_t targetCount
            = std::min<std::size_t>(3, relayCells.size());
        for (std::size_t target = 0; target < targetCount; ++target) {
            const std::size_t index = targetCount == 1
                ? 0
                : target * (relayCells.size() - 1) / (targetCount - 1);
            const CellIndex cell = relayCells[index];
            plan.relayTargets.push_back(RelayTarget {
                cell, generated_level::worldCellCenter(level, cell)
            });
        }
    }
    return plan;
}

std::vector<HordeEnemyRole> hordeCompositionForRound(int round)
{
    int drifters = 0;
    int runners = 0;
    int casters = 0;
    int elites = 0;
    switch (round) {
    case 1:
        drifters = 6;
        break;
    case 2:
        drifters = 8;
        runners = 2;
        break;
    case 3:
        drifters = 10;
        runners = 2;
        casters = 1;
        break;
    case 4:
        drifters = 12;
        runners = 3;
        casters = 1;
        break;
    default:
        drifters = 14;
        runners = 4;
        casters = 2;
        elites = 1;
        break;
    }
    std::vector<HordeEnemyRole> result;
    result.insert(result.end(), drifters, HordeEnemyRole::Drifter);
    result.insert(result.end(), runners, HordeEnemyRole::Runner);
    result.insert(result.end(), casters, HordeEnemyRole::Caster);
    result.insert(result.end(), elites, HordeEnemyRole::Elite);
    return result;
}

std::optional<CellIndex> nextHordeNavigationCell(
    const GeneratedLevel& level, const LevelSession& session,
    CellIndex start, CellIndex destination)
{
    const std::size_t cellCount = level.roomGrid().getCellCount();
    if (start >= cellCount || destination >= cellCount) {
        return std::nullopt;
    }
    if (start == destination) {
        return start;
    }
    std::vector<CellIndex> parent(cellCount, cellCount);
    std::queue<CellIndex> frontier;
    parent[start] = start;
    frontier.push(start);
    while (!frontier.empty() && parent[destination] == cellCount) {
        const CellIndex cell = frontier.front();
        frontier.pop();
        for (const CellIndex neighbor : level.traversableNeighbors(cell)) {
            if (parent[neighbor] != cellCount
                || !session.canTraverse(cell, neighbor)) {
                continue;
            }
            parent[neighbor] = cell;
            frontier.push(neighbor);
        }
    }
    if (parent[destination] == cellCount) {
        return std::nullopt;
    }
    CellIndex step = destination;
    while (parent[step] != start) {
        step = parent[step];
    }
    return step;
}

