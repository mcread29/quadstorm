#pragma once

#include "raylib.h"

#include <array>
#include <cstddef>

inline constexpr std::size_t PROJECTILE_POOL_CAPACITY = 192;
inline constexpr float PLAYER_PROJECTILE_SPEED = 22.0F;
inline constexpr float PLAYER_PROJECTILE_LIFETIME = 1.8F;
inline constexpr float PLAYER_PROJECTILE_RADIUS = 0.16F;

struct ProjectileProfile {
    float speed = PLAYER_PROJECTILE_SPEED;
    float lifetime = PLAYER_PROJECTILE_LIFETIME;
    float radius = PLAYER_PROJECTILE_RADIUS;
};

inline constexpr ProjectileProfile PLAYER_PROJECTILE_PROFILE {};

struct Projectile {
    Vector2 position {};
    Vector2 previousPosition {};
    Vector2 velocity {};
    float remainingLifetime = 0.0F;
    bool active = false;
};

class ProjectilePool {
public:
    explicit ProjectilePool(
        ProjectileProfile profile = PLAYER_PROJECTILE_PROFILE);

    bool spawn(Vector2 position, Vector2 direction);
    void update(float stepTime);

    auto& projectiles() { return slots; }
    const auto& projectiles() const { return slots; }
    const ProjectileProfile& profile() const { return projectileProfile; }
    std::size_t activeCount() const;

private:
    ProjectileProfile projectileProfile;
    std::array<Projectile, PROJECTILE_POOL_CAPACITY> slots {};
};

Vector2 interpolateProjectilePosition(const Projectile& projectile, float amount);
