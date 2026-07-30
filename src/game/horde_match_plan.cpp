#include "horde_match.hpp"

#include "generated_level_queries.hpp"
#include "match_map_metrics.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <ranges>
#include <set>

namespace {

using CellIndex = stalberg::rooms::CellIndex;

constexpr int recipeIndex(stalberg::rooms::SmallMapRecipe recipe)
{
    return static_cast<int>(recipe);
}

int scaleMapCost(int baseCost, stalberg::rooms::SmallMapRecipe recipe)
{
    const int percent = 100 + recipeIndex(recipe) * 2;
    return (baseCost * percent + 99) / 100;
}

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
            *doorway, GatePurpose::Reward,
            scaleMapCost(800, level.roomLayout().getSmallMapRecipe()), false
        });
        return;
    }
}

std::vector<std::size_t> orderedDoorwayCandidates(
    std::span<const std::size_t> path, bool fromFront)
{
    std::vector<std::size_t> result(path.begin(), path.end());
    if (!fromFront) {
        std::ranges::reverse(result);
    }
    return result;
}

std::vector<std::size_t> safeRewardDoorways(const GeneratedLevel& level,
    std::span<const std::size_t> rewardPath, int startRoom,
    int rewardRoom, int exitRoom)
{
    std::vector<std::size_t> result;
    for (auto doorway = rewardPath.rbegin(); doorway != rewardPath.rend();
         ++doorway) {
        if (roomsRemainConnectedWithoutDoorway(
                level, startRoom, exitRoom, *doorway)
            && !roomsRemainConnectedWithoutDoorway(
                level, startRoom, rewardRoom, *doorway)) {
            result.push_back(*doorway);
        }
    }
    return result;
}

bool selectStageSafeGates(const GeneratedLevel& level, SmallMapPlan& plan,
    std::span<const std::size_t> expansionPath,
    std::span<const std::size_t> anchorPath,
    std::span<const std::size_t> exitPath,
    std::span<const std::size_t> rewardPath)
{
    const std::vector<std::size_t> expansionCandidates
        = orderedDoorwayCandidates(expansionPath, true);
    const std::vector<std::size_t> anchorCandidates
        = orderedDoorwayCandidates(anchorPath, false);
    const std::vector<std::size_t> exitCandidates
        = orderedDoorwayCandidates(exitPath, false);
    const std::vector<std::size_t> rewardCandidates = safeRewardDoorways(level,
        rewardPath, plan.startRoom, plan.rewardRoom, plan.exitRoom);
    if (expansionCandidates.empty() || anchorCandidates.empty()
        || exitCandidates.empty() || rewardCandidates.empty()) {
        return false;
    }

    std::vector<MapGate> best = plan.gates;
    std::size_t bestFailureCount = std::numeric_limits<std::size_t>::max();
    for (const std::size_t expansion : expansionCandidates) {
        for (const std::size_t anchor : anchorCandidates) {
            if (anchor == expansion) {
                continue;
            }
            for (const std::size_t exit : exitCandidates) {
                if (exit == expansion || exit == anchor) {
                    continue;
                }
                for (const std::size_t reward : rewardCandidates) {
                    if (reward == expansion || reward == anchor
                        || reward == exit) {
                        continue;
                    }
                    SmallMapPlan candidate = plan;
                    candidate.gates = {
                        { expansion, GatePurpose::Expansion,
                            scaleMapCost(500, plan.recipe), false },
                        { anchor, GatePurpose::Anchor,
                            scaleMapCost(750, plan.recipe), false },
                        { exit, GatePurpose::Exit, 0, false },
                        { reward, GatePurpose::Reward,
                            scaleMapCost(800, plan.recipe), false }
                    };
                    const GateStageValidationReport validation
                        = validateMatchStages(level, candidate, 2);
                    if (validation.failures.size() < bestFailureCount) {
                        bestFailureCount = validation.failures.size();
                        best = std::move(candidate.gates);
                    }
                    if (validation.passed()) {
                        plan.gates = std::move(best);
                        return true;
                    }
                }
            }
        }
    }
    plan.gates = std::move(best);
    return false;
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
        GatePurpose::Expansion, scaleMapCost(500, plan.recipe));
    addGate(plan.gates, usedDoorways, hubToAnchor, false,
        GatePurpose::Anchor, scaleMapCost(750, plan.recipe));
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

    if (selectStageSafeGates(level, plan, startToHub, hubToAnchor,
            hubToExit, hubToReward)) {
        return plan;
    }

    for (const auto& room : level.roomLayout().getRooms()) {
        if (room.role != stalberg::rooms::RoomRole::Combat
            || room.id == plan.rewardRoom || room.id == plan.anchorRoom) {
            continue;
        }
        SmallMapPlan candidate = plan;
        candidate.anchorRoom = room.id;
        candidate.anchorCell = highestClearanceCell(level, room.id);
        candidate.anchorPosition = generated_level::worldCellCenter(
            level, candidate.anchorCell);
        const std::vector<std::size_t> candidateAnchorPath
            = doorwayPath(level, candidate.hubRoom, candidate.anchorRoom);
        if (selectStageSafeGates(level, candidate, startToHub,
                candidateAnchorPath, hubToExit, hubToReward)) {
            plan = std::move(candidate);
            return plan;
        }
    }
    return plan;
}

