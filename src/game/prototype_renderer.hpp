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
        float interpolationAmount, bool showDebug);
    void drawGeneratedOverview(const GeneratedLevel& level,
        const LevelSession* session, const HordeMatch* match,
        std::size_t selectedConfiguration,
        std::size_t configurationCount) const;
    void drawCombat(const Camera3D& camera, const Player& player,
        Vector3 aimPoint, const ProjectilePool& playerProjectiles,
        const Target& target, const Enemy& enemy,
        const ProjectilePool& enemyProjectiles,
        float interpolationAmount, bool showDebug);

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
    void updateGeneratedLights(const HordeMatch& match) const;
    void clearLocalLights() const;
    void setMaterial(float kind) const;
    void ensurePostProcessTargets();
    void buildBloom();
    void drawPostProcessedScene(float damageAmount,
        float dashAmount, float energyPulse) const;
    void drawPlayerHud(const Player& player) const;

    Shader lightingShader {};
    Shader bloomExtractShader {};
    Shader bloomBlurShader {};
    Shader compositeShader {};
    RenderTexture2D sceneTarget {};
    RenderTexture2D bloomTargetA {};
    RenderTexture2D bloomTargetB {};
    Model groundModel {};
    Model generatedFloorModel {};
    Model generatedWallModel {};
    Model wallModel {};
    Model playerModel {};
    Model targetModel {};
    Model enemyModel {};
    Model runnerModel {};
    Model casterModel {};
    Model eliteModel {};
    Model shadowModel {};
    Texture2D projectileGlow {};
    int cameraPositionLocation = -1;
    int cameraTargetLocation = -1;
    int fogColorLocation = -1;
    int materialKindLocation = -1;
    int pointLightPositionALocation = -1;
    int pointLightColorALocation = -1;
    int pointLightPositionBLocation = -1;
    int pointLightColorBLocation = -1;
    int blurDirectionLocation = -1;
    int compositeResolutionLocation = -1;
    int compositeTimeLocation = -1;
    int compositeDamageLocation = -1;
    int compositeDashLocation = -1;
    int compositeEnergyLocation = -1;
    int postProcessWidth = 0;
    int postProcessHeight = 0;
};
