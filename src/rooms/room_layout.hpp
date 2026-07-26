#pragma once

#include "rooms/room_grid.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace stalberg::rooms {

inline constexpr int EMPTY_CELL = -1;
inline constexpr std::size_t MAX_GENERATED_ROOMS = 64;

enum class RoomGenerationMethod : std::uint8_t {
    BranchingShapes,
    OrganicGrowth
};

enum class RoomRole : std::uint8_t {
    Start,
    Combat,
    Connector,
    Hub,
    Reward,
    Exit
};

struct GeneratedRoom {
    int id = 0;
    std::size_t cellCount = 0;
    float area = 0.0F;
    RoomRole role = RoomRole::Combat;
    std::vector<CellIndex> coverCandidates;
    std::vector<CellIndex> enemySpawnCandidates;

    GeneratedRoom() = default;
    GeneratedRoom(int roomId, std::size_t roomCellCount)
        : id(roomId), cellCount(roomCellCount)
    {
    }
};

struct Doorway {
    int firstRegion;
    CellIndex firstCell;
    int secondRegion;
    CellIndex secondCell;
    float width = 0.0F;
    float quality = 0.0F;
};

class RoomGenerator;

class RoomLayout {
public:
    std::uint32_t getSeed() const { return seed; }
    RoomGenerationMethod getMethod() const { return method; }
    std::size_t getRoomCount() const { return rooms.size(); }
    float getQualityScore() const { return qualityScore; }
    std::size_t getSelectedCandidate() const { return selectedCandidate; }
    int getCellAssignment(CellIndex cell) const;

    std::span<const int> getCellAssignments() const { return cellAssignments; }
    std::span<const GeneratedRoom> getRooms() const { return rooms; }
    std::span<const Doorway> getDoorways() const { return doorways; }
    std::span<const CellIndex> getConnectedEntrances() const
    {
        return connectedEntrances;
    }

private:
    friend class RoomGenerator;

    std::uint32_t seed = 1;
    RoomGenerationMethod method {};
    std::vector<int> cellAssignments;
    std::vector<GeneratedRoom> rooms;
    std::vector<Doorway> doorways;
    std::vector<CellIndex> connectedEntrances;
    float qualityScore = 0.0F;
    std::size_t selectedCandidate = 0;
};

} // namespace stalberg::rooms
