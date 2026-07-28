#pragma once

#include "combat.hpp"
#include "enemy.hpp"
#include "generated_level.hpp"
#include "level_session.hpp"
#include "player.hpp"
#include "projectile_pool.hpp"
#include "target.hpp"

#include "raylib.h"

class PrototypeRenderer {
public:
    explicit PrototypeRenderer(const GeneratedLevel& level);
    ~PrototypeRenderer();

    PrototypeRenderer(const PrototypeRenderer&) = delete;
    PrototypeRenderer& operator=(const PrototypeRenderer&) = delete;

    void drawGenerated(const Camera3D& camera, const Player& player,
        Vector3 aimPoint, const GeneratedLevel& level,
        const LevelSession& session, const CombatState* combat,
        bool floorComplete, float interpolationAmount) const;
    void drawCombat(const Camera3D& camera, const Player& player,
        Vector3 aimPoint, const ProjectilePool& playerProjectiles,
        const Target& target, const Enemy& enemy,
        const ProjectilePool& enemyProjectiles,
        float interpolationAmount) const;

private:
    void drawArena() const;
    void drawWalls(std::span<const Segment2D> walls) const;
    void drawPlayerShadow(const Player& player) const;
    void drawPlayer(const Player& player) const;
    void drawProjectiles(const ProjectilePool& projectiles,
        float interpolationAmount, float height, Color headColor,
        Color trailColor) const;
    void drawTarget(const Target& target) const;
    void drawEnemy(const Enemy& enemy, float interpolationAmount) const;

    Shader lightingShader {};
    Model groundModel {};
    Model generatedFloorModel {};
    Model wallModel {};
    Model playerModel {};
    Model projectileModel {};
    Model targetModel {};
    Model enemyModel {};
    Model shadowModel {};
};
