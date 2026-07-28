#include "weapon.hpp"

#include <algorithm>

namespace {

void updateWeaponState(Weapon& weapon, ProjectilePool& projectiles,
    const Player& player, bool fireHeld, float stepTime,
    std::span<const Segment2D> walls, bool rejectBlockedMuzzle)
{
    if (fireHeld && weapon.cooldownRemaining <= 0.0F) {
        const Vector2 playerPosition {
            player.position.x,
            player.position.z
        };
        const Vector2 muzzlePosition {
            player.position.x + player.facing.x * WEAPON_MUZZLE_DISTANCE,
            player.position.z + player.facing.y * WEAPON_MUZZLE_DISTANCE
        };
        const bool muzzleBlocked = rejectBlockedMuzzle
            && earliestCircleSegmentHit(playerPosition, muzzlePosition,
                projectiles.profile().radius, walls)
                .has_value();
        // A blocked muzzle or full pool drops the shot but consumes cooldown.
        if (!muzzleBlocked) {
            projectiles.spawn(muzzlePosition, player.facing);
        }
        weapon.cooldownRemaining = WEAPON_FIRE_INTERVAL;
    }

    weapon.cooldownRemaining = std::max(
        0.0F, weapon.cooldownRemaining - stepTime);
}

} // namespace

void updateWeapon(Weapon& weapon, ProjectilePool& projectiles,
    const Player& player, bool fireHeld, float stepTime)
{
    updateWeaponState(weapon, projectiles, player,
        fireHeld, stepTime, {}, false);
}

void updateWeapon(Weapon& weapon, ProjectilePool& projectiles,
    const Player& player, bool fireHeld, float stepTime,
    std::span<const Segment2D> walls)
{
    updateWeaponState(weapon, projectiles, player,
        fireHeld, stepTime, walls, true);
}
