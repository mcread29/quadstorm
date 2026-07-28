#pragma once

#include "collision_2d.hpp"
#include "enemy.hpp"
#include "player.hpp"
#include "projectile_pool.hpp"
#include "weapon.hpp"

#include <span>

struct Target;

struct PlayerAttackState {
    Weapon weapon {};
    ProjectilePool projectiles {};
};

struct CombatState {
    PlayerAttackState playerAttack {};
    Enemy enemy {};
    ProjectilePool enemyProjectiles { ENEMY_PROJECTILE_PROFILE };
};

struct CombatStepResult {
    PlayerDamageResult playerDamage = PlayerDamageResult::none;
    EnemyDamageResult enemyDamage = EnemyDamageResult::none;
    bool enemyFired = false;
};

void updatePlayerAttack(PlayerAttackState& attack, const Player& player,
    bool fireHeld, float stepTime, std::span<const Segment2D> walls);
void resetCombat(CombatState& combat, Vector2 enemyPosition);
CombatStepResult updateCombat(CombatState& combat, Player& player,
    const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls);
CombatStepResult updateCombat(CombatState& combat, Player& player,
    Target& target, const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls);
