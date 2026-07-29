#include "game_renderer.hpp"

#include "arena.hpp"
#include "render_primitives.hpp"
#include "render_style.hpp"

#include <algorithm>
#include <cmath>

using namespace game_render;

namespace {

Vector2 activeObjectivePosition(const HordeMatch& match)
{
    if (!match.anchorIsComplete()) {
        return match.plan().anchorPosition;
    }
    if (!match.hubIsPowered()) {
        return match.plan().hubPosition;
    }
    return match.plan().exitPosition;
}

Color hordeOutlineColor(HordeEnemyRole role)
{
    switch (role) {
    case HordeEnemyRole::Drifter:
        return Color { 255, 87, 48, 255 };
    case HordeEnemyRole::Runner:
        return Color { 255, 174, 48, 255 };
    case HordeEnemyRole::Caster:
        return Color { 213, 92, 255, 255 };
    case HordeEnemyRole::Elite:
        return Color { 255, 82, 200, 255 };
    }
    return WHITE;
}

} // namespace

GameRenderer::GameRenderer(const GeneratedLevel& level)
    : resources(level, lighting.shader())
{
}

GameRenderer::~GameRenderer() = default;

void GameRenderer::drawGenerated(const Camera3D& camera,
    const Player& player, Vector3 aimPoint, const GeneratedLevel& level,
    const LevelSession& session, const HordeMatch& match,
    float interpolationAmount, bool showDebug)
{
    if (lighting.beginShadowPass(camera.target)) {
        resources.generatedWallModel.materials[0].shader
            = lighting.shadowShader();
        resources.wallModel.materials[0].shader = lighting.shadowShader();
        DrawModel(resources.generatedWallModel, Vector3 {}, 1.0F, WHITE);
        drawGeneratedArchitecture(level);
        drawLockedDoorways(level, session);
        resources.generatedWallModel.materials[0].shader = lighting.shader();
        resources.wallModel.materials[0].shader = lighting.shader();
        lighting.endShadowPass();
    }

    lighting.update(camera, GENERATED_BACKGROUND);
    updateGeneratedLights(player, match);

    postProcess.beginScene(GENERATED_BACKGROUND);
    BeginMode3D(camera);
    drawVoidGrid();
    lighting.setMaterial(1.0F);
    DrawModel(resources.generatedFloorModel,
        Vector3 { 0.0F, -0.01F, 0.0F }, 1.0F, WHITE);
    if (!lighting.shadowsAvailable()) {
        lighting.setMaterial(4.0F);
        DrawModel(resources.generatedWallShadowModel,
            Vector3 {}, 1.0F, WHITE);
    }
    lighting.setMaterial(3.0F);
    DrawModel(resources.generatedFloorDetailModel, Vector3 {}, 1.0F, WHITE);
    lighting.setMaterial(2.0F);
    DrawModel(resources.generatedWallModel, Vector3 {}, 1.0F, WHITE);
    drawGeneratedArchitecture(level);
    drawLockedDoorways(level, session);
    drawHordeLandmarks(level, session, match);
    drawWorldReticle(aimPoint);
    drawProjectiles(camera, match.playerProjectiles(),
        interpolationAmount, PLAYER_RADIUS,
        Color { 92, 225, 255, 255 }, Color { 92, 225, 255, 155 });
    for (const HordeEnemy& enemy : match.enemies()) {
        drawHordeEnemy(enemy, interpolationAmount);
    }
    drawProjectiles(camera, match.enemyProjectiles(),
        interpolationAmount, PLAYER_RADIUS,
        Color { 255, 93, 55, 255 }, Color { 255, 153, 70, 175 });
    drawPlayer(player);
    EndMode3D();
    postProcess.endScene();

    postProcess.beginEmissive();
    BeginMode3D(camera);
    resources.generatedWallModel.materials[0].shader
        = lighting.shadowShader();
    resources.wallModel.materials[0].shader = lighting.shadowShader();
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawModel(resources.generatedWallModel, Vector3 {}, 1.0F, WHITE);
    drawGeneratedArchitecture(level);
    drawLockedDoorways(level, session);
    EndBlendMode();
    resources.generatedWallModel.materials[0].shader = lighting.shader();
    resources.wallModel.materials[0].shader = lighting.shader();
    drawProjectiles(camera, match.playerProjectiles(),
        interpolationAmount, PLAYER_RADIUS,
        Color { 92, 225, 255, 255 }, Color { 42, 151, 190, 255 });
    drawProjectiles(camera, match.enemyProjectiles(),
        interpolationAmount, PLAYER_RADIUS,
        Color { 255, 93, 55, 255 }, Color { 207, 62, 38, 255 });
    const Vector3 playerEmitter {
        player.position.x + player.facing.x * PLAYER_FACING_MARKER_DISTANCE,
        player.position.y + 0.08F,
        player.position.z + player.facing.y * PLAYER_FACING_MARKER_DISTANCE
    };
    DrawSphere(playerEmitter, 0.11F, ENERGY_CYAN);
    const Vector2 hub = match.plan().hubPosition;
    const Vector2 anchor = match.plan().anchorPosition;
    const Vector2 exit = match.plan().exitPosition;
    DrawSphere(Vector3 { hub.x, 2.72F, hub.y }, 0.34F,
        match.hubIsPowered() ? ENERGY_CYAN : Color { 30, 70, 72, 255 });
    DrawSphere(Vector3 { anchor.x, 1.72F, anchor.y }, 0.33F,
        match.anchorIsComplete() ? Color { 112, 229, 185, 255 }
                                 : MACHINE_GOLD);
    DrawSphere(Vector3 { exit.x, 5.8F, exit.y }, 0.14F,
        match.hubIsPowered() ? ENERGY_CYAN : Color { 35, 52, 54, 255 });
    for (const HordeEnemy& entry : match.enemies()) {
        if (!isEnemyAlive(entry.enemy)) {
            continue;
        }
        if (entry.role != HordeEnemyRole::Caster
            && entry.role != HordeEnemyRole::Elite) {
            continue;
        }
        const Vector2 position = interpolateEnemyPosition(
            entry.enemy, interpolationAmount);
        DrawSphere(Vector3 { position.x, ENEMY_RADIUS + 0.42F, position.y },
            entry.role == HordeEnemyRole::Elite ? 0.24F : 0.15F,
            hordeOutlineColor(entry.role));
    }
    EndMode3D();
    postProcess.endEmissive();

    postProcess.beginActorMask();
    BeginMode3D(camera);
    if (isPlayerAlive(player)) {
        DrawSphere(player.position, PLAYER_RADIUS * 0.88F,
            Color { 47, 225, 235, 255 });
    }
    for (const HordeEnemy& entry : match.enemies()) {
        if (!isEnemyAlive(entry.enemy)) {
            continue;
        }
        const Vector2 position = interpolateEnemyPosition(
            entry.enemy, interpolationAmount);
        DrawSphere(Vector3 { position.x, ENEMY_RADIUS, position.y },
            ENEMY_RADIUS * (entry.role == HordeEnemyRole::Elite ? 1.28F : 0.9F),
            hordeOutlineColor(entry.role));
    }
    EndMode3D();
    postProcess.endActorMask();

    const float damageAmount = std::clamp(
        player.hitFlashRemaining / PLAYER_HIT_FLASH_DURATION, 0.0F, 1.0F);
    const float dashAmount = std::clamp(
        player.dashRemaining / PLAYER_DASH_DURATION, 0.0F, 1.0F);
    const float energyPulse = match.anchorIsActive()
        ? 0.5F + 0.5F * std::sin(static_cast<float>(GetTime()) * 4.0F)
        : match.hubIsPowered()
            ? 0.16F + 0.08F * std::sin(static_cast<float>(GetTime()) * 2.0F)
            : 0.0F;
    const Vector2 objective = activeObjectivePosition(match);
    postProcess.process(PostProcessEffects {
        damageAmount,
        dashAmount,
        energyPulse,
        GetWorldToScreen(player.position, camera),
        GetWorldToScreen(Vector3 { objective.x, 0.0F, objective.y }, camera)
    });
    BeginDrawing();
    postProcess.present();
    drawGeneratedHud(player, level, session, match, showDebug);
    EndDrawing();
}

