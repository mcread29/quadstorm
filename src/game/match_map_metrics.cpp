#include "match_map_metrics.hpp"

#include "collision_2d.hpp"
#include "enemy.hpp"
#include "horde_match.hpp"
#include "player.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <ranges>
#include <utility>
#include <vector>

namespace {

using CellIndex = stalberg::rooms::CellIndex;

constexpr float playerDiameter()
{
    return PLAYER_RADIUS * 2.0F;
}

float triangleArea(const FloorTriangle& triangle)
{
    const float firstX = triangle.second.x - triangle.first.x;
    const float firstY = triangle.second.y - triangle.first.y;
    const float secondX = triangle.third.x - triangle.first.x;
    const float secondY = triangle.third.y - triangle.first.y;
    return std::abs(firstX * secondY - firstY * secondX) * 0.5F;
}

std::vector<float> roomAreas(const GeneratedLevel& level)
{
    std::vector<float> result(level.roomLayout().getRoomCount(), 0.0F);
    for (const FloorTriangle& triangle : level.floorTriangles()) {
        if (triangle.region >= 0
            && static_cast<std::size_t>(triangle.region) < result.size()) {
            result[static_cast<std::size_t>(triangle.region)]
                += triangleArea(triangle);
        }
    }
    return result;
}

float distanceBetween(const GeneratedLevel& level,
    CellIndex first, CellIndex second)
{
    const stalberg::Point firstPoint = level.dualGrid().cells[first].center;
    const stalberg::Point secondPoint = level.dualGrid().cells[second].center;
    const float x = secondPoint.x - firstPoint.x;
    const float y = secondPoint.y - firstPoint.y;
    return std::sqrt(x * x + y * y) * level.worldScale();
}

float routeDistance(const GeneratedLevel& level,
    CellIndex start, CellIndex destination)
{
    const std::size_t cellCount = level.roomGrid().getCellCount();
    if (start >= cellCount || destination >= cellCount) {
        return 0.0F;
    }

    using QueueEntry = std::pair<float, CellIndex>;
    std::priority_queue<QueueEntry,
        std::vector<QueueEntry>, std::greater<>> frontier;
    std::vector<float> distances(
        cellCount, std::numeric_limits<float>::infinity());
    distances[start] = 0.0F;
    frontier.emplace(0.0F, start);
    while (!frontier.empty()) {
        const auto [distance, cell] = frontier.top();
        frontier.pop();
        if (distance != distances[cell]) {
            continue;
        }
        if (cell == destination) {
            return distance;
        }
        for (const CellIndex neighbor : level.traversableNeighbors(cell)) {
            const float candidate = distance
                + distanceBetween(level, cell, neighbor);
            if (candidate < distances[neighbor]) {
                distances[neighbor] = candidate;
                frontier.emplace(candidate, neighbor);
            }
        }
    }
    return 0.0F;
}

float roomSpan(const GeneratedLevel& level, int room)
{
    const auto assignments = level.roomLayout().getCellAssignments();
    std::vector<CellIndex> cells;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == room) {
            cells.push_back(cell);
        }
    }
    float result = 0.0F;
    for (std::size_t first = 0; first < cells.size(); ++first) {
        for (std::size_t second = first + 1; second < cells.size(); ++second) {
            result = std::max(result,
                distanceBetween(level, cells[first], cells[second]));
        }
    }
    return result;
}

float objectiveClearance(const GeneratedLevel& level, CellIndex cell)
{
    if (cell >= level.dualGrid().cells.size()) {
        return 0.0F;
    }
    return level.dualGrid().cells[cell].clearance * level.worldScale();
}

std::vector<bool> reachableFloor(const GeneratedLevel& level)
{
    std::vector<bool> reached(level.roomGrid().getCellCount(), false);
    if (level.playerSpawnCell() >= reached.size()) {
        return reached;
    }
    std::queue<CellIndex> frontier;
    reached[level.playerSpawnCell()] = true;
    frontier.push(level.playerSpawnCell());
    while (!frontier.empty()) {
        const CellIndex cell = frontier.front();
        frontier.pop();
        for (const CellIndex neighbor : level.traversableNeighbors(cell)) {
            if (!reached[neighbor]) {
                reached[neighbor] = true;
                frontier.push(neighbor);
            }
        }
    }
    return reached;
}

bool isStaticSpawnCandidateUsable(
    const GeneratedLevel& level, CellIndex cell)
{
    if (cell >= level.dualGrid().cells.size()
        || level.dualGrid().cells[cell].clearance * level.worldScale()
            < ENEMY_RADIUS + 0.08F) {
        return false;
    }
    const stalberg::Point center = level.dualGrid().cells[cell].center;
    const Vector2 position {
        center.x * level.worldScale(), center.y * level.worldScale()
    };
    return std::ranges::none_of(level.walls(), [&](const Segment2D& wall) {
        const Vector2 closest = closestPointOnSegment(position, wall).position;
        const float x = closest.x - position.x;
        const float y = closest.y - position.y;
        constexpr float clearance = ENEMY_RADIUS + 0.04F;
        return x * x + y * y < clearance * clearance;
    });
}

} // namespace

