#pragma once

#include "raylib.h"

namespace game_render {

void drawVoidGrid();
void drawRadialFloorDecal(Vector2 center, float radius,
    int spokes, Color color);
void drawWorldReticle(Vector3 aimPoint);

} // namespace game_render
