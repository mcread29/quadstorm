#pragma once

#include "combat.hpp"
#include "enemy.hpp"
#include "generated_level.hpp"
#include "horde_match.hpp"
#include "level_session.hpp"
#include "player.hpp"
#include "post_process_pipeline.hpp"
#include "projectile_pool.hpp"
#include "render_resources.hpp"
#include "target.hpp"
#include "world_lighting.hpp"

#include "raylib.h"

#include <cstddef>

class GameRenderer {
public:
    explicit GameRenderer(const GeneratedLevel& level);
    ~GameRenderer();

    GameRenderer(const GameRenderer&) = delete;
    GameRenderer& operator=(const GameRenderer&) = delete;

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
    void drawText(const char* text, float x, float y,
        float fontSize, Color color) const;
    float measureText(const char* text, float fontSize) const;
    void drawArena() const;
    void drawGeneratedArchitecture(const GeneratedLevel& level) const;
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
    void updateGeneratedLights(
        const Player& player, const HordeMatch& match) const;
    void drawGeneratedHud(const Player& player, const GeneratedLevel& level,
        const LevelSession& session, const HordeMatch& match,
        bool showDebug) const;
    void drawCombatHud(const Player& player,
        const ProjectilePool& playerProjectiles, const Target& target,
        const Enemy& enemy, const ProjectilePool& enemyProjectiles,
        bool showDebug) const;
    void drawPlayerHud(const Player& player) const;

    WorldLighting lighting;
    RenderResources resources;
    PostProcessPipeline postProcess;
};
