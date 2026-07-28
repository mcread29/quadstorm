#include "encounter.hpp"

#include "arena.hpp"

namespace {

void freezeProjectileInterpolation(ProjectilePool& projectiles)
{
    for (Projectile& projectile : projectiles.projectiles()) {
        projectile.previousPosition = projectile.position;
    }
}

void freezeEncounterInterpolation(Encounter& encounter)
{
    encounter.enemy.previousPosition = encounter.enemy.position;
    freezeProjectileInterpolation(encounter.playerProjectiles);
    freezeProjectileInterpolation(encounter.enemyProjectiles);
}

} // namespace

void resetEncounter(Encounter& encounter)
{
    encounter = Encounter {};
}

EncounterStepResult updateEncounter(
    Encounter& encounter, const PlayerInput& input, float stepTime)
{
    EncounterStepResult result;
    if (!isPlayerAlive(encounter.player)
        || !isEnemyAlive(encounter.enemy)) {
        if (!isPlayerAlive(encounter.player)) {
            const Vector2 playerPosition {
                encounter.player.position.x,
                encounter.player.position.z
            };
            updatePlayerDamage(encounter.player, encounter.enemyProjectiles,
                playerPosition, stepTime);
        }
        freezeEncounterInterpolation(encounter);
        if (input.restartPressed) {
            resetEncounter(encounter);
            result.restarted = true;
        }
        return result;
    }

    const Vector2 previousPlayerPosition {
        encounter.player.position.x,
        encounter.player.position.z
    };
    updatePlayer(encounter.player, input, stepTime);
    resolvePlayerWallCollisions(
        encounter.player, previousPlayerPosition, ARENA_WALLS);

    updateWeapon(encounter.weapon, encounter.playerProjectiles,
        encounter.player, input.fireHeld, stepTime);
    encounter.playerProjectiles.update(stepTime);
    resolveProjectileWallCollisions(
        encounter.playerProjectiles, ARENA_WALLS);

    const Vector2 playerPosition {
        encounter.player.position.x,
        encounter.player.position.z
    };
    updateEnemyMovement(encounter.enemy, playerPosition, stepTime);
    resolveCircleWallCollisions(encounter.enemy.position,
        encounter.enemy.velocity, ENEMY_RADIUS,
        encounter.enemy.previousPosition, ARENA_WALLS);
    result.enemyDamage = updateEnemyDamage(
        encounter.enemy, encounter.playerProjectiles, stepTime);
    updateTarget(
        encounter.target, encounter.playerProjectiles, stepTime);
    if (result.enemyDamage == EnemyDamageResult::died) {
        freezeEncounterInterpolation(encounter);
        return result;
    }

    result.enemyFired = updateEnemyPattern(encounter.enemy,
        encounter.enemyProjectiles, playerPosition, stepTime);
    encounter.enemyProjectiles.update(stepTime);
    resolveProjectileWallCollisions(
        encounter.enemyProjectiles, ARENA_WALLS);
    result.playerDamage = updatePlayerDamage(encounter.player,
        encounter.enemyProjectiles, previousPlayerPosition, stepTime);
    if (result.playerDamage == PlayerDamageResult::died) {
        freezeEncounterInterpolation(encounter);
    }
    return result;
}
