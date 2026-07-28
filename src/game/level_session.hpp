#pragma once

#include "generated_level.hpp"
#include "player.hpp"

#include <optional>
#include <span>
#include <vector>

enum class RoomLifecycleState {
    dormant,
    entered,
    locked,
    fighting,
    cleared,
    rewarded
};

struct LevelSessionStepResult {
    bool reset = false;
    std::optional<int> enteredRoom;
};

class LevelSession {
public:
    explicit LevelSession(const GeneratedLevel& level);

    const Player& player() const { return playerState; }
    std::optional<int> currentRoom() const { return currentRoomId; }
    std::optional<int> lockedRoom() const { return lockedRoomId; }
    std::span<const RoomLifecycleState> roomStates() const { return states; }
    std::span<const Segment2D> activeWalls() const { return collisionWalls; }

    bool doorwayIsLocked(std::size_t doorway) const;
    bool canTraverse(const GeneratedLevel& level,
        stalberg::rooms::CellIndex first,
        stalberg::rooms::CellIndex second) const;

private:
    friend void resetLevelSession(LevelSession&, const GeneratedLevel&);
    friend LevelSessionStepResult updateLevelSession(LevelSession&,
        const GeneratedLevel&, const PlayerInput&, float);
    friend bool lockRoom(LevelSession&, const GeneratedLevel&, int);
    friend bool setRoomLifecycleState(LevelSession&, int, RoomLifecycleState);
    friend bool unlockRoom(LevelSession&, const GeneratedLevel&, int,
        RoomLifecycleState);

    Player playerState;
    std::vector<RoomLifecycleState> states;
    std::optional<int> currentRoomId;
    std::optional<int> lockedRoomId;
    std::vector<bool> lockedDoorways;
    std::vector<Segment2D> collisionWalls;

    void rebuildWalls(const GeneratedLevel& level);
};

void resetLevelSession(LevelSession& session, const GeneratedLevel& level);
LevelSessionStepResult updateLevelSession(LevelSession& session,
    const GeneratedLevel& level, const PlayerInput& input, float stepTime);
bool lockRoom(LevelSession& session, const GeneratedLevel& level, int room);
bool setRoomLifecycleState(
    LevelSession& session, int room, RoomLifecycleState state);
bool unlockRoom(LevelSession& session, const GeneratedLevel& level, int room,
    RoomLifecycleState nextState);
