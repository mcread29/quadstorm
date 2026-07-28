#pragma once

#include "player.hpp"
#include "projectile_pool.hpp"

#include "raylib.h"

#include <array>
#include <span>

struct WallSegment {
    Vector2 start {};
    Vector2 end {};
};

inline constexpr float ARENA_HALF_EXTENT = 10.0F;
inline constexpr std::array<WallSegment, 4> ARENA_WALLS {
    WallSegment {
        Vector2 { -ARENA_HALF_EXTENT, -ARENA_HALF_EXTENT },
        Vector2 { ARENA_HALF_EXTENT, -ARENA_HALF_EXTENT } },
    WallSegment {
        Vector2 { ARENA_HALF_EXTENT, -ARENA_HALF_EXTENT },
        Vector2 { ARENA_HALF_EXTENT, ARENA_HALF_EXTENT } },
    WallSegment {
        Vector2 { ARENA_HALF_EXTENT, ARENA_HALF_EXTENT },
        Vector2 { -ARENA_HALF_EXTENT, ARENA_HALF_EXTENT } },
    WallSegment {
        Vector2 { -ARENA_HALF_EXTENT, ARENA_HALF_EXTENT },
        Vector2 { -ARENA_HALF_EXTENT, -ARENA_HALF_EXTENT } },
};

void resolvePlayerWallCollisions(Player& player, Vector2 previousPosition,
    std::span<const WallSegment> walls);
void resolveProjectileWallCollisions(ProjectilePool& projectiles,
    std::span<const WallSegment> walls);
