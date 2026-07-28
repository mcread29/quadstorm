#include "game/arena.hpp"
#include "game/collision_2d.hpp"
#include "game/projectile_pool.hpp"
#include "game/weapon.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
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

bool sweptCircleQueriesReturnFirstContact()
{
    const auto hitAmount = sweepCircleAgainstCircle(
        Vector2 { -5.0F, 0.0F }, Vector2 { 5.0F, 0.0F }, 0.5F,
        Vector2 {}, 1.0F);
    const auto missAmount = sweepCircleAgainstCircle(
        Vector2 { -5.0F, 2.0F }, Vector2 { 5.0F, 2.0F }, 0.5F,
        Vector2 {}, 1.0F);
    const auto grazingHit = sweepCircleAgainstCircle(
        Vector2 { -17.838455F, 11.300903F },
        Vector2 { -53.980194F, 9.517175F }, 0.16F,
        Vector2 { -45.974655F, 10.9235F }, 0.85F);
    const auto initialOverlap = sweepCircleAgainstCircle(
        Vector2 { 0.5F, 0.0F }, Vector2 { 0.5F, 0.0F }, 0.5F,
        Vector2 {}, 0.5F);

    return check(hitAmount.has_value() && nearlyEqual(*hitAmount, 0.35F),
               "swept circles report their first contact amount")
        && check(!missAmount.has_value(),
            "separated swept circles do not report a hit")
        && check(grazingHit.has_value(),
            "long shallow circle sweeps preserve grazing contacts")
        && check(initialOverlap.has_value()
                && nearlyEqual(*initialOverlap, 0.0F),
            "zero-length initial overlaps report immediate contact");
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

bool projectileProfilesControlSimulationAndCollision()
{
    constexpr ProjectileProfile profile {
        .speed = 8.0F,
        .lifetime = 0.5F,
        .radius = 0.4F
    };
    constexpr std::array walls {
        WallSegment { Vector2 { 1.5F, -2.0F }, Vector2 { 1.5F, 2.0F } }
    };
    ProjectilePool pool(profile);
    bool valid = check(pool.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F }),
        "a projectile with a valid custom profile spawns");

    pool.update(0.25F);
    const Projectile& advanced = pool.projectiles().front();
    valid &= check(nearlyEqual(advanced.position.x, 2.0F)
            && nearlyEqual(advanced.remainingLifetime, 0.25F),
        "projectile speed and lifetime come from its pool profile");

    resolveProjectileWallCollisions(pool, walls);
    const Projectile& collided = pool.projectiles().front();
    valid &= check(!collided.active
            && nearlyEqual(collided.position.x, 1.1F),
        "wall collision uses the projectile pool profile radius");
    return valid;
}

bool invalidProjectileProfilesCannotSpawn()
{
    ProjectilePool pool(ProjectileProfile {
        .speed = std::numeric_limits<float>::infinity(),
        .lifetime = 1.0F,
        .radius = 0.2F
    });
    return check(!pool.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F }),
        "non-finite projectile profiles cannot create invalid simulation state");
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

    pool.update(PLAYER_PROJECTILE_LIFETIME);
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

bool playerStopsAtWallFaces()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 0.0F, -5.0F }, Vector2 { 0.0F, 5.0F } }
    };
    Player player;
    player.position = Vector3 { -0.4F, PLAYER_RADIUS, 0.0F };
    player.velocity = Vector2 { 2.0F, 0.0F };

    resolvePlayerWallCollisions(player, Vector2 { -1.0F, 0.0F }, walls);

    return check(nearlyEqual(player.position.x, -PLAYER_RADIUS),
               "player circle is pushed out of a wall face")
        && check(nearlyEqual(player.velocity.x, 0.0F),
            "wall face removes velocity into the contact");
}

bool playerResolvesWallEndpoints()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 0.0F, 0.0F }, Vector2 { 3.0F, 0.0F } }
    };
    Player player;
    player.position = Vector3 { -0.3F, PLAYER_RADIUS, -0.3F };
    player.velocity = Vector2 { 1.0F, 0.0F };

    resolvePlayerWallCollisions(player, Vector2 { -1.0F, -1.0F }, walls);

    const float endpointDistance = std::sqrt(
        player.position.x * player.position.x
        + player.position.z * player.position.z);
    const Vector2 endpointNormal {
        player.position.x / endpointDistance,
        player.position.z / endpointDistance
    };
    return check(nearlyEqual(endpointDistance, PLAYER_RADIUS),
               "player circle is pushed out of a wall endpoint")
        && check(nearlyEqual(player.velocity.x * endpointNormal.x
                    + player.velocity.y * endpointNormal.y,
                   0.0F),
            "endpoint contact removes only inward velocity");
}