void GameRenderer::drawCombat(const Camera3D& camera,
    const Player& player, Vector3 aimPoint,
    const ProjectilePool& playerProjectiles,
    const Target& target, const Enemy& enemy,
    const ProjectilePool& enemyProjectiles,
    float interpolationAmount, bool showDebug)
{
    if (lighting.beginShadowPass(camera.target)) {
        resources.wallModel.materials[0].shader = lighting.shadowShader();
        drawArena();
        resources.wallModel.materials[0].shader = lighting.shader();
        lighting.endShadowPass();
    }

    lighting.update(camera, ARENA_BACKGROUND);
    lighting.clearPointLights();

    postProcess.beginScene(ARENA_BACKGROUND);
    BeginMode3D(camera);
    drawVoidGrid();
    lighting.setMaterial(1.0F);
    DrawModel(resources.groundModel, Vector3 { 0.0F, -0.015F, 0.0F }, 1.0F,
        Color { 48, 65, 68, 255 });
    lighting.setMaterial(2.0F);
    drawArena();
    drawWorldReticle(aimPoint);
    drawTarget(target);
    drawEnemy(enemy, interpolationAmount);
    drawProjectiles(camera, playerProjectiles, interpolationAmount,
        PLAYER_RADIUS, Color { 92, 225, 255, 255 },
        Color { 92, 225, 255, 155 });
    drawProjectiles(camera, enemyProjectiles, interpolationAmount,
        PLAYER_RADIUS, Color { 255, 93, 55, 255 },
        Color { 255, 153, 70, 175 });
    drawPlayer(player);
    EndMode3D();
    postProcess.endScene();

    postProcess.beginEmissive();
    BeginMode3D(camera);
    resources.wallModel.materials[0].shader = lighting.shadowShader();
    BeginBlendMode(BLEND_MULTIPLIED);
    drawArena();
    EndBlendMode();
    resources.wallModel.materials[0].shader = lighting.shader();
    drawProjectiles(camera, playerProjectiles, interpolationAmount,
        PLAYER_RADIUS, Color { 92, 225, 255, 255 },
        Color { 42, 151, 190, 255 });
    drawProjectiles(camera, enemyProjectiles, interpolationAmount,
        PLAYER_RADIUS, Color { 255, 93, 55, 255 },
        Color { 207, 62, 38, 255 });
    if (target.health > 0) {
        DrawSphere(Vector3 { target.position.x, TARGET_RADIUS,
                       target.position.y },
            TARGET_RADIUS * 0.28F, Color { 255, 105, 74, 255 });
    }
    if (isEnemyAlive(enemy)) {
        const Vector2 enemyPosition = interpolateEnemyPosition(
            enemy, interpolationAmount);
        DrawSphere(Vector3 { enemyPosition.x, ENEMY_RADIUS,
                       enemyPosition.y },
            ENEMY_RADIUS * 0.24F, Color { 211, 91, 255, 255 });
    }
    EndMode3D();
    postProcess.endEmissive();

    postProcess.beginActorMask();
    BeginMode3D(camera);
    if (isPlayerAlive(player)) {
        DrawSphere(player.position, PLAYER_RADIUS * 0.88F,
            Color { 47, 225, 235, 255 });
    }
    if (target.health > 0) {
        DrawSphere(Vector3 { target.position.x, TARGET_RADIUS,
                       target.position.y },
            TARGET_RADIUS, Color { 255, 91, 61, 255 });
    }
    if (isEnemyAlive(enemy)) {
        const Vector2 enemyPosition = interpolateEnemyPosition(
            enemy, interpolationAmount);
        DrawSphere(Vector3 { enemyPosition.x, ENEMY_RADIUS,
                       enemyPosition.y },
            ENEMY_RADIUS, Color { 211, 91, 255, 255 });
    }
    EndMode3D();
    postProcess.endActorMask();

    const float damageAmount = std::clamp(
        player.hitFlashRemaining / PLAYER_HIT_FLASH_DURATION, 0.0F, 1.0F);
    const float dashAmount = std::clamp(
        player.dashRemaining / PLAYER_DASH_DURATION, 0.0F, 1.0F);
    postProcess.process(PostProcessEffects {
        damageAmount,
        dashAmount,
        0.0F,
        GetWorldToScreen(player.position, camera),
        GetWorldToScreen(Vector3 { target.position.x, 0.0F,
                             target.position.y }, camera)
    });
    BeginDrawing();
    postProcess.present();
    drawCombatHud(player, playerProjectiles, target, enemy,
        enemyProjectiles, showDebug);
    EndDrawing();
}

