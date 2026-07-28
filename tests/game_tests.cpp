#include "game/arena.hpp"
#include "game/collision_2d.hpp"
#include "game/combat.hpp"
#include "game/encounter.hpp"
#include "game/enemy.hpp"
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

bool projectileCollidesDuringFinalLifetimeStep()
{
    ProjectilePool pool(ProjectileProfile {
        .speed = 10.0F,
        .lifetime = 0.1F,
        .radius = 0.1F
    });
    Target target;
    target.position = Vector2 { 1.0F, 0.0F };
    bool valid = check(pool.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F }),
        "a final-step projectile can be staged");
    pool.update(0.1F);
    valid &= check(pool.activeCount() == 1,
        "lifetime expiry waits until collision processing completes");
    updateTarget(target, pool, 0.1F);
    pool.retireExpired();
    valid &= check(target.health == TARGET_MAX_HEALTH - 1,
        "a projectile can collide during its final valid timestep");
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
    valid &= check(pool.activeCount() == PROJECTILE_POOL_CAPACITY,
        "projectiles remain collidable during their final integrated timestep");
    pool.retireExpired();
    valid &= check(pool.activeCount() == 0,
        "expired projectiles retire after collision processing");
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

bool playerDashMovesQuicklyAndRespectsCooldown()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Player player;
    PlayerInput input;
    input.movement = Vector2 { 1.0F, 0.0F };
    input.dashPressed = true;

    updatePlayer(player, input, fixedStep);
    bool valid = check(nearlyEqual(
                           player.position.x, PLAYER_DASH_SPEED * fixedStep)
            && nearlyEqual(player.velocity.x, PLAYER_DASH_SPEED)
            && player.dashRemaining > 0.0F
            && nearlyEqual(
                player.dashCooldownRemaining, PLAYER_DASH_COOLDOWN),
        "dash starts immediately at dash speed");

    const float firstDashRemaining = player.dashRemaining;
    updatePlayer(player, input, fixedStep);
    valid &= check(player.dashRemaining < firstDashRemaining,
        "dash input cannot restart an active dash");

    input.dashPressed = false;
    const int recoverySteps = static_cast<int>(
        PLAYER_DASH_COOLDOWN / fixedStep) + 2;
    for (int step = 0; step < recoverySteps; ++step) {
        updatePlayer(player, input, fixedStep);
    }
    input.movement = Vector2 { 0.0F, 1.0F };
    input.dashPressed = true;
    updatePlayer(player, input, fixedStep);
    valid &= check(player.dashRemaining > 0.0F
            && nearlyEqual(player.dashDirection.x, 0.0F)
            && nearlyEqual(player.dashDirection.y, 1.0F),
        "dash becomes available again after its cooldown");
    return valid;
}

bool playerDashRespectsWallCollision()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    constexpr std::array walls {
        WallSegment { Vector2 { 1.0F, -5.0F }, Vector2 { 1.0F, 5.0F } }
    };
    Player player;
    PlayerInput input;
    input.movement = Vector2 { 1.0F, 0.0F };
    input.dashPressed = true;

    for (int step = 0; step < 30; ++step) {
        const Vector2 previousPosition { player.position.x, player.position.z };
        updatePlayer(player, input, fixedStep);
        resolvePlayerWallCollisions(player, previousPosition, walls);
        input.dashPressed = false;
    }

    return check(nearlyEqual(player.position.x, 1.0F - PLAYER_RADIUS),
               "dash cannot carry the player through a wall")
        && check(player.velocity.x <= 0.0001F,
            "dash wall contact removes velocity into the wall");
}

bool reusableCombatUpdatesACallerOwnedPlayer()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 1.0F, -5.0F }, Vector2 { 1.0F, 5.0F } }
    };
    Player player;
    player.health = PLAYER_MAX_HEALTH - 1;
    CombatState combat;
    resetCombat(combat, Vector2 { -5.0F, -5.0F });
    PlayerInput input;
    input.movement = Vector2 { 1.0F, 0.0F };

    updateCombat(combat, player, input, 0.1F, walls);

    return check(nearlyEqual(player.position.x, 1.0F - PLAYER_RADIUS)
            && player.health == PLAYER_MAX_HEALTH - 1,
               "reusable combat directly updates its caller-owned player")
        && check(nearlyEqual(combat.enemy.previousPosition.x, -5.0F),
            "reusable combat starts from an injected deterministic enemy spawn");
}

