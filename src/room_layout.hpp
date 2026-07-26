#pragma once

#include "stalberg_grid.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace stalberg {

inline constexpr int EMPTY_CELL = -1;
inline constexpr std::size_t MAX_GENERATED_ROOMS = 32;

struct GeneratedRoom {
    int id;
    std::size_t cellCount;
};

struct Doorway {
    int firstRegion;
    VertexIndex firstCell;
    int secondRegion;
    VertexIndex secondCell;
};

class RoomLayout {
public:
    void generate(const StalbergGrid& grid, std::uint32_t newSeed);

    std::uint32_t getSeed() const { return seed; }
    std::size_t getRoomCount() const { return rooms.size(); }
    int getCellAssignment(VertexIndex cell) const;

    std::span<const int> getCellAssignments() const { return cellAssignments; }
    std::span<const GeneratedRoom> getRooms() const { return rooms; }
    std::span<const Doorway> getDoorways() const { return doorways; }
    std::span<const VertexIndex> getConnectedEdgeCenters() const
    {
        return connectedEdgeCenters;
    }

private:
    std::uint32_t seed = 1;
    std::vector<int> cellAssignments;
    std::vector<GeneratedRoom> rooms;
    std::vector<Doorway> doorways;
    std::vector<VertexIndex> connectedEdgeCenters;
};

} // namespace stalberg