bool playerResolvesCornersIteratively()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 0.0F, -5.0F }, Vector2 { 0.0F, 5.0F } },
        WallSegment { Vector2 { -5.0F, 0.0F }, Vector2 { 5.0F, 0.0F } }
    };
    Player player;
    player.position = Vector3 { -0.4F, PLAYER_RADIUS, -0.4F };
    player.velocity = Vector2 { 2.0F, 2.0F };

    resolvePlayerWallCollisions(player, Vector2 { -1.0F, -1.0F }, walls);

    return check(nearlyEqual(player.position.x, -PLAYER_RADIUS)
            && nearlyEqual(player.position.z, -PLAYER_RADIUS),
               "iterative contacts push the player out of a corner")
        && check(nearlyEqual(player.velocity.x, 0.0F)
                && nearlyEqual(player.velocity.y, 0.0F),
            "corner contacts remove velocity into both walls");
}

bool playerSlidesAlongWalls()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 0.0F, -5.0F }, Vector2 { 0.0F, 5.0F } }
    };
    Player player;
    player.position = Vector3 { -0.4F, PLAYER_RADIUS, 0.2F };
    player.velocity = Vector2 { 2.0F, 3.0F };

    resolvePlayerWallCollisions(player, Vector2 { -1.0F, 0.0F }, walls);

    return check(nearlyEqual(player.velocity.x, 0.0F),
               "sliding removes velocity into the wall")
        && check(nearlyEqual(player.velocity.y, 3.0F),
            "sliding preserves tangential velocity");
}

bool projectileWallEndpointsAreSolid()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 0.0F, -2.0F }, Vector2 { 0.0F, 2.0F } }
    };
    ProjectilePool pool;
    bool valid = check(pool.spawn(
                           Vector2 { -5.0F, 2.1F }, Vector2 { 1.0F, 0.0F }),
        "endpoint-collision projectile spawns");
    pool.update(0.5F);
    resolveProjectileWallCollisions(pool, walls);

    const Projectile& projectile = pool.projectiles().front();
    valid &= check(!projectile.active,
        "swept wall collision includes endpoint circles");
    valid &= check(projectile.position.x < 0.0F,
        "endpoint collision clips to the near side of contact");
    return valid;
}

bool fastProjectilesHitTheFirstWall()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 2.0F, -2.0F }, Vector2 { 2.0F, 2.0F } },
        WallSegment { Vector2 { 0.0F, -2.0F }, Vector2 { 0.0F, 2.0F } }
    };
    ProjectilePool pool;
    bool valid = check(pool.spawn(
                           Vector2 { -5.0F, 0.0F }, Vector2 { 1.0F, 0.0F }),
        "fast wall-collision projectile spawns");
    pool.update(0.5F);
    resolveProjectileWallCollisions(pool, walls);

    const Projectile& projectile = pool.projectiles().front();
    valid &= check(!projectile.active,
        "swept wall collision catches a fast projectile");
    valid &= check(nearlyEqual(
            projectile.position.x, -PLAYER_PROJECTILE_RADIUS),
        "projectile stops at the earliest wall regardless of array order");
    return valid;
}

bool outwardMuzzleProjectilesDoNotEscape()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Player player;
    player.position = Vector3 {
        ARENA_HALF_EXTENT - PLAYER_RADIUS,
        PLAYER_RADIUS,
        0.0F
    };
    player.facing = Vector2 { 1.0F, 0.0F };
    Weapon weapon;
    ProjectilePool pool;

    updateWeapon(weapon, pool, player, true, fixedStep);
    pool.update(fixedStep);
    resolveProjectileWallCollisions(pool, ARENA_WALLS);

    return check(pool.activeCount() == 0,
        "a muzzle beyond the closed arena cannot emit an escaping projectile");
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
    valid &= sweptCircleQueriesReturnFirstContact();
    valid &= projectilesAdvanceAndInterpolate();
    valid &= projectileProfilesControlSimulationAndCollision();
    valid &= invalidProjectileProfilesCannotSpawn();
    valid &= poolExhaustionAndReuseAreSafe();
    valid &= playerStopsAtWallFaces();
    valid &= playerResolvesWallEndpoints();
    valid &= playerResolvesCornersIteratively();
    valid &= playerSlidesAlongWalls();
    valid &= projectileWallEndpointsAreSolid();
    valid &= fastProjectilesHitTheFirstWall();
    valid &= outwardMuzzleProjectilesDoNotEscape();
    valid &= weaponUsesMuzzleAndFixedCadence();
    return valid ? 0 : 1;
}
