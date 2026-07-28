#pragma once

#include "collision_2d.hpp"
#include "enemy.hpp"
#include "projectile_pool.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using EnemyId = std::uint64_t;

class StableEnemy {
public:
    EnemyId id() const { return identity; }
    Enemy enemy {};

private:
    friend class EnemyCollection;
    StableEnemy(EnemyId stableId, Enemy initialEnemy)
        : enemy(initialEnemy)
        , identity(stableId)
    {
    }

    EnemyId identity = 0;
};

class EnemyCollection {
public:
    bool add(EnemyId id, Vector2 position);
    void clear();

    std::span<StableEnemy> entries() { return enemies; }
    std::span<const StableEnemy> entries() const { return enemies; }
    StableEnemy* find(EnemyId id);
    const StableEnemy* find(EnemyId id) const;
    std::size_t size() const { return enemies.size(); }
    std::size_t livingCount() const;
    bool allDefeated() const;

private:
    std::vector<StableEnemy> enemies;
};

void freezeEnemyCollectionInterpolation(EnemyCollection& enemies);
void updateEnemyCollectionMovement(EnemyCollection& enemies,
    Vector2 playerPosition, float stepTime,
    std::span<const Segment2D> walls);
EnemyDamageResult updateEnemyCollectionDamage(EnemyCollection& enemies,
    ProjectilePool& playerProjectiles, float stepTime);
bool updateEnemyCollectionPatterns(EnemyCollection& enemies,
    ProjectilePool& enemyProjectiles, Vector2 playerPosition,
    float stepTime, std::span<const Segment2D> walls);
