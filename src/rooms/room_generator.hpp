#pragma once

#include "rooms/room_grid.hpp"
#include "rooms/room_layout.hpp"

#include <cstdint>

namespace stalberg::rooms {

class RoomGenerator {
public:
    RoomLayout generate(
        const RoomGrid& grid,
        std::uint32_t seed,
        RoomGenerationMethod method = RoomGenerationMethod::BranchingShapes) const;
};

} // namespace stalberg::rooms
