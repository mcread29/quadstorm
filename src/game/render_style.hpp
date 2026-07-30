#pragma once

#include "generated_level.hpp"
#include "horde_match.hpp"

#include "raylib.h"

namespace game_render {

inline constexpr float WALL_HEIGHT = 2.15F;
inline constexpr float WALL_THICKNESS = 0.24F;
inline constexpr Color GENERATED_BACKGROUND { 5, 10, 15, 255 };
inline constexpr Color ARENA_BACKGROUND { 7, 12, 17, 255 };
inline constexpr Color HUD_SURFACE { 5, 11, 16, 242 };
inline constexpr Color HUD_BORDER { 74, 101, 101, 205 };
inline constexpr Color ENERGY_CYAN { 68, 231, 224, 255 };
inline constexpr Color MACHINE_GOLD { 245, 177, 54, 255 };
inline constexpr float UI_CANVAS_WIDTH = 1280.0F;
inline constexpr float UI_CANVAS_HEIGHT = 800.0F;

void beginUiCanvas();
void endUiCanvas();
Color roomRoleColor(stalberg::rooms::RoomRole role);
const char* smallMapRecipeName(stalberg::rooms::SmallMapRecipe recipe);
const char* largeMapArchetypeName(stalberg::rooms::LargeMapArchetype archetype);
const char* roundPhaseName(RoundPhase phase);
const char* roomRoleName(stalberg::rooms::RoomRole role);
Color generatedFloorColor(const GeneratedLevel& level,
    int region, stalberg::rooms::CellIndex cell);
void drawHudPanel(Rectangle bounds, Color accent);

} // namespace game_render
