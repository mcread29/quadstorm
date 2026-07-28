#include "generated_encounter.hpp"

#include "arena.hpp"
#include "vector2_math.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float MINIMUM_PLAYER_SPAWN_DISTANCE = 4.0F;
constexpr float MINIMUM_ENEMY_CLEARANCE = ENEMY_RADIUS + 0.2F;
constexpr float MINIMUM_DOORWAY_SPAWN_DISTANCE = ENEMY_RADIUS + 0.75F;
constexpr float MINIMUM_WALL_SPAWN_DISTANCE = ENEMY_RADIUS + 0.05F;

using vector2::distanceSquared;

float distanceToSegment(Vector2 point, const Segment2D& segment)
{
    const Vector2 closest = closestPointOnSegment(point, segment).position;
    return std::sqrt(distanceSquared(point, closest));
}

const stalberg::rooms::GeneratedRoom* findRoom(
    const GeneratedLevel& level, int room)
{
    const auto rooms = level.roomLayout().getRooms();
    const auto found = std::ranges::find(rooms, room,
        &stalberg::rooms::GeneratedRoom::id);
    return found == rooms.end() ? nullptr : &*found;
}

bool roomStartsEncounter(const stalberg::rooms::GeneratedRoom& room)
{
    return room.role == stalberg::rooms::RoomRole::Combat
        || room.role == stalberg::rooms::RoomRole::Hub;
}

bool farEnoughFromRoomDoorways(
    const GeneratedLevel& level, int room, Vector2 position)
{
    return std::ranges::none_of(level.doorwayThresholds(),
        [&](const DoorwayThreshold& doorway) {
            return (doorway.firstRegion == room
                       || doorway.secondRegion == room)
                && distanceToSegment(position, doorway.segment)
                    < MINIMUM_DOORWAY_SPAWN_DISTANCE;
        });
}

bool hasWallClearance(const GeneratedLevel& level, Vector2 position)
{
    return std::ranges::none_of(level.walls(),
        [&](const Segment2D& wall) {
            return distanceToSegment(position, wall)
                < MINIMUM_WALL_SPAWN_DISTANCE;
        });
}

} // namespace

CombatState* GeneratedEncounterCoordinator::activeCombat()
{
    return fighting ? &combatState : nullptr;
}

const CombatState* GeneratedEncounterCoordinator::activeCombat() const
{
    return fighting ? &combatState : nullptr;
}

const CombatState* GeneratedEncounterCoordinator::combatForRoom(
    std::optional<int> room) const
{
    return roomId.has_value() && room == roomId ? &combatState : nullptr;
}

void resetGeneratedEncounter(GeneratedEncounterCoordinator& coordinator)
{
    coordinator = GeneratedEncounterCoordinator {};
}

std::optional<Vector2> selectGeneratedEnemySpawn(
    const GeneratedLevel& level, int room, Vector2 playerPosition)
{
    const stalberg::rooms::GeneratedRoom* generatedRoom = findRoom(level, room);
    if (generatedRoom == nullptr) {
        return std::nullopt;
    }

    const auto playerCell = level.cellAtWorldPoint(playerPosition);
    const auto assignments = level.roomLayout().getCellAssignments();
    std::optional<Vector2> bestPosition;
    stalberg::rooms::CellIndex bestCell = 0;

    for (const stalberg::rooms::CellIndex cell
        : generatedRoom->enemySpawnCandidates) {
        if (cell >= assignments.size() || assignments[cell] != room
            || (playerCell.has_value() && cell == *playerCell)) {
            continue;
        }

        const stalberg::DualCell& dualCell = level.dualGrid().cells[cell];
        const float clearance = dualCell.clearance * level.worldScale();
        const Vector2 position {
            dualCell.center.x * level.worldScale(),
            dualCell.center.y * level.worldScale()
        };
        const float playerDistance = distanceSquared(position, playerPosition);
        if (!std::isfinite(clearance) || !std::isfinite(position.x)
            || !std::isfinite(position.y) || !std::isfinite(playerDistance)
            || clearance < MINIMUM_ENEMY_CLEARANCE
            || playerDistance
                < MINIMUM_PLAYER_SPAWN_DISTANCE
                    * MINIMUM_PLAYER_SPAWN_DISTANCE
            || !farEnoughFromRoomDoorways(level, room, position)
            || !hasWallClearance(level, position)) {
            continue;
        }

        if (!bestPosition.has_value() || cell < bestCell) {
            bestPosition = position;
            bestCell = cell;
        }
    }
    return bestPosition;
}

GeneratedEncounterStepResult updateGeneratedEncounter(
    GeneratedEncounterCoordinator& coordinator, LevelSession& session,
    const GeneratedLevel& level, const PlayerInput& input, float stepTime)
{
    GeneratedEncounterStepResult result;
    if (input.restartPressed) {
        result.levelSession = session.update(input, stepTime);
        resetGeneratedEncounter(coordinator);
        return result;
    }

    if (coordinator.fighting) {
        result.combat = updateCombat(coordinator.combatState,
            session.player(), input, stepTime, session.activeWalls());
        if (result.combat.enemyDamage == EnemyDamageResult::died
            && coordinator.roomId.has_value()) {
            const int room = *coordinator.roomId;
            if (session.clearEncounter(room)) {
                coordinator.combatState.playerProjectiles = ProjectilePool {};
                coordinator.combatState.enemyProjectiles
                    = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
                coordinator.fighting = false;
                result.encounterCleared = true;
            }
        }
        return result;
    }

    result.levelSession = session.update(input, stepTime);
    if (!result.levelSession.enteredRoom.has_value()) {
        return result;
    }

    const int room = *result.levelSession.enteredRoom;
    const stalberg::rooms::GeneratedRoom* generatedRoom = findRoom(level, room);
    if (generatedRoom == nullptr
        || session.roomStates()[static_cast<std::size_t>(room)]
            != RoomLifecycleState::entered) {
        return result;
    }
    if (generatedRoom->role == stalberg::rooms::RoomRole::Exit) {
        session.markRoomCleared(room);
        coordinator.floorComplete = true;
        result.floorCompleted = true;
        return result;
    }
    if (!roomStartsEncounter(*generatedRoom)) {
        return result;
    }

    const Vector2 playerPosition {
        session.player().position.x,
        session.player().position.z
    };
    const auto spawn = selectGeneratedEnemySpawn(level, room, playerPosition);
    if (!spawn.has_value()) {
        session.markRoomCleared(room);
        return result;
    }

    if (!session.beginEncounter(room)) {
        return result;
    }

    const Vector2 lockedPosition {
        session.player().position.x,
        session.player().position.z
    };
    resolvePlayerWallCollisions(
        session.player(), lockedPosition, session.activeWalls());
    resetCombat(coordinator.combatState, *spawn);
    coordinator.roomId = room;
    coordinator.fighting = true;
    result.encounterStarted = true;
    return result;
}
