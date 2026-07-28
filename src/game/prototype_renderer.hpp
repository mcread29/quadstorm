#pragma once

#include "enemy.hpp"
#include "player.hpp"
#include "projectile_pool.hpp"
#include "target.hpp"

#include "raylib.h"

class PrototypeRenderer {
public:
    PrototypeRenderer();
    ~PrototypeRenderer();

    PrototypeRenderer(const PrototypeRenderer&) = delete;
    PrototypeRenderer& operator=(const PrototypeRenderer&) = delete;

    void draw(const Camera3D& camera, const Player& player, Vector3 aimPoint,
        const ProjectilePool& playerProjectiles, const Target& target,
        const Enemy& enemy, const ProjectilePool& enemyProjectiles,
        float interpolationAmount) const;

private:
    void drawArena() const;
    void drawPlayerShadow(const Player& player) const;
    void drawPlayer(const Player& player) const;
    void drawProjectiles(const ProjectilePool& projectiles,
        float interpolationAmount, float height, Color headColor,
        Color trailColor) const;
    void drawTarget(const Target& target) const;
    void drawEnemy(const Enemy& enemy, float interpolationAmount) const;

    Shader lightingShader {};
    Model groundModel {};
    Model wallModel {};
    Model playerModel {};
    Model projectileModel {};
    Model targetModel {};
    Model enemyModel {};
    Model shadowModel {};
};
