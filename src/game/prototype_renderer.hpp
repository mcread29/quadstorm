#pragma once

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
        const ProjectilePool& projectiles, const Target& target,
        float interpolationAmount) const;

private:
    void drawPlayerShadow(const Player& player) const;
    void drawPlayer(const Player& player) const;
    void drawProjectiles(const ProjectilePool& projectiles,
        float interpolationAmount) const;
    void drawTarget(const Target& target) const;

    Shader lightingShader {};
    Model groundModel {};
    Model playerModel {};
    Model projectileModel {};
    Model targetModel {};
    Model shadowModel {};
};
