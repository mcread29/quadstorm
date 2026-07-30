#include "render_style.hpp"

#include "generated_level_queries.hpp"

#include <algorithm>
#include <cstdint>

namespace game_render {

void beginUiCanvas()
{
    const float scale = std::max(0.01F, std::min(
        static_cast<float>(GetScreenWidth()) / UI_CANVAS_WIDTH,
        static_cast<float>(GetScreenHeight()) / UI_CANVAS_HEIGHT));
    const Vector2 offset {
        (static_cast<float>(GetScreenWidth()) - UI_CANVAS_WIDTH * scale) * 0.5F,
        (static_cast<float>(GetScreenHeight()) - UI_CANVAS_HEIGHT * scale) * 0.5F
    };
    Camera2D camera {};
    camera.offset = offset;
    camera.zoom = scale;
    BeginMode2D(camera);
}

void endUiCanvas()
{
    EndMode2D();
}

Color roomRoleColor(stalberg::rooms::RoomRole role)
{
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        return Color { 58, 84, 79, 255 };
    case stalberg::rooms::RoomRole::Combat:
        return Color { 59, 74, 87, 255 };
    case stalberg::rooms::RoomRole::Connector:
        return Color { 42, 56, 63, 255 };
    case stalberg::rooms::RoomRole::Hub:
        return Color { 46, 94, 96, 255 };
    case stalberg::rooms::RoomRole::Reward:
        return Color { 110, 82, 49, 255 };
    case stalberg::rooms::RoomRole::Exit:
        return Color { 58, 101, 78, 255 };
    }
    return Color { 38, 52, 58, 255 };
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

const char* largeMapArchetypeName(stalberg::rooms::LargeMapArchetype archetype)
{
    switch (archetype) {
    case stalberg::rooms::LargeMapArchetype::HubAndSpokes:
        return "HUB AND SPOKES";
    case stalberg::rooms::LargeMapArchetype::RingAndBranches:
        return "RING AND BRANCHES";
    case stalberg::rooms::LargeMapArchetype::MainSpine:
        return "MAIN SPINE";
    case stalberg::rooms::LargeMapArchetype::TwinDistricts:
        return "TWIN DISTRICTS";
    case stalberg::rooms::LargeMapArchetype::DenseCoreWithSparseBranch:
        return "DENSE CORE / SPARSE BRANCH";
    }
    return "LARGE MAP";
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
    const float variation = 0.94F
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
    DrawRectangleRec(Rectangle {
        bounds.x + 4.0F, bounds.y + 5.0F,
        bounds.width, bounds.height }, Color { 0, 0, 0, 110 });
    DrawRectangleRec(bounds, HUD_BORDER);
    DrawRectangleRec(Rectangle {
        bounds.x + 1.0F, bounds.y + 1.0F,
        bounds.width - 2.0F, bounds.height - 2.0F }, HUD_SURFACE);
    DrawRectangle(static_cast<int>(bounds.x + 1.0F),
        static_cast<int>(bounds.y + 8.0F), 3,
        static_cast<int>(bounds.height - 16.0F), accent);
    DrawRectangle(static_cast<int>(bounds.x + 10.0F),
        static_cast<int>(bounds.y + 1.0F),
        static_cast<int>(std::min(bounds.width * 0.22F, 74.0F)), 2, accent);

    constexpr float corner = 7.0F;
    const Color bracket { accent.r, accent.g, accent.b, 190 };
    DrawLineEx(Vector2 { bounds.x + bounds.width - corner, bounds.y + 1.0F },
        Vector2 { bounds.x + bounds.width - 1.0F, bounds.y + 1.0F },
        1.0F, bracket);
    DrawLineEx(Vector2 { bounds.x + bounds.width - 1.0F, bounds.y + 1.0F },
        Vector2 { bounds.x + bounds.width - 1.0F, bounds.y + corner },
        1.0F, bracket);
    DrawLineEx(Vector2 { bounds.x + bounds.width - corner,
                   bounds.y + bounds.height - 1.0F },
        Vector2 { bounds.x + bounds.width - 1.0F,
            bounds.y + bounds.height - 1.0F }, 1.0F, bracket);
}

} // namespace game_render
