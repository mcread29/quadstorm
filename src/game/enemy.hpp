#pragma once

#include "collision_2d.hpp"
#include "projectile_pool.hpp"

#include "raylib.h"

#include <span>

inline constexpr float ENEMY_RADIUS = 0.78F;
inline constexpr int ENEMY_MAX_HEALTH = 20;
inline constexpr float ENEMY_HIT_FLASH_DURATION = 0.14F;
inline constexpr float ENEMY_MOVE_SPEED = 2.4F;
inline constexpr float ENEMY_PREFERRED_DISTANCE = 5.0F;
inline constexpr float ENEMY_FIRST_SHOT_DELAY = 0.8F;
inline constexpr float ENEMY_SHOT_INTERVAL = 1.15F;
inline constexpr float ENEMY_PROJECTILE_SPEED = 6.5F;
inline constexpr float ENEMY_PROJECTILE_LIFETIME = 3.4F;
inline constexpr float ENEMY_PROJECTILE_RADIUS = 0.22F;
inline constexpr ProjectileProfile ENEMY_PROJECTILE_PROFILE {
    .speed = ENEMY_PROJECTILE_SPEED,
    .lifetime = ENEMY_PROJECTILE_LIFETIME,
    .radius = ENEMY_PROJECTILE_RADIUS
};

struct Enemy {
    Vector2 position { 5.0F, 5.0F };
    Vector2 previousPosition = position;
    Vector2 velocity {};
    Vector2 facing { -1.0F, 0.0F };
    int health = ENEMY_MAX_HEALTH;
    float hitFlashRemaining = 0.0F;
    float shotCooldownRemaining = ENEMY_FIRST_SHOT_DELAY;
};

enum class EnemyDamageResult {
    none,
    hit,
    died
};

void updateEnemyMovement(Enemy& enemy, Vector2 playerPosition, float stepTime);
bool updateEnemyPattern(Enemy& enemy, ProjectilePool& projectiles,
    Vector2 playerPosition, float stepTime);
bool updateEnemyPattern(Enemy& enemy, ProjectilePool& projectiles,
    Vector2 playerPosition, float stepTime,
    std::span<const Segment2D> walls);
EnemyDamageResult updateEnemyDamage(Enemy& enemy,
    ProjectilePool& playerProjectiles, float stepTime);
bool isEnemyAlive(const Enemy& enemy);
Vector2 interpolateEnemyPosition(
    const Enemy& enemy, float interpolationAmount);
