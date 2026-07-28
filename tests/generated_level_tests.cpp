#include "game/generated_level.hpp"
#include "game/level_session.hpp"
#include "game/player.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using CellIndex = stalberg::rooms::CellIndex;

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool nearlyEqual(float first, float second, float tolerance = 0.01F)
{
    return std::abs(first - second) <= tolerance;
}

float triangleArea(const FloorTriangle& triangle)
{
    return std::abs((triangle.second.x - triangle.first.x)
            * (triangle.third.y - triangle.first.y)
        - (triangle.second.y - triangle.first.y)
            * (triangle.third.x - triangle.first.x))
        * 0.5F;
}

bool samePoint(Vector2 first, Vector2 second)
{
    return nearlyEqual(first.x, second.x, 0.001F)
        && nearlyEqual(first.y, second.y, 0.001F);
}

bool hasWall(std::span<const Segment2D> walls, Vector2 first, Vector2 second)
{
    for (const Segment2D& wall : walls) {
        if ((samePoint(wall.start, first) && samePoint(wall.end, second))
            || (samePoint(wall.start, second) && samePoint(wall.end, first))) {
            return true;
        }
    }
    return false;
}

std::pair<CellIndex, CellIndex> orderedPair(CellIndex first, CellIndex second)
{
    return std::minmax(first, second);
}

bool packageRetainsAlignedGeneratorArtifacts(const GeneratedLevel& level)
{
    const std::size_t cells = level.grid().getVertexCount();
    return check(level.grid().isFullyRelaxed(),
               "runtime package retains a fully relaxed source grid")
        && check(level.dualGrid().cells.size() == cells,
            "runtime package retains one dual polygon per source cell")
        && check(level.roomGrid().getCellCount() == cells,
            "runtime package retains the aligned neutral room graph")
        && check(level.roomLayout().getCellAssignments().size() == cells
                && level.roomLayout().getRoomCount() > 0,
            "runtime package retains a valid aligned room layout");
}

bool floorsTriangulateExactlyAssignedPolygons(const GeneratedLevel& level)
{
    const auto assignments = level.roomLayout().getCellAssignments();
    std::size_t expectedTriangles = 0;
    float expectedArea = 0.0F;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == stalberg::rooms::EMPTY_CELL) {
            continue;
        }
        expectedTriangles += level.dualGrid().cells[cell].polygon.size();
        expectedArea += level.dualGrid().cells[cell].area
            * level.worldScale() * level.worldScale();
    }

    float measuredArea = 0.0F;
    bool assignmentsMatch = true;
    for (const FloorTriangle& triangle : level.floorTriangles()) {
        measuredArea += triangleArea(triangle);
        assignmentsMatch &= triangle.cell < assignments.size()
            && triangle.region == assignments[triangle.cell]
            && triangle.region != stalberg::rooms::EMPTY_CELL;
    }

    return check(level.floorTriangles().size() == expectedTriangles,
               "every assigned dual polygon contributes its complete center fan")
        && check(assignmentsMatch,
            "floor triangles never include unassigned cells or wrong regions")
        && check(nearlyEqual(measuredArea, expectedArea,
                     std::max(0.05F, expectedArea * 0.0001F)),
            "floor triangle area matches exact assigned dual-cell area");
}

bool navigationOpensOnlyPublishedDoorways(const GeneratedLevel& level)
{
    const auto assignments = level.roomLayout().getCellAssignments();
    std::set<std::pair<CellIndex, CellIndex>> doors;
    bool valid = true;
    for (const stalberg::rooms::Doorway& door
        : level.roomLayout().getDoorways()) {
        doors.insert(orderedPair(door.firstCell, door.secondCell));
        valid &= check(door.width * level.worldScale() > 2.0F * PLAYER_RADIUS,
            "every published doorway is physically wider than the player");
    }
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        for (const CellIndex neighbor : level.roomGrid().neighbors[cell]) {
            if (cell >= neighbor || assignments[cell] == stalberg::rooms::EMPTY_CELL
                || assignments[neighbor] == stalberg::rooms::EMPTY_CELL) {
                continue;
            }
            const bool expected = assignments[cell] == assignments[neighbor]
                || doors.contains(orderedPair(cell, neighbor));
            valid &= check(level.canTraverse(cell, neighbor) == expected
                    && level.canTraverse(neighbor, cell) == expected,
                "navigation uses room interiors and exact published doorway pairs");
        }
    }
    return valid;
}

bool wallsCloseEveryUnauthorizedDualBoundary(const GeneratedLevel& level)
{
    const auto assignments = level.roomLayout().getCellAssignments();
    std::set<std::pair<CellIndex, CellIndex>> doors;
    for (const stalberg::rooms::Doorway& door
        : level.roomLayout().getDoorways()) {
        doors.insert(orderedPair(door.firstCell, door.secondCell));
    }

    bool valid = true;
    for (const stalberg::DualConnection& connection
        : level.dualGrid().connections) {
        const CellIndex firstCell = connection.cells.a;
        const CellIndex secondCell = connection.cells.b;
        const bool firstFloor
            = assignments[firstCell] != stalberg::rooms::EMPTY_CELL;
        const bool secondFloor
            = assignments[secondCell] != stalberg::rooms::EMPTY_CELL;
        if (!firstFloor && !secondFloor) {
            continue;
        }

        const bool open = firstFloor && secondFloor
            && (assignments[firstCell] == assignments[secondCell]
                || doors.contains(orderedPair(firstCell, secondCell)));
        const Vector2 first {
            connection.first.x * level.worldScale(),
            connection.first.y * level.worldScale()
        };
        const Vector2 second {
            connection.second.x * level.worldScale(),
            connection.second.y * level.worldScale()
        };
        valid &= check(hasWall(level.walls(), first, second) == !open,
            "wall geometry closes contacts except exact room interiors and doors");
    }
    return valid;
}

