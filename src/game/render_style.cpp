#include "render_style.hpp"

#include "generated_level_queries.hpp"

#include <algorithm>
#include <cstdint>

namespace game_render {

Color roomRoleColor(stalberg::rooms::RoomRole role)
{
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        return Color { 50, 78, 76, 255 };
    case stalberg::rooms::RoomRole::Combat:
        return Color { 49, 62, 76, 255 };
    case stalberg::rooms::RoomRole::Connector:
        return Color { 40, 53, 62, 255 };
    case stalberg::rooms::RoomRole::Hub:
        return Color { 45, 72, 78, 255 };
    case stalberg::rooms::RoomRole::Reward:
        return Color { 78, 67, 45, 255 };
    case stalberg::rooms::RoomRole::Exit:
        return Color { 55, 78, 64, 255 };
    }
    return Color { 43, 58, 64, 255 };
}

const char* smallMapRecipeName(stalberg::rooms::SmallMapRecipe recipe)
{
    switch (recipe) {
    case stalberg::rooms::SmallMapRecipe::HubCircuit:
        return "HUB CIRCUIT";
    case stalberg::rooms::SmallMapRecipe::BrokenRing:
        return "BROKEN RING";
    case stalberg::rooms::SmallMapRecipe::TwinWings:
        return "TWIN WINGS";
    }
    return "SMALL MAP";
}

const char* roundPhaseName(RoundPhase phase)
{
    switch (phase) {
    case RoundPhase::Intermission:
        return "INTERMISSION";
    case RoundPhase::Buildup:
        return "BUILDUP";
    case RoundPhase::Peak:
        return "PEAK";
    case RoundPhase::Cleanup:
        return "CLEANUP";
    }
    return "ROUND";
}

const char* roomRoleName(stalberg::rooms::RoomRole role)
{
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        return "START";
    case stalberg::rooms::RoomRole::Combat:
        return "COMBAT";
    case stalberg::rooms::RoomRole::Connector:
        return "PASSAGE";
    case stalberg::rooms::RoomRole::Hub:
        return "HUB";
    case stalberg::rooms::RoomRole::Reward:
        return "REWARD";
    case stalberg::rooms::RoomRole::Exit:
        return "EXIT";
    }
    return "ROOM";
}

Color generatedFloorColor(const GeneratedLevel& level,
    int region, stalberg::rooms::CellIndex cell)
{
    const auto* room = generated_level::findRoomById(level, region);
    const Color base = room != nullptr
        ? roomRoleColor(room->role)
        : Color { 43, 58, 64, 255 };
    std::uint32_t hash = static_cast<std::uint32_t>(cell) * 747796405U
        + 2891336453U;
    hash ^= hash >> 16U;
    const float variation = 0.88F
        + static_cast<float>(hash & 255U) / 255.0F * 0.12F;
    return Color {
        static_cast<unsigned char>(std::clamp(base.r * variation, 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(base.g * variation, 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(base.b * variation, 0.0F, 255.0F)),
        255
    };
}

void drawHudPanel(Rectangle bounds, Color accent)
{
    DrawRectangleRounded(Rectangle {
                             bounds.x + 4.0F, bounds.y + 5.0F,
                             bounds.width, bounds.height },
        0.16F, 7, Color { 0, 0, 0, 105 });
    DrawRectangleRounded(bounds, 0.16F, 7, HUD_BORDER);
    DrawRectangleRounded(Rectangle {
                             bounds.x + 1.0F, bounds.y + 1.0F,
                             bounds.width - 2.0F, bounds.height - 2.0F },
        0.16F, 7, HUD_SURFACE);
    DrawRectangle(static_cast<int>(bounds.x + 1.0F),
        static_cast<int>(bounds.y + 9.0F), 3,
        static_cast<int>(bounds.height - 18.0F), accent);
}

} // namespace game_render
