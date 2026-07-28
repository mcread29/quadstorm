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
    freezeProjectileInterpolation(combat.playerAttack.projectiles);
    freezeProjectileInterpolation(combat.enemyProjectiles);
}

CombatStepResult updateCombatState(CombatState& combat, Player& player,
    Target* target, const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls)
{
    CombatStepResult result;
    updatePlayerEffects(player, stepTime);
    if (!isPlayerAlive(player) || !isEnemyAlive(combat.enemy)) {
        freezeCombatInterpolation(combat);
        return result;
    }

    const Vector2 previousPlayerPosition {
        player.position.x,
        player.position.z
    };
    updatePlayer(player, input, stepTime);
    resolvePlayerWallCollisions(player, previousPlayerPosition, walls);

    updatePlayerAttack(combat.playerAttack,
        player, input.fireHeld, stepTime, walls);

    const Vector2 playerPosition {
        player.position.x,
        player.position.z
    };
    updateEnemyMovement(combat.enemy, playerPosition, stepTime);
    resolveCircleWallCollisions(combat.enemy.position,
        combat.enemy.velocity, ENEMY_RADIUS,
        combat.enemy.previousPosition, walls);
    result.enemyDamage = updateEnemyDamage(
        combat.enemy, combat.playerAttack.projectiles, stepTime);
    if (target != nullptr) {
        updateTarget(*target, combat.playerAttack.projectiles, stepTime);
    }
    combat.playerAttack.projectiles.retireExpired();
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
    combat.enemyProjectiles.retireExpired();
    if (result.playerDamage == PlayerDamageResult::died) {
        freezeCombatInterpolation(combat);
    }
    return result;
}

} // namespace

void updatePlayerAttack(PlayerAttackState& attack, const Player& player,
    bool fireHeld, float stepTime, std::span<const Segment2D> walls)
{
    updateWeapon(attack.weapon, attack.projectiles,
        player, fireHeld, stepTime, walls);
    attack.projectiles.update(stepTime);
    resolveProjectileWallCollisions(attack.projectiles, walls);
}

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