bool encounterUsesSuppliedWallGeometry()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 1.0F, -5.0F }, Vector2 { 1.0F, 5.0F } }
    };
    Encounter encounter;
    PlayerInput input;
    input.movement = Vector2 { 1.0F, 0.0F };

    updateEncounter(encounter, input, 0.1F, walls);

    return check(nearlyEqual(encounter.player.position.x,
                     1.0F - PLAYER_RADIUS),
               "encounter player collision uses supplied wall geometry")
        && check(nearlyEqual(encounter.player.velocity.x, 0.0F),
            "supplied encounter wall removes inward player velocity");
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

bool enemyMovementAndPatternAreDeterministic()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Enemy first;
    Enemy second;
    ProjectilePool firstProjectiles(ENEMY_PROJECTILE_PROFILE);
    ProjectilePool secondProjectiles(ENEMY_PROJECTILE_PROFILE);
    constexpr Vector2 playerPosition { -1.0F, 2.0F };

    for (int step = 0; step < 360; ++step) {
        updateEnemyMovement(first, playerPosition, fixedStep);
        updateEnemyMovement(second, playerPosition, fixedStep);
        updateEnemyPattern(
            first, firstProjectiles, playerPosition, fixedStep);
        updateEnemyPattern(
            second, secondProjectiles, playerPosition, fixedStep);
        firstProjectiles.update(fixedStep);
        secondProjectiles.update(fixedStep);
    }

    return check(nearlyEqual(first.position.x, second.position.x)
            && nearlyEqual(first.position.y, second.position.y)
            && firstProjectiles.activeCount()
                == secondProjectiles.activeCount(),
        "enemy movement and firing repeat exactly from the same state")
        && check(!nearlyEqual(first.position.x, 5.0F)
                || !nearlyEqual(first.position.y, 5.0F),
            "the deterministic enemy advances around the player")
        && check(firstProjectiles.activeCount() == 6,
            "three seconds produces two complete three-shot fans");
}

bool projectilePoolsKeepOwnershipSeparate()
{
    Encounter encounter;
    const bool playerSpawned = encounter.combat.playerProjectiles.spawn(
        Vector2 {}, Vector2 { 1.0F, 0.0F });
    const bool enemySpawned = encounter.combat.enemyProjectiles.spawn(
        Vector2 {}, Vector2 { 1.0F, 0.0F });

    return check(playerSpawned && enemySpawned,
               "both projectile owners can use their own pool")
        && check(nearlyEqual(encounter.combat.playerProjectiles.profile().speed,
                     PLAYER_PROJECTILE_SPEED)
                && nearlyEqual(encounter.combat.enemyProjectiles.profile().speed,
                    ENEMY_PROJECTILE_SPEED),
            "projectile ownership keeps player and enemy profiles separate")
        && check(encounter.combat.playerProjectiles.activeCount() == 1
                && encounter.combat.enemyProjectiles.activeCount() == 1,
            "spawning for one owner does not consume the other owner's slots");
}

bool playerProjectilesDamageAndDefeatEnemy()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Enemy enemy;
    enemy.previousPosition = Vector2 { 3.0F, 5.0F };
    enemy.position = Vector2 { 7.0F, 5.0F };
    ProjectilePool playerProjectiles;
    bool valid = check(playerProjectiles.spawn(
                           Vector2 { 5.0F, 5.0F }, Vector2 { 1.0F, 0.0F }),
        "a player projectile spawns for enemy damage testing");
    Projectile& crossingProjectile = playerProjectiles.projectiles().front();
    crossingProjectile.previousPosition = Vector2 { 5.0F, 5.0F };
    crossingProjectile.position = Vector2 { 5.0F, 5.0F };

    const EnemyDamageResult movingHit = updateEnemyDamage(
        enemy, playerProjectiles, fixedStep);
    valid &= check(movingHit == EnemyDamageResult::hit
            && enemy.health == ENEMY_MAX_HEALTH - 1
            && playerProjectiles.activeCount() == 0,
        "relative swept collision damages a moving enemy and consumes the shot");

    Encounter encounter;
    encounter.combat.enemy.health = 1;
    encounter.player.invulnerabilityRemaining = 0.5F;
    playerProjectiles.spawn(
        encounter.combat.enemy.position, Vector2 { 1.0F, 0.0F });
    encounter.combat.playerProjectiles = playerProjectiles;
    const EncounterStepResult defeated = updateEncounter(
        encounter, PlayerInput {}, fixedStep);
    valid &= check(defeated.enemyDamage == EnemyDamageResult::died
            && !isEnemyAlive(encounter.combat.enemy),
        "the enemy enters a defeated state when its final health is removed");
    valid &= check(nearlyEqual(encounter.player.invulnerabilityRemaining,
                       0.5F - fixedStep),
        "player effects advance on the enemy-defeat simulation step");
    updateEncounter(encounter, PlayerInput {}, fixedStep);
    valid &= check(nearlyEqual(encounter.player.invulnerabilityRemaining,
                       0.5F - fixedStep * 2.0F),
        "player effects continue advancing after victory");

    PlayerInput restartInput;
    restartInput.restartPressed = true;
    const EncounterStepResult restarted = updateEncounter(
        encounter, restartInput, fixedStep);
    valid &= check(restarted.restarted
            && encounter.combat.enemy.health == ENEMY_MAX_HEALTH
            && encounter.combat.playerProjectiles.activeCount() == 0,
        "post-victory restart restores enemy health and clears shots");
    return valid;
}

