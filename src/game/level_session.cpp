#include "level_session.hpp"

#include "arena.hpp"

#include <algorithm>

namespace {

bool validRoom(const LevelSession& session, int room)
{
    return room >= 0
        && static_cast<std::size_t>(room) < session.roomStates().size();
}

bool thresholdMatches(const DoorwayThreshold& threshold,
    stalberg::rooms::CellIndex first, stalberg::rooms::CellIndex second)
{
    return (threshold.firstCell == first && threshold.secondCell == second)
        || (threshold.firstCell == second && threshold.secondCell == first);
}

} // namespace

LevelSession::LevelSession(const GeneratedLevel& level)
{
    resetLevelSession(*this, level);
}

bool LevelSession::doorwayIsLocked(std::size_t doorway) const
{
    return doorway < lockedDoorways.size() && lockedDoorways[doorway];
}

bool LevelSession::canTraverse(const GeneratedLevel& level,
    stalberg::rooms::CellIndex first,
    stalberg::rooms::CellIndex second) const
{
    if (!level.canTraverse(first, second)) {
        return false;
    }
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        if (thresholdMatches(threshold, first, second)) {
            return !doorwayIsLocked(threshold.doorway);
        }
    }
    return true;
}

void LevelSession::rebuildWalls(const GeneratedLevel& level)
{
    collisionWalls.assign(level.walls().begin(), level.walls().end());
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        if (doorwayIsLocked(threshold.doorway)) {
            collisionWalls.push_back(threshold.segment);
        }
    }
}

void resetLevelSession(LevelSession& session, const GeneratedLevel& level)
{
    session.playerState = Player {};
    const Vector2 spawn = level.playerSpawn();
    session.playerState.position = Vector3 { spawn.x, PLAYER_RADIUS, spawn.y };
    session.states.assign(level.roomLayout().getRoomCount(),
        RoomLifecycleState::dormant);
    session.lockedDoorways.assign(
        level.doorwayThresholds().size(), false);
    session.lockedRoomId.reset();
    session.currentRoomId = level.roomLayout().getCellAssignment(
        level.playerSpawnCell());
    if (session.currentRoomId.has_value()
        && validRoom(session, *session.currentRoomId)) {
        session.states[static_cast<std::size_t>(*session.currentRoomId)]
            = RoomLifecycleState::entered;
    }
    session.rebuildWalls(level);
}

LevelSessionStepResult updateLevelSession(LevelSession& session,
    const GeneratedLevel& level, const PlayerInput& input, float stepTime)
{
    LevelSessionStepResult result;
    if (input.restartPressed) {
        resetLevelSession(session, level);
        result.reset = true;
        return result;
    }

    const Vector2 previousPosition {
        session.playerState.position.x,
        session.playerState.position.z
    };
    updatePlayer(session.playerState, input, stepTime);
    resolvePlayerWallCollisions(session.playerState,
        previousPosition, session.activeWalls());

    const auto cell = level.cellAtWorldPoint(Vector2 {
        session.playerState.position.x,
        session.playerState.position.z
    });
    if (!cell.has_value()) {
        session.currentRoomId.reset();
        return result;
    }

    const int room = level.roomLayout().getCellAssignment(*cell);
    if (!validRoom(session, room)) {
        session.currentRoomId.reset();
        return result;
    }
    if (session.currentRoomId != room) {
        session.currentRoomId = room;
        result.enteredRoom = room;
        RoomLifecycleState& state
            = session.states[static_cast<std::size_t>(room)];
        if (state == RoomLifecycleState::dormant) {
            state = RoomLifecycleState::entered;
        }
    }
    return result;
}

bool lockRoom(LevelSession& session, const GeneratedLevel& level, int room)
{
    if (!validRoom(session, room)
        || (session.lockedRoomId.has_value()
            && session.lockedRoomId != room)) {
        return false;
    }

    session.lockedRoomId = room;
    session.states[static_cast<std::size_t>(room)]
        = RoomLifecycleState::locked;
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        if (threshold.firstRegion == room || threshold.secondRegion == room) {
            session.lockedDoorways[threshold.doorway] = true;
        }
    }
    session.rebuildWalls(level);
    return true;
}

bool setRoomLifecycleState(
    LevelSession& session, int room, RoomLifecycleState state)
{
    if (!validRoom(session, room) || state == RoomLifecycleState::locked) {
        return false;
    }
    session.states[static_cast<std::size_t>(room)] = state;
    return true;
}

bool unlockRoom(LevelSession& session, const GeneratedLevel& level, int room,
    RoomLifecycleState nextState)
{
    if (!validRoom(session, room) || session.lockedRoomId != room
        || nextState == RoomLifecycleState::locked) {
        return false;
    }

    std::fill(session.lockedDoorways.begin(),
        session.lockedDoorways.end(), false);
    session.lockedRoomId.reset();
    session.states[static_cast<std::size_t>(room)] = nextState;
    session.rebuildWalls(level);
    return true;
}
