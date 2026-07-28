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

    Player& player() { return playerState; }
    const Player& player() const { return playerState; }
    std::optional<int> currentRoom() const { return currentRoomId; }
    std::optional<int> lockedRoom() const { return lockedRoomId; }
    std::span<const RoomLifecycleState> roomStates() const { return states; }
    std::span<const Segment2D> activeWalls() const { return collisionWalls; }

    bool doorwayIsLocked(std::size_t doorway) const;
    bool setDoorwayLocked(std::size_t doorway, bool locked);
    void openAllDoorways();
    bool canTraverse(stalberg::rooms::CellIndex first,
        stalberg::rooms::CellIndex second) const;

    void reset();
    LevelSessionStepResult update(const PlayerInput& input, float stepTime);
    bool beginEncounter(int room);
    bool clearEncounter(int room);
    bool markRoomCleared(int room);

private:
    const GeneratedLevel* level;
    Player playerState;
    std::vector<RoomLifecycleState> states;
    std::optional<int> currentRoomId;
    std::optional<int> lockedRoomId;
    std::vector<bool> lockedDoorways;
    std::vector<Segment2D> collisionWalls;

    bool validRoom(int room) const;
    void rebuildWalls();
};
