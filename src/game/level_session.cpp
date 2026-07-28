#include "level_session.hpp"

#include "arena.hpp"

#include <algorithm>

namespace {

bool thresholdMatches(const DoorwayThreshold& threshold,
    stalberg::rooms::CellIndex first, stalberg::rooms::CellIndex second)
{
    return (threshold.firstCell == first && threshold.secondCell == second)
        || (threshold.firstCell == second && threshold.secondCell == first);
}

} // namespace

LevelSession::LevelSession(const GeneratedLevel& sourceLevel)
    : level(&sourceLevel)
{
    reset();
}

bool LevelSession::validRoom(int room) const
{
    return room >= 0 && static_cast<std::size_t>(room) < states.size();
}

bool LevelSession::doorwayIsLocked(std::size_t doorway) const
{
    return doorway < lockedDoorways.size() && lockedDoorways[doorway];
}

bool LevelSession::setDoorwayLocked(std::size_t doorway, bool locked)
{
    return setDoorwaysLocked(std::span { &doorway, 1U }, locked);
}

bool LevelSession::setDoorwaysLocked(
    std::span<const std::size_t> doorways, bool locked)
{
    if (std::ranges::any_of(doorways,
            [&](std::size_t doorway) {
                return doorway >= lockedDoorways.size();
            })) {
        return false;
    }
    for (const std::size_t doorway : doorways) {
        lockedDoorways[doorway] = locked;
    }
    rebuildWalls();
    return true;
}

void LevelSession::openAllDoorways()
{
    std::fill(lockedDoorways.begin(), lockedDoorways.end(), false);
    lockedRoomId.reset();
    rebuildWalls();
}

bool LevelSession::canTraverse(stalberg::rooms::CellIndex first,
    stalberg::rooms::CellIndex second) const
{
    if (!level->canTraverse(first, second)) {
        return false;
    }
    for (const DoorwayThreshold& threshold : level->doorwayThresholds()) {
        if (thresholdMatches(threshold, first, second)) {
            return !doorwayIsLocked(threshold.doorway);
        }
    }
    return true;
}

void LevelSession::rebuildWalls()
{
    collisionWalls.assign(level->walls().begin(), level->walls().end());
    for (const DoorwayThreshold& threshold : level->doorwayThresholds()) {
        if (doorwayIsLocked(threshold.doorway)) {
            collisionWalls.push_back(threshold.segment);
        }
    }
}

void LevelSession::reset()
{
    playerState = Player {};
    const Vector2 spawn = level->playerSpawn();
    playerState.position = Vector3 { spawn.x, PLAYER_RADIUS, spawn.y };
    states.assign(level->roomLayout().getRoomCount(),
        RoomLifecycleState::dormant);
    lockedDoorways.assign(level->doorwayThresholds().size(), false);
    lockedRoomId.reset();
    currentRoomId = level->roomLayout().getCellAssignment(
        level->playerSpawnCell());
    if (currentRoomId.has_value() && validRoom(*currentRoomId)) {
        states[static_cast<std::size_t>(*currentRoomId)]
            = RoomLifecycleState::entered;
    }
    rebuildWalls();
}

LevelSessionStepResult LevelSession::update(
    const PlayerInput& input, float stepTime)
{
    LevelSessionStepResult result;
    if (input.restartPressed) {
        reset();
        result.reset = true;
        return result;
    }

    const Vector2 previousPosition {
        playerState.position.x,
        playerState.position.z
    };
    updatePlayerEffects(playerState, stepTime);
    updatePlayer(playerState, input, stepTime);
    resolvePlayerWallCollisions(playerState, previousPosition, activeWalls());

    const auto cell = level->cellAtWorldPoint(Vector2 {
        playerState.position.x,
        playerState.position.z
    });
    if (!cell.has_value()) {
        currentRoomId.reset();
        return result;
    }

    const int room = level->roomLayout().getCellAssignment(*cell);
    if (!validRoom(room)) {
        currentRoomId.reset();
        return result;
    }
    if (currentRoomId != room) {
        currentRoomId = room;
        result.enteredRoom = room;
        RoomLifecycleState& state = states[static_cast<std::size_t>(room)];
        if (state == RoomLifecycleState::dormant) {
            state = RoomLifecycleState::entered;
        }
    }
    return result;
}

bool LevelSession::beginEncounter(int room)
{
    if (!validRoom(room) || currentRoomId != room || lockedRoomId.has_value()
        || states[static_cast<std::size_t>(room)] != RoomLifecycleState::entered) {
        return false;
    }

    std::vector<bool> newLocks(lockedDoorways.size(), false);
    for (const DoorwayThreshold& threshold : level->doorwayThresholds()) {
        if (threshold.firstRegion == room || threshold.secondRegion == room) {
            newLocks[threshold.doorway] = true;
        }
    }
    lockedDoorways = std::move(newLocks);
    lockedRoomId = room;
    states[static_cast<std::size_t>(room)] = RoomLifecycleState::fighting;
    rebuildWalls();
    return true;
}

bool LevelSession::clearEncounter(int room)
{
    if (!validRoom(room) || lockedRoomId != room
        || states[static_cast<std::size_t>(room)] != RoomLifecycleState::fighting) {
        return false;
    }

    std::fill(lockedDoorways.begin(), lockedDoorways.end(), false);
    lockedRoomId.reset();
    states[static_cast<std::size_t>(room)] = RoomLifecycleState::cleared;
    rebuildWalls();
    return true;
}

bool LevelSession::markRoomCleared(int room)
{
    if (!validRoom(room) || lockedRoomId.has_value()
        || states[static_cast<std::size_t>(room)] != RoomLifecycleState::entered) {
        return false;
    }
    states[static_cast<std::size_t>(room)] = RoomLifecycleState::cleared;
    return true;
}
