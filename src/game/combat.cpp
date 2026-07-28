#include "combat.hpp"

#include "arena.hpp"
#include "target.hpp"

namespace {

void freezeProjectileInterpolation(ProjectilePool& projectiles)
{
    for (Projectile& projectile : projectiles.projectiles()) {
        projectile.previousPosition = projectile.position;
    }
}

void freezeCombatInterpolation(CombatState& combat)
{
    combat.enemy.previousPosition = combat.enemy.position;
    freezeProjectileInterpolation(combat.playerProjectiles);
    freezeProjectileInterpolation(combat.enemyProjectiles);
}

CombatStepResult updateCombatState(CombatState& combat, Player& player,
    Target* target, const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls)
{
    CombatStepResult result;
    if (!isPlayerAlive(player) || !isEnemyAlive(combat.enemy)) {
        if (!isPlayerAlive(player)) {
            const Vector2 playerPosition {
                player.position.x,
                player.position.z
            };
            updatePlayerDamage(player, combat.enemyProjectiles,
                playerPosition, stepTime);
        }
        freezeCombatInterpolation(combat);
        return result;
    }

    const Vector2 previousPlayerPosition {
        player.position.x,
        player.position.z
    };
    updatePlayer(player, input, stepTime);
    resolvePlayerWallCollisions(player, previousPlayerPosition, walls);

    updateWeapon(combat.weapon, combat.playerProjectiles,
        player, input.fireHeld, stepTime, walls);
    combat.playerProjectiles.update(stepTime);
    resolveProjectileWallCollisions(combat.playerProjectiles, walls);

    const Vector2 playerPosition {
        player.position.x,
        player.position.z
    };
    updateEnemyMovement(combat.enemy, playerPosition, stepTime);
    resolveCircleWallCollisions(combat.enemy.position,
        combat.enemy.velocity, ENEMY_RADIUS,
        combat.enemy.previousPosition, walls);
    result.enemyDamage = updateEnemyDamage(
        combat.enemy, combat.playerProjectiles, stepTime);
    if (target != nullptr) {
        updateTarget(*target, combat.playerProjectiles, stepTime);
    }
    if (result.enemyDamage == EnemyDamageResult::died) {
        freezeCombatInterpolation(combat);
        return result;
    }

    result.enemyFired = updateEnemyPattern(combat.enemy,
        combat.enemyProjectiles, playerPosition, stepTime, walls);
    combat.enemyProjectiles.update(stepTime);
    resolveProjectileWallCollisions(combat.enemyProjectiles, walls);
    result.playerDamage = updatePlayerDamage(player,
        combat.enemyProjectiles, previousPlayerPosition, stepTime);
    if (result.playerDamage == PlayerDamageResult::died) {
        freezeCombatInterpolation(combat);
    }
    return result;
}

} // namespace

void resetCombat(CombatState& combat, Vector2 enemyPosition)
{
    combat = CombatState {};
    combat.enemy.position = enemyPosition;
    combat.enemy.previousPosition = enemyPosition;
}

CombatStepResult updateCombat(CombatState& combat, Player& player,
    const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls)
{
    return updateCombatState(combat, player, nullptr, input, stepTime, walls);
}

CombatStepResult updateCombat(CombatState& combat, Player& player,
    Target& target, const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls)
{
    return updateCombatState(combat, player, &target, input, stepTime, walls);
}
