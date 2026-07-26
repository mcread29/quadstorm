#pragma once

#include "rooms/room_grid.hpp"

namespace stalberg {

class StalbergGrid;

rooms::RoomGrid makeRoomGrid(const StalbergGrid& grid);

} // namespace stalberg
