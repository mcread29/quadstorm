#pragma once

#include "enemy.hpp"
#include "player.hpp"
#include "projectile_pool.hpp"
#include "target.hpp"
#include "weapon.hpp"

struct Encounter {
    Player player {};
    Weapon weapon {};
    ProjectilePool playerProjectiles {};
    Target target {};
    Enemy enemy {};
    ProjectilePool enemyProjectiles { ENEMY_PROJECTILE_PROFILE };
};

struct EncounterStepResult {
    PlayerDamageResult playerDamage = PlayerDamageResult::none;
    EnemyDamageResult enemyDamage = EnemyDamageResult::none;
    bool enemyFired = false;
    bool restarted = false;
};

void resetEncounter(Encounter& encounter);
EncounterStepResult updateEncounter(
    Encounter& encounter, const PlayerInput& input, float stepTime);
