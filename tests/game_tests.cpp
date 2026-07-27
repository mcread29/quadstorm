#include "game/projectile_pool.hpp"
#include "game/weapon.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace {

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool nearlyEqual(float left, float right)
{
    return std::abs(left - right) <= 0.0001F;
}

bool projectilesAdvanceAndInterpolate()
{
    ProjectilePool pool;
    bool valid = check(pool.spawn(Vector2 { 1.0F, 2.0F },
                           Vector2 { 3.0F, 4.0F }),
        "a valid projectile spawns");

    pool.update(0.25F);
    const Projectile& projectile = pool.projectiles().front();
    valid &= check(nearlyEqual(projectile.previousPosition.x, 1.0F)
            && nearlyEqual(projectile.previousPosition.y, 2.0F),
        "projectile keeps its previous simulation position");
    valid &= check(nearlyEqual(projectile.position.x, 4.3F)
            && nearlyEqual(projectile.position.y, 6.4F),
        "projectile advances at its normalized fixed speed");

    const Vector2 midpoint = interpolateProjectilePosition(projectile, 0.5F);
    valid &= check(nearlyEqual(midpoint.x, 2.65F)
            && nearlyEqual(midpoint.y, 4.2F),
        "projectile render position interpolates between fixed states");
    return valid;
}

bool poolExhaustionAndReuseAreSafe()
{
    ProjectilePool pool;
    bool valid = true;
    for (std::size_t index = 0; index < PROJECTILE_POOL_CAPACITY; ++index) {
        valid &= check(pool.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F }),
            "each preallocated projectile slot can be occupied");
    }
    valid &= check(pool.activeCount() == PROJECTILE_POOL_CAPACITY,
        "the full pool reports every active slot");
    valid &= check(!pool.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F }),
        "pool exhaustion drops a projectile safely");

    pool.update(PROJECTILE_LIFETIME);
    valid &= check(pool.activeCount() == 0,
        "projectiles deactivate when their lifetime expires");
    valid &= check(pool.spawn(Vector2 { 7.0F, 8.0F }, Vector2 { 0.0F, 1.0F }),
        "an expired slot can be reused");
    valid &= check(pool.projectiles().front().active
            && nearlyEqual(pool.projectiles().front().position.x, 7.0F)
            && nearlyEqual(pool.projectiles().front().position.y, 8.0F),
        "reuse starts at the requested position");
    return valid;
}

bool weaponUsesMuzzleAndFixedCadence()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Player player;
    player.position = Vector3 { 2.0F, PLAYER_RADIUS, 3.0F };
    player.facing = Vector2 { 0.0F, -1.0F };

    Weapon weapon;
    ProjectilePool pool;
    updateWeapon(weapon, pool, player, true, fixedStep);

    bool valid = check(pool.activeCount() == 1,
        "holding fire emits immediately when the weapon is ready");
    const Projectile& first = pool.projectiles().front();
    valid &= check(nearlyEqual(first.position.x, 2.0F)
            && nearlyEqual(first.position.y, 3.0F - WEAPON_MUZZLE_DISTANCE),
        "weapon emits from the facing marker endpoint");

    for (int step = 1; step < 120; ++step) {
        updateWeapon(weapon, pool, player, true, fixedStep);
    }
    valid &= check(pool.activeCount() == 10,
        "one second of held fire follows the fixed ten-shot cadence");
    return valid;
}

} // namespace

int main()
{
    bool valid = true;
    valid &= projectilesAdvanceAndInterpolate();
    valid &= poolExhaustionAndReuseAreSafe();
    valid &= weaponUsesMuzzleAndFixedCadence();
    return valid ? 0 : 1;
}
