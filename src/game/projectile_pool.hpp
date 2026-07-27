#pragma once

#include "raylib.h"

#include <array>
#include <cstddef>

inline constexpr std::size_t PROJECTILE_POOL_CAPACITY = 192;
inline constexpr float PROJECTILE_SPEED = 22.0F;
inline constexpr float PROJECTILE_LIFETIME = 1.8F;
inline constexpr float PROJECTILE_RADIUS = 0.16F;

struct Projectile {
    Vector2 position {};
    Vector2 previousPosition {};
    Vector2 velocity {};
    float remainingLifetime = 0.0F;
    bool active = false;
};

class ProjectilePool {
public:
    bool spawn(Vector2 position, Vector2 direction);
    void update(float stepTime);

    auto& projectiles() { return slots; }
    const auto& projectiles() const { return slots; }
    std::size_t activeCount() const;

private:
    std::array<Projectile, PROJECTILE_POOL_CAPACITY> slots {};
};

Vector2 interpolateProjectilePosition(const Projectile& projectile, float amount);
