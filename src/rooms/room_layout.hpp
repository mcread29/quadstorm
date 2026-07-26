#pragma once

#include "rooms/room_grid.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace stalberg::rooms {

inline constexpr int EMPTY_CELL = -1;
inline constexpr std::size_t MAX_GENERATED_ROOMS = 64;

struct GeneratedRoom {
    int id;
    std::size_t cellCount;
};

struct Doorway {
    int firstRegion;
    CellIndex firstCell;
    int secondRegion;
    CellIndex secondCell;
};

class RoomGenerator;

class RoomLayout {
public:
    std::uint32_t getSeed() const { return seed; }
    std::size_t getRoomCount() const { return rooms.size(); }
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
    std::vector<int> cellAssignments;
    std::vector<GeneratedRoom> rooms;
    std::vector<Doorway> doorways;
    std::vector<CellIndex> connectedEntrances;
};

} // namespace stalberg::rooms