HordeDifficultyProfile hordeDifficultyForRound(std::uint64_t round,
    stalberg::rooms::SmallMapRecipe recipe)
{
    const std::uint64_t normalizedRound = std::max<std::uint64_t>(1, round);
    const std::uint32_t effectiveRound = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(normalizedRound, 100));
    const std::uint32_t pressureTier = std::min<std::uint32_t>(
        (effectiveRound - 1) / 5, 10);
    const std::size_t recipePressure = effectiveRound > 5
        ? static_cast<std::size_t>(recipeIndex(recipe))
        : 0;

    constexpr std::array<std::size_t, 5> openingBudgets {
        6, 10, 13, 16, 21
    };
    std::size_t spawnBudget = openingBudgets[std::min<std::size_t>(
        effectiveRound - 1, openingBudgets.size() - 1)];
    if (effectiveRound > openingBudgets.size()) {
        const std::size_t laterRound = effectiveRound - 5;
        spawnBudget = std::min<std::size_t>(48,
            openingBudgets.back() + laterRound * 2 + laterRound / 5
                + recipePressure);
    }

    HordeDifficultyProfile profile;
    profile.pressureTier = pressureTier;
    profile.spawnBudget = spawnBudget;
    profile.maximumLiving = std::min<std::size_t>(18,
        6 + (effectiveRound - 1) / 2 + pressureTier
            + (effectiveRound >= 10 ? recipePressure : 0));
    profile.buildupDuration = std::max(
        1.1F, 2.0F - static_cast<float>(pressureTier) * 0.09F);
    profile.buildupSpawnInterval = std::max(
        0.4F, 0.72F - static_cast<float>(pressureTier) * 0.032F);
    profile.peakSpawnInterval = std::max(
        0.22F, 0.42F - static_cast<float>(pressureTier) * 0.02F);
    profile.healthScale
        = 1.0F + static_cast<float>(pressureTier) * 0.08F;
    profile.movementSpeedScale
        = 1.0F + static_cast<float>(pressureTier) * 0.025F;
    profile.projectileSpeedScale
        = 1.0F + static_cast<float>(pressureTier) * 0.04F;
    profile.firingIntervalScale
        = 1.0F - static_cast<float>(pressureTier) * 0.035F;
    profile.enemyDamage = pressureTier >= 8 ? 2 : 1;
    profile.rewardPercent = 100 + static_cast<int>(pressureTier) * 5;
    profile.eliteEvent = normalizedRound >= 5 && normalizedRound % 5 == 0;
    return profile;
}

std::vector<HordeEnemyRole> hordeCompositionForRound(std::uint64_t round,
    stalberg::rooms::SmallMapRecipe recipe)
{
    const HordeDifficultyProfile profile
        = hordeDifficultyForRound(round, recipe);
    const std::uint64_t normalizedRound = std::max<std::uint64_t>(1, round);
    const std::size_t effectiveRound = static_cast<std::size_t>(
        std::min<std::uint64_t>(normalizedRound, 100));
    const std::size_t recipePressure = effectiveRound >= 6
        ? static_cast<std::size_t>(recipeIndex(recipe))
        : 0;

    std::size_t elites = profile.eliteEvent ? 1 : 0;
    elites = std::min<std::size_t>(4,
        elites + profile.pressureTier / 4);
    std::size_t casters = effectiveRound < 3 ? 0
        : std::min<std::size_t>(profile.spawnBudget / 3,
            1 + (effectiveRound - 3) / 4 + profile.pressureTier / 2
                + (recipePressure == 1 ? 1 : 0));
    std::size_t runners = effectiveRound < 2 ? 0
        : std::min<std::size_t>(profile.spawnBudget / 2,
            2 + (effectiveRound - 2) / 2 + profile.pressureTier
                + (recipePressure == 2 ? 1 : 0));
    if (elites + casters + runners > profile.spawnBudget) {
        runners = profile.spawnBudget - std::min(
            profile.spawnBudget, elites + casters);
    }
    const std::size_t drifters
        = profile.spawnBudget - elites - casters - runners;

    std::array<std::size_t, 4> remaining {
        drifters, runners, casters, elites
    };
    constexpr std::array<HordeEnemyRole, 4> roles {
        HordeEnemyRole::Drifter,
        HordeEnemyRole::Runner,
        HordeEnemyRole::Caster,
        HordeEnemyRole::Elite
    };
    const std::size_t firstRole = static_cast<std::size_t>(
        (normalizedRound % roles.size()
            + static_cast<std::uint64_t>(recipeIndex(recipe)))
        % roles.size());
    std::vector<HordeEnemyRole> result;
    result.reserve(profile.spawnBudget);
    while (result.size() < profile.spawnBudget) {
        for (std::size_t offset = 0; offset < roles.size(); ++offset) {
            const std::size_t role = (firstRole + offset) % roles.size();
            if (remaining[role] == 0) {
                continue;
            }
            result.push_back(roles[role]);
            --remaining[role];
        }
    }
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

