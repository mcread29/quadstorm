#pragma once

#include "combat.hpp"
#include "enemy.hpp"
#include "generated_level.hpp"
#include "horde_match.hpp"
#include "level_session.hpp"
#include "player.hpp"
#include "projectile_pool.hpp"
#include "target.hpp"

#include "raylib.h"

#include <cstddef>

class PrototypeRenderer {
public:
    explicit PrototypeRenderer(const GeneratedLevel& level);
    ~PrototypeRenderer();

    PrototypeRenderer(const PrototypeRenderer&) = delete;
    PrototypeRenderer& operator=(const PrototypeRenderer&) = delete;

    void drawGenerated(const Camera3D& camera, const Player& player,
        Vector3 aimPoint, const GeneratedLevel& level,
        const LevelSession& session, const HordeMatch& match,
        float interpolationAmount, bool showDebug) const;
    void drawGeneratedOverview(const GeneratedLevel& level,
        const LevelSession* session, const HordeMatch* match,
        std::size_t selectedConfiguration,
        std::size_t configurationCount) const;
    void drawCombat(const Camera3D& camera, const Player& player,
        Vector3 aimPoint, const ProjectilePool& playerProjectiles,
        const Target& target, const Enemy& enemy,
        const ProjectilePool& enemyProjectiles,
        float interpolationAmount, bool showDebug) const;

private:
    void drawArena() const;
    void drawLockedDoorways(const GeneratedLevel& level,
        const LevelSession& session) const;
    void drawActorShadow(Vector3 position, float radius) const;
    void drawPlayer(const Player& player) const;
    void drawProjectiles(const Camera3D& camera,
        const ProjectilePool& projectiles, float interpolationAmount,
        float height, Color headColor, Color trailColor) const;
    void drawTarget(const Target& target) const;
    void drawEnemy(const Enemy& enemy, float interpolationAmount) const;
    void drawHordeEnemy(
        const HordeEnemy& enemy, float interpolationAmount) const;
    void drawHordeLandmarks(const GeneratedLevel& level,
        const LevelSession& session, const HordeMatch& match) const;
    void updateLighting(const Camera3D& camera, Color fogColor) const;
    void drawPlayerHud(const Player& player) const;

    Shader lightingShader {};
    Model groundModel {};
    Model generatedFloorModel {};
    Model generatedWallModel {};
    Model wallModel {};
    Model playerModel {};
    Model targetModel {};
    Model enemyModel {};
    Model shadowModel {};
    Texture2D projectileGlow {};
    int cameraPositionLocation = -1;
    int cameraTargetLocation = -1;
    int fogColorLocation = -1;
};