bool sessionOwnsLifecycleAndDynamicDoorWalls(const GeneratedLevel& level)
{
    LevelSession session(level);
    const auto thresholds = level.doorwayThresholds();
    bool valid = check(thresholds.size()
            == level.roomLayout().getDoorways().size(),
        "runtime geometry retains one threshold per published doorway");
    valid &= check(session.roomStates().size()
            == level.roomLayout().getRoomCount(),
        "mutable session owns one lifecycle state per generated room");
    valid &= check(session.currentRoom().has_value()
            && *session.currentRoom()
                == level.roomLayout().getCellAssignment(level.playerSpawnCell()),
        "session starts in the generated Start room");
    if (thresholds.empty()) {
        return check(false, "generated level provides a doorway to lock");
    }

    const DoorwayThreshold& threshold = thresholds.front();
    valid &= check(!hasWall(session.activeWalls(),
                       threshold.segment.start, threshold.segment.end)
            && session.canTraverse(
                level, threshold.firstCell, threshold.secondCell),
        "published doorway starts open for collision and traversal");
    valid &= check(lockRoom(session, level, threshold.firstRegion),
        "a generated room can be locked");
    valid &= check(session.doorwayIsLocked(threshold.doorway)
            && hasWall(session.activeWalls(),
                threshold.segment.start, threshold.segment.end)
            && !session.canTraverse(
                level, threshold.firstCell, threshold.secondCell),
        "locking a room closes its published doorway geometry and navigation");
    valid &= check(setRoomLifecycleState(session,
                       threshold.firstRegion, RoomLifecycleState::fighting)
            && session.lockedRoom() == threshold.firstRegion,
        "room lifecycle can advance while its physical doors remain locked");
    valid &= check(unlockRoom(session, level, threshold.firstRegion,
                       RoomLifecycleState::cleared),
        "a cleared room can reopen its doors");
    valid &= check(!session.doorwayIsLocked(threshold.doorway)
            && !hasWall(session.activeWalls(),
                threshold.segment.start, threshold.segment.end)
            && session.canTraverse(
                level, threshold.firstCell, threshold.secondCell),
        "unlocking removes threshold collision and restores traversal");

    PlayerInput resetInput;
    resetInput.restartPressed = true;
    const LevelSessionStepResult reset = updateLevelSession(
        session, level, resetInput, 1.0F / 120.0F);
    valid &= check(reset.reset && !session.lockedRoom().has_value()
            && session.activeWalls().size() == level.walls().size(),
        "session reset restores lifecycle and open-door collision state");
    return valid;
}

bool spawnAndNavigationReachAllFloor(const GeneratedLevel& level)
{
    const CellIndex spawn = level.playerSpawnCell();
    const int spawnRegion = level.roomLayout().getCellAssignment(spawn);
    bool startRole = false;
    for (const stalberg::rooms::GeneratedRoom& room
        : level.roomLayout().getRooms()) {
        if (room.id == spawnRegion) {
            startRole = room.role == stalberg::rooms::RoomRole::Start;
            break;
        }
    }

    const auto spawnAtPoint = level.cellAtWorldPoint(level.playerSpawn());
    bool valid = check(startRole,
        "player spawn belongs to the generated Start room");
    valid &= check(spawnAtPoint.has_value() && *spawnAtPoint == spawn,
        "player spawn lies inside its exact floor polygon");
    valid &= check(level.dualGrid().cells[spawn].clearance * level.worldScale()
            > PLAYER_RADIUS,
        "player spawn has enough local clearance for the player circle");

    std::vector<bool> visited(level.roomGrid().getCellCount(), false);
    std::queue<CellIndex> frontier;
    visited[spawn] = true;
    frontier.push(spawn);
    while (!frontier.empty()) {
        const CellIndex cell = frontier.front();
        frontier.pop();
        for (const CellIndex neighbor : level.traversableNeighbors(cell)) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                frontier.push(neighbor);
            }
        }
    }

    const auto assignments = level.roomLayout().getCellAssignments();
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] != stalberg::rooms::EMPTY_CELL) {
            valid &= check(visited[cell],
                "door-aware navigation reaches every generated floor cell");
        }
    }
    return valid;
}

} // namespace

int main()
{
    const GeneratedLevel level;
    bool valid = true;
    valid &= packageRetainsAlignedGeneratorArtifacts(level);
    valid &= floorsTriangulateExactlyAssignedPolygons(level);
    valid &= navigationOpensOnlyPublishedDoorways(level);
    valid &= wallsCloseEveryUnauthorizedDualBoundary(level);
    valid &= sessionOwnsLifecycleAndDynamicDoorWalls(level);
    valid &= spawnAndNavigationReachAllFloor(level);
    return valid ? 0 : 1;
}