float MatchMapMetrics::minimumDoorwayWidthInPlayerDiameters() const
{
    return minimumDoorwayWidth / playerDiameter();
}

float MatchMapMetrics::minimumSubstantialRoomAreaInPlayerDiameterSquares() const
{
    return minimumSubstantialRoomArea / (playerDiameter() * playerDiameter());
}

float MatchMapMetrics::anchorRoomAreaInPlayerDiameterSquares() const
{
    return anchorRoomArea / (playerDiameter() * playerDiameter());
}

float MatchMapMetrics::minimumObjectiveClearanceInPlayerDiameters() const
{
    return minimumObjectiveClearance / playerDiameter();
}

float MatchMapMetrics::anchorRoomSpanInPlayerDiameters() const
{
    return anchorRoomSpan / playerDiameter();
}

float MatchMapMetrics::startToExitRouteDistanceInPlayerDiameters() const
{
    return startToExitRouteDistance / playerDiameter();
}

float MatchMapMetrics::maximumUsableIngressSeparationInPlayerDiameters() const
{
    return maximumUsableIngressSeparation / playerDiameter();
}

MatchMapMetrics measureMatchMap(const GeneratedLevel& level)
{
    MatchMapMetrics result;
    result.minimumDoorwayWidth = std::numeric_limits<float>::infinity();
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        const float x = threshold.segment.end.x - threshold.segment.start.x;
        const float y = threshold.segment.end.y - threshold.segment.start.y;
        result.minimumDoorwayWidth = std::min(
            result.minimumDoorwayWidth, std::sqrt(x * x + y * y));
    }
    if (!std::isfinite(result.minimumDoorwayWidth)) {
        result.minimumDoorwayWidth = 0.0F;
    }

    const std::vector<float> areas = roomAreas(level);
    result.minimumSubstantialRoomArea = std::numeric_limits<float>::infinity();
    for (const auto& room : level.roomLayout().getRooms()) {
        if (room.role != stalberg::rooms::RoomRole::Connector) {
            result.minimumSubstantialRoomArea = std::min(
                result.minimumSubstantialRoomArea,
                areas[static_cast<std::size_t>(room.id)]);
        }
    }
    if (!std::isfinite(result.minimumSubstantialRoomArea)) {
        result.minimumSubstantialRoomArea = 0.0F;
    }

    const SmallMapPlan plan = buildSmallMapPlan(level);
    if (plan.anchorRoom >= 0
        && static_cast<std::size_t>(plan.anchorRoom) < areas.size()) {
        result.anchorRoomArea = areas[static_cast<std::size_t>(plan.anchorRoom)];
    }
    result.minimumObjectiveClearance = std::min({
        objectiveClearance(level, plan.hubCell),
        objectiveClearance(level, plan.anchorCell),
        objectiveClearance(level, plan.exitCell)
    });
    result.anchorRoomSpan = roomSpan(level, plan.anchorRoom);
    result.startToExitRouteDistance = routeDistance(
        level, level.playerSpawnCell(), plan.exitCell);

    struct UsableIngress {
        CellIndex cell = 0;
        int room = stalberg::rooms::EMPTY_CELL;
    };
    const std::vector<bool> reachable = reachableFloor(level);
    std::vector<UsableIngress> ingress;
    for (const auto& room : level.roomLayout().getRooms()) {
        const std::size_t before = ingress.size();
        for (const CellIndex cell : room.enemySpawnCandidates) {
            if (cell < reachable.size() && reachable[cell]
                && isStaticSpawnCandidateUsable(level, cell)) {
                ingress.push_back(UsableIngress { cell, room.id });
            }
        }
        if (ingress.size() > before) {
            ++result.usableEnemySpawnRoomCount;
        }
    }
    std::ranges::sort(ingress, {}, &UsableIngress::cell);
    ingress.erase(std::unique(ingress.begin(), ingress.end(),
        [](const UsableIngress& first, const UsableIngress& second) {
            return first.cell == second.cell;
        }), ingress.end());
    result.usableEnemySpawnCandidateCount = ingress.size();
    for (std::size_t first = 0; first < ingress.size(); ++first) {
        for (std::size_t second = first + 1;
             second < ingress.size(); ++second) {
            if (ingress[first].room == ingress[second].room) {
                continue;
            }
            result.maximumUsableIngressSeparation = std::max(
                result.maximumUsableIngressSeparation,
                distanceBetween(level, ingress[first].cell,
                    ingress[second].cell));
        }
    }
    for (const auto& doorway : level.roomLayout().getDoorways()) {
        if (doorway.firstRegion == plan.hubRoom
            || doorway.secondRegion == plan.hubRoom) {
            ++result.hubDoorwayDegree;
        }
    }
    return result;
}
