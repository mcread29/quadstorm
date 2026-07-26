#include "rooms/room_layout.hpp"

namespace stalberg::rooms {

int RoomLayout::getCellAssignment(CellIndex cell) const
{
    return cell < cellAssignments.size() ? cellAssignments[cell] : EMPTY_CELL;
}

} // namespace stalberg::rooms
