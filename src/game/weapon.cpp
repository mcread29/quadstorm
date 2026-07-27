#include "weapon.hpp"

#include <algorithm>

void updateWeapon(Weapon& weapon, ProjectilePool& projectiles,
    const Player& player, bool fireHeld, float stepTime)
{
    if (fireHeld && weapon.cooldownRemaining <= 0.0F) {
        const Vector2 muzzlePosition {
            player.position.x + player.facing.x * WEAPON_MUZZLE_DISTANCE,
            player.position.z + player.facing.y * WEAPON_MUZZLE_DISTANCE
        };
        // A full pool drops this shot and still consumes the cooldown interval.
        projectiles.spawn(muzzlePosition, player.facing);
        weapon.cooldownRemaining = WEAPON_FIRE_INTERVAL;
    }

    weapon.cooldownRemaining = std::max(
        0.0F, weapon.cooldownRemaining - stepTime);
}
