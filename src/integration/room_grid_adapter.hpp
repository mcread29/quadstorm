#pragma once

#include "rooms/room_grid.hpp"

namespace stalberg {

class StalbergGrid;
struct DualGrid;

rooms::RoomGrid makeRoomGrid(const StalbergGrid& grid);
rooms::RoomGrid makeRoomGrid(const StalbergGrid& grid, const DualGrid& dual);

} // namespace stalberg
