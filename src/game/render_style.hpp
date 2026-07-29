#pragma once

#include "generated_level.hpp"
#include "horde_match.hpp"

#include "raylib.h"

namespace game_render {

inline constexpr float WALL_HEIGHT = 2.15F;
inline constexpr float WALL_THICKNESS = 0.24F;
inline constexpr Color GENERATED_BACKGROUND { 8, 14, 20, 255 };
inline constexpr Color ARENA_BACKGROUND { 10, 16, 22, 255 };
inline constexpr Color HUD_SURFACE { 6, 13, 19, 238 };
inline constexpr Color HUD_BORDER { 66, 96, 101, 180 };
inline constexpr Color ENERGY_CYAN { 76, 224, 226, 255 };
inline constexpr Color MACHINE_GOLD { 238, 171, 62, 255 };

Color roomRoleColor(stalberg::rooms::RoomRole role);
const char* smallMapRecipeName(stalberg::rooms::SmallMapRecipe recipe);
const char* roundPhaseName(RoundPhase phase);
const char* roomRoleName(stalberg::rooms::RoomRole role);
Color generatedFloorColor(const GeneratedLevel& level,
    int region, stalberg::rooms::CellIndex cell);
void drawHudPanel(Rectangle bounds, Color accent);

} // namespace game_render