bool playerDamageRespectsInvulnerability()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Player player;
    ProjectilePool hostileProjectiles(ENEMY_PROJECTILE_PROFILE);
    bool valid = check(hostileProjectiles.spawn(
                           Vector2 {}, Vector2 { 1.0F, 0.0F }),
        "a hostile projectile spawns for damage testing");
    const PlayerDamageResult firstHit = updatePlayerDamage(
        player, hostileProjectiles, Vector2 {}, fixedStep);
    valid &= check(firstHit == PlayerDamageResult::hit
            && player.health == PLAYER_MAX_HEALTH - 1
            && hostileProjectiles.activeCount() == 0,
        "a hostile projectile damages once and deactivates");

    hostileProjectiles.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F });
    const PlayerDamageResult blockedHit = updatePlayerDamage(
        player, hostileProjectiles, Vector2 {}, fixedStep);
    valid &= check(blockedHit == PlayerDamageResult::none
            && player.health == PLAYER_MAX_HEALTH - 1
            && hostileProjectiles.activeCount() == 0,
        "invulnerability consumes overlapping shots without repeated damage");

    const int recoverySteps = static_cast<int>(
        PLAYER_INVULNERABILITY_DURATION / fixedStep) + 2;
    for (int step = 0; step < recoverySteps; ++step) {
        updatePlayerEffects(player, fixedStep);
        updatePlayerDamage(
            player, hostileProjectiles, Vector2 {}, fixedStep);
    }
    hostileProjectiles.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F });
    const PlayerDamageResult recoveredHit = updatePlayerDamage(
        player, hostileProjectiles, Vector2 {}, fixedStep);
    valid &= check(recoveredHit == PlayerDamageResult::hit
            && player.health == PLAYER_MAX_HEALTH - 2,
        "damage resumes after invulnerability expires");
    return valid;
}

bool movingPlayerSweepsAgainstHostileProjectiles()
{
    Player player;
    player.position.x = 1.2F;
    ProjectilePool hostileProjectiles(ENEMY_PROJECTILE_PROFILE);
    bool valid = check(hostileProjectiles.spawn(
                           Vector2 {}, Vector2 { 1.0F, 0.0F }),
        "a stationary hostile projectile spawns for relative sweep testing");
    Projectile& projectile = hostileProjectiles.projectiles().front();
    projectile.previousPosition = Vector2 {};
    projectile.position = Vector2 {};

    const PlayerDamageResult result = updatePlayerDamage(player,
        hostileProjectiles, Vector2 { -1.2F, 0.0F }, 0.0F);
    valid &= check(result == PlayerDamageResult::hit
            && player.health == PLAYER_MAX_HEALTH - 1,
        "relative swept collision catches a player crossing a hostile shot");
    return valid;
}

