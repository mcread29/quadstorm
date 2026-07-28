#include "enemy_collection.hpp"

#include "arena.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace {

EnemyId stableEnemyId(const StableEnemy& enemy)
{
    return enemy.id();
}

std::optional<float> projectileHitAmount(const Projectile& projectile,
    float projectileRadius, const Enemy& enemy)
{
    const Vector2 relativeStart {
        projectile.previousPosition.x - enemy.previousPosition.x,
        projectile.previousPosition.y - enemy.previousPosition.y
    };
    const Vector2 relativeEnd {
        projectile.position.x - enemy.position.x,
        projectile.position.y - enemy.position.y
    };
    return sweepCircleAgainstCircle(relativeStart, relativeEnd,
        projectileRadius, Vector2 {}, ENEMY_RADIUS);
}

} // namespace

bool EnemyCollection::add(EnemyId id, Vector2 position)
{
    const auto entry = std::ranges::lower_bound(
        enemies, id, {}, stableEnemyId);
    if (entry != enemies.end() && entry->id() == id) {
        return false;
    }

    Enemy enemy;
    enemy.position = position;
    enemy.previousPosition = position;
    enemies.insert(entry, StableEnemy { id, enemy });
    return true;
}

void EnemyCollection::clear()
{
    enemies.clear();
}

StableEnemy* EnemyCollection::find(EnemyId id)
{
    const auto entry = std::ranges::lower_bound(
        enemies, id, {}, stableEnemyId);
    return entry != enemies.end() && entry->id() == id ? &*entry : nullptr;
}

const StableEnemy* EnemyCollection::find(EnemyId id) const
{
    const auto entry = std::ranges::lower_bound(
        enemies, id, {}, stableEnemyId);
    return entry != enemies.end() && entry->id() == id ? &*entry : nullptr;
}

std::size_t EnemyCollection::livingCount() const
{
    return static_cast<std::size_t>(std::ranges::count_if(enemies,
        [](const StableEnemy& entry) {
            return isEnemyAlive(entry.enemy);
        }));
}

bool EnemyCollection::allDefeated() const
{
    return !enemies.empty() && livingCount() == 0;
}

void freezeEnemyCollectionInterpolation(EnemyCollection& enemies)
{
    for (StableEnemy& entry : enemies.entries()) {
        entry.enemy.previousPosition = entry.enemy.position;
    }
}

void updateEnemyCollectionMovement(EnemyCollection& enemies,
    Vector2 playerPosition, float stepTime,
    std::span<const Segment2D> walls)
{
    for (StableEnemy& entry : enemies.entries()) {
        updateEnemyMovement(entry.enemy, playerPosition, stepTime);
        resolveCircleWallCollisions(entry.enemy.position,
            entry.enemy.velocity, ENEMY_RADIUS,
            entry.enemy.previousPosition, walls);
    }
}

EnemyDamageResult updateEnemyCollectionDamage(EnemyCollection& enemies,
    ProjectilePool& playerProjectiles, float stepTime)
{
    for (StableEnemy& entry : enemies.entries()) {
        entry.enemy.hitFlashRemaining = std::max(
            0.0F, entry.enemy.hitFlashRemaining - stepTime);
    }

    EnemyDamageResult result = EnemyDamageResult::none;
    for (Projectile& projectile : playerProjectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }

        StableEnemy* selected = nullptr;
        float selectedHitAmount = std::numeric_limits<float>::infinity();
        for (StableEnemy& entry : enemies.entries()) {
            if (!isEnemyAlive(entry.enemy)) {
                continue;
            }
            const auto hitAmount = projectileHitAmount(projectile,
                playerProjectiles.profile().radius, entry.enemy);
            if (!hitAmount.has_value()) {
                continue;
            }
            const bool earlier = *hitAmount < selectedHitAmount;
            const bool tied = *hitAmount == selectedHitAmount;
            if (selected == nullptr || earlier
                || (tied && entry.id() < selected->id())) {
                selected = &entry;
                selectedHitAmount = *hitAmount;
            }
        }
        if (selected == nullptr) {
            continue;
        }

        projectile.active = false;
        --selected->enemy.health;
        selected->enemy.hitFlashRemaining = ENEMY_HIT_FLASH_DURATION;
        if (selected->enemy.health <= 0) {
            selected->enemy.health = 0;
            selected->enemy.velocity = Vector2 {};
            result = EnemyDamageResult::died;
        } else if (result == EnemyDamageResult::none) {
            result = EnemyDamageResult::hit;
        }
    }
    return result;
}

bool updateEnemyCollectionPatterns(EnemyCollection& enemies,
    ProjectilePool& enemyProjectiles, Vector2 playerPosition,
    float stepTime, std::span<const Segment2D> walls)
{
    bool fired = false;
    for (StableEnemy& entry : enemies.entries()) {
        fired |= updateEnemyPattern(entry.enemy, enemyProjectiles,
            playerPosition, stepTime, walls);
    }
    return fired;
}
