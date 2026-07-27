#pragma once

#include "projectile_pool.hpp"

#include "raylib.h"

inline constexpr float TARGET_RADIUS = 0.85F;
inline constexpr int TARGET_MAX_HEALTH = 5;
inline constexpr float TARGET_HIT_FLASH_DURATION = 0.16F;
inline constexpr float TARGET_RESET_DELAY = 1.0F;

struct Target {
    Vector2 position { -5.0F, -5.0F };
    int health = TARGET_MAX_HEALTH;
    float hitFlashRemaining = 0.0F;
    float resetRemaining = 0.0F;
};

void updateTarget(Target& target, ProjectilePool& projectiles, float stepTime);