bool deathAndRestartResetEncounter()
{
    constexpr float fixedStep = 1.0F / 120.0F;
    Encounter encounter;
    encounter.player.health = 1;
    encounter.combat.enemyProjectiles.spawn(
        Vector2 {}, Vector2 { 1.0F, 0.0F });
    encounter.combat.enemyProjectiles.spawn(
        Vector2 { 5.0F, 0.0F }, Vector2 { 1.0F, 0.0F });

    const EncounterStepResult death = updateEncounter(
        encounter, PlayerInput {}, fixedStep);
    bool valid = check(death.playerDamage == PlayerDamageResult::died
            && !isPlayerAlive(encounter.player),
        "the final hit enters the defeated state");
    const Projectile& frozenProjectile
        = encounter.combat.enemyProjectiles.projectiles()[1];
    valid &= check(frozenProjectile.active
            && nearlyEqual(frozenProjectile.previousPosition.x,
                frozenProjectile.position.x)
            && nearlyEqual(frozenProjectile.previousPosition.y,
                frozenProjectile.position.y),
        "defeat freezes surviving projectile interpolation state");

    PlayerInput restartInput;
    restartInput.restartPressed = true;
    const EncounterStepResult restart = updateEncounter(
        encounter, restartInput, fixedStep);
    valid &= check(restart.restarted && isPlayerAlive(encounter.player)
            && encounter.player.health == PLAYER_MAX_HEALTH,
        "explicit restart restores player life and health");
    valid &= check(encounter.combat.playerProjectiles.activeCount() == 0
            && encounter.combat.enemyProjectiles.activeCount() == 0
            && nearlyEqual(encounter.combat.weapon.cooldownRemaining, 0.0F)
            && encounter.target.health == TARGET_MAX_HEALTH
            && nearlyEqual(encounter.combat.enemy.position.x, 5.0F)
            && nearlyEqual(encounter.combat.enemy.position.y, 5.0F)
            && nearlyEqual(encounter.combat.enemy.shotCooldownRemaining,
                ENEMY_FIRST_SHOT_DELAY),
        "restart deterministically clears projectiles and combat state");
    return valid;
}

bool blockedMuzzlesDropShotsAndConsumeCooldown()
{
    constexpr std::array walls {
        WallSegment { Vector2 { 0.8F, -2.0F }, Vector2 { 0.8F, 2.0F } }
    };
    Player player;
    player.facing = Vector2 { 1.0F, 0.0F };
    Weapon weapon;
    ProjectilePool playerProjectiles;
    updateWeapon(weapon, playerProjectiles, player,
        true, 1.0F / 120.0F, walls);

    Enemy enemy;
    enemy.position = Vector2 {};
    enemy.previousPosition = enemy.position;
    enemy.shotCooldownRemaining = 0.0F;
    ProjectilePool enemyProjectiles(ENEMY_PROJECTILE_PROFILE);
    const bool enemyFired = updateEnemyPattern(enemy, enemyProjectiles,
        Vector2 { 5.0F, 0.0F }, 1.0F / 120.0F, walls);

    return check(playerProjectiles.activeCount() == 0
            && weapon.cooldownRemaining > 0.0F,
               "a wall-blocked player muzzle drops the shot and consumes cooldown")
        && check(!enemyFired && enemyProjectiles.activeCount() == 0
                && enemy.shotCooldownRemaining > 0.0F,
            "a wall-blocked enemy muzzle cannot emit through generated walls");
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
    valid &= projectileCollidesDuringFinalLifetimeStep();
    valid &= invalidProjectileProfilesCannotSpawn();
    valid &= poolExhaustionAndReuseAreSafe();
    valid &= playerStopsAtWallFaces();
    valid &= playerResolvesWallEndpoints();
    valid &= playerResolvesCornersIteratively();
    valid &= playerSlidesAlongWalls();
    valid &= playerDashMovesQuicklyAndRespectsCooldown();
    valid &= playerDashRespectsWallCollision();
    valid &= reusableCombatUpdatesACallerOwnedPlayer();
    valid &= encounterUsesSuppliedWallGeometry();
    valid &= projectileWallEndpointsAreSolid();
    valid &= fastProjectilesHitTheFirstWall();
    valid &= outwardMuzzleProjectilesDoNotEscape();
    valid &= enemyMovementAndPatternAreDeterministic();
    valid &= projectilePoolsKeepOwnershipSeparate();
    valid &= playerProjectilesDamageAndDefeatEnemy();
    valid &= playerDamageRespectsInvulnerability();
    valid &= movingPlayerSweepsAgainstHostileProjectiles();
    valid &= deathAndRestartResetEncounter();
    valid &= blockedMuzzlesDropShotsAndConsumeCooldown();
    valid &= weaponUsesMuzzleAndFixedCadence();
    return valid ? 0 : 1;
}
