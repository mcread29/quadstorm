#pragma once

#include "player.hpp"
#include "projectile_pool.hpp"

inline constexpr float WEAPON_FIRE_INTERVAL = 0.1F;
inline constexpr float WEAPON_MUZZLE_DISTANCE = PLAYER_FACING_MARKER_DISTANCE;

struct Weapon {
    float cooldownRemaining = 0.0F;
};

void updateWeapon(Weapon& weapon, ProjectilePool& projectiles,
    const Player& player, bool fireHeld, float stepTime);
