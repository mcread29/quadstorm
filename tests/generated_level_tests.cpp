#include "game/generated_encounter.hpp"
#include "geometry/quantized_geometry.hpp"
#include "game/generated_level.hpp"
#include "game/level_session.hpp"
#include "game/player.hpp"

#include <algorithm>
#include <array>
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
        for (const CellIndex neighbor : level.roomGrid().neighbors(cell)) {
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

bool wallsHaveStableCanonicalOrder(const GeneratedLevel& level)
{
    std::vector<stalberg::geometry::QuantizedEdge> keys;
    keys.reserve(level.walls().size());
    const double sourceScale
        = stalberg::geometry::DEFAULT_KEY_SCALE / level.worldScale();
    for (const Segment2D& wall : level.walls()) {
        keys.emplace_back(
            stalberg::geometry::quantizePoint(
                wall.start.x, wall.start.y, sourceScale),
            stalberg::geometry::quantizePoint(
                wall.end.x, wall.end.y, sourceScale));
    }
    return check(std::ranges::is_sorted(keys),
        "runtime walls are published in canonical endpoint order");
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

    const int currentRoom = *session.currentRoom();
    const auto thresholdForCurrentRoom = std::ranges::find_if(
        thresholds, [currentRoom](const DoorwayThreshold& candidate) {
            return candidate.firstRegion == currentRoom
                || candidate.secondRegion == currentRoom;
        });
    if (thresholdForCurrentRoom == thresholds.end()) {
        return check(false, "generated Start room provides a doorway to lock");
    }
    const DoorwayThreshold& threshold = *thresholdForCurrentRoom;
    valid &= check(!hasWall(session.activeWalls(),
                       threshold.segment.start, threshold.segment.end)
            && session.canTraverse(threshold.firstCell, threshold.secondCell),
        "published doorway starts open for collision and traversal");
    const std::size_t openWallCount = session.activeWalls().size();
    const int otherRoom = (currentRoom + 1)
        % static_cast<int>(session.roomStates().size());
    valid &= check(!session.clearEncounter(currentRoom)
            && !session.beginEncounter(otherRoom)
            && session.activeWalls().size() == openWallCount
            && !session.lockedRoom().has_value(),
        "invalid encounter transitions fail without mutating session invariants");
    valid &= check(session.beginEncounter(currentRoom),
        "an entered generated room can begin an encounter atomically");
    valid &= check(session.doorwayIsLocked(threshold.doorway)
            && hasWall(session.activeWalls(),
                threshold.segment.start, threshold.segment.end)
            && !session.canTraverse(threshold.firstCell, threshold.secondCell)
            && session.roomStates()[static_cast<std::size_t>(currentRoom)]
                == RoomLifecycleState::fighting,
        "beginning an encounter synchronizes lifecycle, collision, and navigation");
    const std::size_t lockedWallCount = session.activeWalls().size();
    valid &= check(!session.beginEncounter(currentRoom)
            && session.activeWalls().size() == lockedWallCount,
        "a fighting room cannot be begun again or duplicate locked walls");
    valid &= check(session.clearEncounter(currentRoom),
        "clearing an encounter reopens its doors atomically");
    valid &= check(!session.doorwayIsLocked(threshold.doorway)
            && !hasWall(session.activeWalls(),
                threshold.segment.start, threshold.segment.end)
            && session.canTraverse(threshold.firstCell, threshold.secondCell),
        "clearing removes threshold collision and restores traversal");
    valid &= check(!session.clearEncounter(currentRoom)
            && session.activeWalls().size() == openWallCount,
        "a cleared encounter cannot be cleared twice");

    session.player().invulnerabilityRemaining = 0.5F;
    session.player().hitFlashRemaining = 0.4F;
    session.update(PlayerInput {}, 0.1F);
    valid &= check(nearlyEqual(
                       session.player().invulnerabilityRemaining, 0.4F)
            && nearlyEqual(session.player().hitFlashRemaining, 0.3F),
        "player combat effect timers continue during generated traversal");

    PlayerInput resetInput;
    resetInput.restartPressed = true;
    const LevelSessionStepResult reset
        = session.update(resetInput, 1.0F / 120.0F);
    valid &= check(reset.reset && !session.lockedRoom().has_value()
            && session.activeWalls().size() == level.walls().size(),
        "session reset restores lifecycle and open-door collision state");
    return valid;
}

bool generatedTraversalAllowsPlayerFire(const GeneratedLevel& level)
{
    LevelSession session(level);
    GeneratedEncounterCoordinator coordinator;
    const Vector2 playerPosition {
        session.player().position.x,
        session.player().position.z
    };
    constexpr std::array directions {
        Vector2 { 1.0F, 0.0F },
        Vector2 { -1.0F, 0.0F },
        Vector2 { 0.0F, 1.0F },
        Vector2 { 0.0F, -1.0F }
    };

    PlayerInput input;
    input.fireHeld = true;
    bool foundOpenMuzzle = false;
    for (const Vector2 direction : directions) {
        const Vector2 muzzle {
            playerPosition.x + direction.x * WEAPON_MUZZLE_DISTANCE,
            playerPosition.y + direction.y * WEAPON_MUZZLE_DISTANCE
        };
        if (earliestCircleSegmentHit(playerPosition, muzzle,
                coordinator.playerProjectiles().profile().radius,
                session.activeWalls())
                .has_value()) {
            continue;
        }
        input.hasAimPoint = true;
        input.aimPoint = Vector3 {
            playerPosition.x + direction.x * 10.0F,
            0.0F,
            playerPosition.y + direction.y * 10.0F
        };
        foundOpenMuzzle = true;
        break;
    }

    bool valid = check(foundOpenMuzzle,
        "generated Start spawn has an open firing direction");
    const GeneratedEncounterStepResult fired = updateGeneratedEncounter(
        coordinator, session, level, input, 1.0F / 120.0F);
    valid &= check(!fired.encounterStarted && !coordinator.isFighting()
            && coordinator.playerProjectiles().activeCount() == 1,
        "the generated player can fire outside an active encounter");

    PlayerInput resetInput;
    resetInput.restartPressed = true;
    updateGeneratedEncounter(
        coordinator, session, level, resetInput, 1.0F / 120.0F);
    valid &= check(coordinator.playerProjectiles().activeCount() == 0,
        "generated restart clears traversal-fired projectiles");
    return valid;
}

bool generatedCombatLocksClearsAndReopensRoom(const GeneratedLevel& level)
{
    std::optional<int> encounterRoom;
    std::optional<Vector2> playerPosition;
    std::vector<GeneratedEnemySpawn> enemySpawns;
    const auto assignments = level.roomLayout().getCellAssignments();
    std::size_t encounterEntranceCount = 0;
    std::size_t spawnableEntranceCount = 0;
    for (const stalberg::rooms::GeneratedRoom& room
        : level.roomLayout().getRooms()) {
        if (room.role != stalberg::rooms::RoomRole::Combat
            && room.role != stalberg::rooms::RoomRole::Hub) {
            continue;
        }
        for (const DoorwayThreshold& doorway : level.doorwayThresholds()) {
            CellIndex entryCell = 0;
            if (doorway.firstRegion == room.id) {
                entryCell = doorway.firstCell;
            } else if (doorway.secondRegion == room.id) {
                entryCell = doorway.secondCell;
            } else {
                continue;
            }
            ++encounterEntranceCount;
            const Vector2 candidatePlayerPosition {
                level.dualGrid().cells[entryCell].center.x * level.worldScale(),
                level.dualGrid().cells[entryCell].center.y * level.worldScale()
            };
            const auto spawns = selectGeneratedEnemySpawns(
                level, room.id, candidatePlayerPosition);
            if (spawns.empty()) {
                continue;
            }
            ++spawnableEntranceCount;
            if (!encounterRoom.has_value()
                && spawns.size() == GENERATED_ENEMIES_PER_ENCOUNTER) {
                encounterRoom = room.id;
                playerPosition = candidatePlayerPosition;
                enemySpawns = spawns;
            }
        }
    }

    bool valid = check(encounterEntranceCount > 0
            && spawnableEntranceCount == encounterEntranceCount
            && encounterRoom.has_value() && playerPosition.has_value()
            && enemySpawns.size() == GENERATED_ENEMIES_PER_ENCOUNTER,
        "every generated encounter entrance has a spawn and one supports a crowd");
    if (!valid) {
        return false;
    }
    const auto repeatedSpawns = selectGeneratedEnemySpawns(
        level, *encounterRoom, *playerPosition);
    bool repeatedMatch = repeatedSpawns.size() == enemySpawns.size();
    for (std::size_t index = 0;
        repeatedMatch && index < enemySpawns.size(); ++index) {
        repeatedMatch = repeatedSpawns[index].id == enemySpawns[index].id
            && samePoint(repeatedSpawns[index].position,
                enemySpawns[index].position);
    }
    valid &= check(repeatedMatch,
        "enemy identities, spawn selection, and spawn order are deterministic");

    LevelSession session(level);
    GeneratedEncounterCoordinator coordinator;
    const GeneratedEncounterStepResult startStep = updateGeneratedEncounter(
        coordinator, session, level, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(!startStep.encounterStarted && !coordinator.isFighting(),
        "the generated Start room remains traversal-only");
    session.player().position = Vector3 {
        playerPosition->x, PLAYER_RADIUS, playerPosition->y
    };
    session.player().health = PLAYER_MAX_HEALTH - 1;
    PlayerInput entryInput;
    entryInput.hasAimPoint = true;
    entryInput.aimPoint = Vector3 {
        enemySpawns.front().position.x,
        0.0F,
        enemySpawns.front().position.y
    };
    entryInput.fireHeld = true;
    const GeneratedEncounterStepResult entered = updateGeneratedEncounter(
        coordinator, session, level, entryInput, 1.0F / 120.0F);
    valid &= check(entered.encounterStarted
            && coordinator.isFighting()
            && coordinator.encounterRoom() == encounterRoom
            && session.lockedRoom() == encounterRoom,
        "entering a dormant combat room starts and locks its encounter");
    valid &= check(session.roomStates()[static_cast<std::size_t>(*encounterRoom)]
                == RoomLifecycleState::fighting
            && session.activeWalls().size() > level.walls().size(),
        "an active encounter advances lifecycle and closes doorway walls");
    for (const DoorwayThreshold& doorway : level.doorwayThresholds()) {
        if (doorway.firstRegion == *encounterRoom
            || doorway.secondRegion == *encounterRoom) {
            valid &= check(session.doorwayIsLocked(doorway.doorway)
                    && hasWall(session.activeWalls(),
                        doorway.segment.start, doorway.segment.end)
                    && !session.canTraverse(
                        doorway.firstCell, doorway.secondCell),
                "generated combat locks every doorway incident to its room");
        }
    }
    const GeneratedCombatState* startedCombat = coordinator.activeCombat();
    valid &= check(startedCombat != nullptr
            && startedCombat->enemies.size()
                == GENERATED_ENEMIES_PER_ENCOUNTER
            && startedCombat->enemies.entries().front().id()
                == enemySpawns.front().id
            && samePoint(startedCombat->enemies.entries().front().enemy.position,
                enemySpawns.front().position)
            && session.player().health == PLAYER_MAX_HEALTH - 1
            && coordinator.playerProjectiles().activeCount() == 1,
        "generated combat preserves the player and shot while spawning a crowd");

    GeneratedCombatState* combat = coordinator.activeCombat();
    if (combat == nullptr) {
        return false;
    }
    StableEnemy& firstEnemy = combat->enemies.entries().front();
    firstEnemy.enemy.health = 1;
    firstEnemy.enemy.previousPosition = firstEnemy.enemy.position;
    valid &= check(coordinator.playerProjectiles().spawn(
                       firstEnemy.enemy.position, Vector2 { 1.0F, 0.0F }),
        "a finishing player projectile can be staged for one crowd member");
    const GeneratedEncounterStepResult partiallyCleared
        = updateGeneratedEncounter(
            coordinator, session, level, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(!partiallyCleared.encounterCleared
            && coordinator.isFighting()
            && session.lockedRoom() == encounterRoom
            && combat->enemies.livingCount()
                == GENERATED_ENEMIES_PER_ENCOUNTER - 1,
        "defeating one enemy keeps every encounter doorway locked");

    for (StableEnemy& entry : combat->enemies.entries()) {
        if (!isEnemyAlive(entry.enemy)) {
            continue;
        }
        entry.enemy.health = 1;
        entry.enemy.previousPosition = entry.enemy.position;
        valid &= check(coordinator.playerProjectiles().spawn(
                           entry.enemy.position, Vector2 { 1.0F, 0.0F }),
            "each remaining enemy can receive a deterministic finishing shot");
    }
    valid &= check(combat->enemyProjectiles.spawn(
                       firstEnemy.enemy.position, Vector2 { 1.0F, 0.0F }),
        "a hostile shot can be staged before the all-clear transition");
    const GeneratedEncounterStepResult cleared = updateGeneratedEncounter(
        coordinator, session, level, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(cleared.encounterCleared && !coordinator.isFighting()
            && !session.lockedRoom().has_value()
            && session.roomStates()[static_cast<std::size_t>(*encounterRoom)]
                == RoomLifecycleState::cleared,
        "defeating every enemy clears the room and ends its encounter");
    valid &= check(combat->enemyProjectiles.activeCount() == 0
            && coordinator.playerProjectiles().activeCount() > 0,
        "all-clear removes hostile shots without discarding player shots");
    valid &= check(session.activeWalls().size() == level.walls().size(),
        "clearing generated combat reopens every retained room doorway");
    for (const DoorwayThreshold& doorway : level.doorwayThresholds()) {
        if (doorway.firstRegion == *encounterRoom
            || doorway.secondRegion == *encounterRoom) {
            valid &= check(!session.doorwayIsLocked(doorway.doorway)
                    && session.canTraverse(
                        doorway.firstCell, doorway.secondCell),
                "cleared generated combat restores each room traversal edge");
        }
    }

    PlayerInput resetInput;
    resetInput.restartPressed = true;
    const GeneratedEncounterStepResult reset = updateGeneratedEncounter(
        coordinator, session, level, resetInput, 1.0F / 120.0F);
    valid &= check(reset.levelSession.reset
            && !coordinator.encounterRoom().has_value()
            && session.player().health == PLAYER_MAX_HEALTH,
        "generated restart resets both session and encounter coordinator state");

    session.player().position = Vector3 {
        playerPosition->x, PLAYER_RADIUS, playerPosition->y
    };
    const GeneratedEncounterStepResult reentered = updateGeneratedEncounter(
        coordinator, session, level, PlayerInput {}, 1.0F / 120.0F);
    GeneratedCombatState* defeatCombat = coordinator.activeCombat();
    bool resetIdentitiesMatch = defeatCombat != nullptr
        && defeatCombat->enemies.size() == enemySpawns.size();
    for (std::size_t index = 0;
        resetIdentitiesMatch && index < enemySpawns.size(); ++index) {
        resetIdentitiesMatch
            = defeatCombat->enemies.entries()[index].id()
            == enemySpawns[index].id;
    }
    valid &= check(reentered.encounterStarted && resetIdentitiesMatch,
        "whole-match reset restores the same stable enemy identities");
    if (defeatCombat == nullptr) {
        return false;
    }
    session.player().health = 1;
    const Vector2 sessionPlayerPosition {
        session.player().position.x,
        session.player().position.z
    };
    defeatCombat->enemyProjectiles.spawn(
        sessionPlayerPosition, Vector2 { 1.0F, 0.0F });
    const GeneratedEncounterStepResult defeated = updateGeneratedEncounter(
        coordinator, session, level, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(defeated.combat.playerDamage == PlayerDamageResult::died
            && coordinator.isFighting()
            && session.lockedRoom() == encounterRoom,
        "generated player defeat freezes combat and leaves the room locked");

    const GeneratedEncounterStepResult defeatReset = updateGeneratedEncounter(
        coordinator, session, level, resetInput, 1.0F / 120.0F);
    valid &= check(defeatReset.levelSession.reset
            && !coordinator.isFighting()
            && !session.lockedRoom().has_value(),
        "restart recovers a defeated generated room without stale locks");

    std::optional<int> exitRoom;
    std::optional<Vector2> exitPosition;
    for (const stalberg::rooms::GeneratedRoom& room
        : level.roomLayout().getRooms()) {
        if (room.role != stalberg::rooms::RoomRole::Exit) {
            continue;
        }
        exitRoom = room.id;
        for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
            if (assignments[cell] == room.id) {
                exitPosition = Vector2 {
                    level.dualGrid().cells[cell].center.x * level.worldScale(),
                    level.dualGrid().cells[cell].center.y * level.worldScale()
                };
                break;
            }
        }
        break;
    }
    if (!exitRoom.has_value() || !exitPosition.has_value()) {
        return check(false, "generated progression has an Exit room");
    }
    session.player().position = Vector3 {
        exitPosition->x, PLAYER_RADIUS, exitPosition->y
    };
    const GeneratedEncounterStepResult completed = updateGeneratedEncounter(
        coordinator, session, level, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(completed.floorCompleted && coordinator.floorIsComplete()
            && session.roomStates()[static_cast<std::size_t>(*exitRoom)]
                == RoomLifecycleState::cleared,
        "entering the generated Exit completes the cleared floor progression");

    updateGeneratedEncounter(
        coordinator, session, level, resetInput, 1.0F / 120.0F);
    valid &= check(!coordinator.floorIsComplete(),
        "generated restart clears floor-completion state");
    return valid;
}

bool spawnAndNavigationReachAllFloor(const GeneratedLevel& level);

bool representativeConfigurationsRemainValid()
{
    bool valid = check(!REPRESENTATIVE_LEVEL_CONFIGS.empty(),
        "the runtime seed browser publishes representative configurations");
    std::vector<std::vector<int>> publishedAssignments;
    for (std::size_t index = 0;
         index < REPRESENTATIVE_LEVEL_CONFIGS.size(); ++index) {
        const GeneratedLevelConfig& config
            = REPRESENTATIVE_LEVEL_CONFIGS[index];
        for (std::size_t other = 0; other < index; ++other) {
            const GeneratedLevelConfig& candidate
                = REPRESENTATIVE_LEVEL_CONFIGS[other];
            valid &= check(config.gridRadius != candidate.gridRadius
                    || config.gridSeed != candidate.gridSeed
                    || config.roomSeed != candidate.roomSeed,
                "representative browser configurations are unique");
        }

        const GeneratedLevel level(config);
        valid &= check(level.grid().getRadius() == config.gridRadius
                && level.grid().getSeed() == config.gridSeed
                && level.roomLayout().getSeed() == config.roomSeed,
            "representative browser configuration metadata remains exact");
        valid &= check(level.roomLayout().getRoomCount() > 0
                && std::isfinite(level.roomLayout().getQualityScore())
                && level.roomLayout().getQualityScore() > 0.0F,
            "every representative browser configuration generates a valid layout");
        const auto assignments = level.roomLayout().getCellAssignments();
        const std::vector<int> published(
            assignments.begin(), assignments.end());
        for (const std::vector<int>& previous : publishedAssignments) {
            valid &= check(published != previous,
                "representative browser layouts have distinct assignments");
        }
        publishedAssignments.push_back(published);

        valid &= packageRetainsAlignedGeneratorArtifacts(level);
        valid &= floorsTriangulateExactlyAssignedPolygons(level);
        valid &= navigationOpensOnlyPublishedDoorways(level);
        valid &= wallsHaveStableCanonicalOrder(level);
        valid &= wallsCloseEveryUnauthorizedDualBoundary(level);
        valid &= sessionOwnsLifecycleAndDynamicDoorWalls(level);
        valid &= spawnAndNavigationReachAllFloor(level);

        const GeneratedLevel repeated(config);
        valid &= check(std::ranges::equal(
                           level.roomLayout().getCellAssignments(),
                           repeated.roomLayout().getCellAssignments())
                && level.roomLayout().getSelectedCandidate()
                    == repeated.roomLayout().getSelectedCandidate()
                && level.roomLayout().getQualityScore()
                    == repeated.roomLayout().getQualityScore(),
            "representative browser configurations remain deterministic");
    }
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
    valid &= wallsHaveStableCanonicalOrder(level);
    valid &= wallsCloseEveryUnauthorizedDualBoundary(level);
    valid &= sessionOwnsLifecycleAndDynamicDoorWalls(level);
    valid &= generatedTraversalAllowsPlayerFire(level);
    valid &= generatedCombatLocksClearsAndReopensRoom(level);
    valid &= representativeConfigurationsRemainValid();
    valid &= spawnAndNavigationReachAllFloor(level);
    return valid ? 0 : 1;
}