void GameRenderer::drawArena() const
{
    constexpr Color wallColor { 94, 111, 118, 255 };

    for (const WallSegment& wall : ARENA_WALLS) {
        const Vector2 direction {
            wall.end.x - wall.start.x,
            wall.end.y - wall.start.y
        };
        const float wallLength = std::sqrt(
            direction.x * direction.x + direction.y * direction.y);
        if (wallLength <= 0.0001F) {
            continue;
        }

        const Vector2 outwardNormal {
            direction.y / wallLength,
            -direction.x / wallLength
        };
        const Vector3 center {
            (wall.start.x + wall.end.x) * 0.5F
                + outwardNormal.x * WALL_THICKNESS * 0.5F,
            WALL_HEIGHT * 0.5F,
            (wall.start.y + wall.end.y) * 0.5F
                + outwardNormal.y * WALL_THICKNESS * 0.5F
        };
        const float angle = -std::atan2(direction.y, direction.x) * RAD2DEG;
        DrawModelEx(resources.wallModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, angle,
            Vector3 { wallLength, 1.0F, 1.0F }, wallColor);
    }
}

void GameRenderer::drawGeneratedArchitecture(
    const GeneratedLevel& level) const
{
    constexpr Color pylonColor { 30, 39, 45, 255 };
    constexpr Color pylonEdge { 116, 128, 123, 255 };
    constexpr Color signalColor { 38, 103, 105, 255 };
    const auto walls = level.walls();

    for (std::size_t index = 0; index < walls.size(); ++index) {
        if (index % 7U != 0U) {
            continue;
        }
        const Segment2D& wall = walls[index];
        const Vector3 center {
            (wall.start.x + wall.end.x) * 0.5F,
            WALL_HEIGHT * 0.62F,
            (wall.start.y + wall.end.y) * 0.5F
        };
        DrawModelEx(resources.wallModel, center,
            Vector3 { 0.0F, 1.0F, 0.0F }, 0.0F,
            Vector3 { 0.42F, 1.24F, 1.78F }, pylonColor);
        DrawModelEx(resources.wallModel,
            Vector3 { center.x, WALL_HEIGHT + 0.22F, center.z },
            Vector3 { 0.0F, 1.0F, 0.0F }, 0.0F,
            Vector3 { 0.58F, 0.1F, 2.35F }, pylonEdge);
        if (index % 21U == 0U) {
            DrawCylinder(Vector3 { center.x, WALL_HEIGHT * 0.62F, center.z },
                0.055F, 0.055F, WALL_HEIGHT * 1.06F, 6, signalColor);
            DrawSphere(Vector3 { center.x, WALL_HEIGHT + 0.34F, center.z },
                0.1F, Color { 69, 173, 171, 255 });
        }
    }
}

void GameRenderer::drawLockedDoorways(const GeneratedLevel& level,
    const LevelSession& session) const
{
    constexpr Color lockedColor { 132, 82, 72, 255 };
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        if (!session.doorwayIsLocked(threshold.doorway)) {
            continue;
        }
        const Segment2D& wall = threshold.segment;
        const Vector2 direction {
            wall.end.x - wall.start.x,
            wall.end.y - wall.start.y
        };
        const float wallLength = std::sqrt(
            direction.x * direction.x + direction.y * direction.y);
        if (wallLength <= 0.0001F) {
            continue;
        }
        const Vector3 center {
            (wall.start.x + wall.end.x) * 0.5F,
            WALL_HEIGHT * 0.5F,
            (wall.start.y + wall.end.y) * 0.5F
        };
        const float angle = -std::atan2(direction.y, direction.x) * RAD2DEG;
        DrawModelEx(resources.wallModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, angle,
            Vector3 { wallLength, 1.0F, 1.0F }, lockedColor);
    }
}
