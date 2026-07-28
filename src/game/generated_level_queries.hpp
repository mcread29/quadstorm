#pragma once

#include "generated_level.hpp"

#include <ranges>

namespace generated_level {

inline auto findRoomById(const GeneratedLevel& level, int roomId)
{
    const auto rooms = level.roomLayout().getRooms();
    const auto room = std::ranges::find(
        rooms, roomId, &stalberg::rooms::GeneratedRoom::id);
    return room == rooms.end() ? nullptr : &*room;
}

inline auto findRoomByRole(
    const GeneratedLevel& level, stalberg::rooms::RoomRole role)
{
    const auto rooms = level.roomLayout().getRooms();
    const auto room = std::ranges::find(
        rooms, role, &stalberg::rooms::GeneratedRoom::role);
    return room == rooms.end() ? nullptr : &*room;
}

inline auto worldCellCenter(
    const GeneratedLevel& level, stalberg::rooms::CellIndex cell)
{
    const stalberg::Point center = level.dualGrid().cells[cell].center;
    return Vector2 {
        center.x * level.worldScale(), center.y * level.worldScale()
    };
}

} // namespace generated_level
