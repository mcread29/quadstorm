#include "game_renderer.hpp"

#include "combat.hpp"
#include "directional_shader.hpp"
#include "render_style.hpp"

#include <algorithm>
#include <cmath>

using namespace game_render;

void GameRenderer::drawActorShadow(Vector3 position, float radius) const
{
    const float groundDistance = position.y / -DIRECTIONAL_LIGHT.y;
    const Vector3 shadowPosition {
        position.x + DIRECTIONAL_LIGHT.x * groundDistance,
        0.006F,
        position.z + DIRECTIONAL_LIGHT.z * groundDistance
    };
    const float shadowAngle = std::atan2(
        DIRECTIONAL_LIGHT.z, DIRECTIONAL_LIGHT.x) * RAD2DEG;
    const float size = radius / PLAYER_RADIUS;
    DrawModelEx(resources.shadowModel,
        Vector3 { shadowPosition.x, shadowPosition.y - 0.002F,
            shadowPosition.z },
        Vector3 { 0.0F, 1.0F, 0.0F }, shadowAngle,
        Vector3 { size * 1.68F, 1.0F, size * 1.0F },
        Color { 3, 7, 10, 30 });
    DrawModelEx(resources.shadowModel, shadowPosition,
        Vector3 { 0.0F, 1.0F, 0.0F }, shadowAngle,
        Vector3 { size * 1.18F, 1.0F, size * 0.68F },
        Color { 2, 5, 8, 92 });
}

void GameRenderer::drawPlayer(const Player& player) const
{
    lighting.setMaterial(0.0F);
    constexpr Color armorColor { 45, 58, 64, 255 };
    constexpr Color armorEdge { 103, 119, 119, 255 };
    const Color bodyColor = player.hitFlashRemaining > 0.0F
        ? Color { 255, 245, 210, 255 }
        : isPlayerAlive(player)
            ? Color { 211, 142, 43, 255 }
            : Color { 77, 65, 61, 255 };
    const float bodyScale = isPlayerAlive(player) ? 1.0F : 0.65F;
    const Vector2 side { -player.facing.y, player.facing.x };

    drawActorShadow(player.position, PLAYER_RADIUS * bodyScale);
    if (isPlayerAlive(player)) {
        DrawCircle3D(Vector3 {
                         player.position.x, 0.024F, player.position.z },
            PLAYER_RADIUS * 1.16F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 171, 111, 29, 175 });
        if (player.dashRemaining > 0.0F) {
            DrawCircle3D(Vector3 {
                             player.position.x, 0.03F, player.position.z },
                PLAYER_RADIUS * 1.7F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
                Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                    ENERGY_CYAN.b, 175 });
            const Vector3 dashTail {
                player.position.x - player.dashDirection.x * 1.15F,
                player.position.y * 0.85F,
                player.position.z - player.dashDirection.y * 1.15F
            };
            DrawCylinderEx(dashTail, player.position,
                0.18F, 0.04F, 8,
                Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                    ENERGY_CYAN.b, 115 });
        }
    }
    DrawModelEx(resources.playerModel, player.position,
        Vector3 { 0.0F, 1.0F, 0.0F }, 0.0F,
        Vector3 { bodyScale, bodyScale * 0.82F, bodyScale }, bodyColor);
    const Vector3 helmet {
        player.position.x,
        player.position.y + 0.46F * bodyScale,
        player.position.z
    };
    DrawSphere(helmet, 0.27F * bodyScale, armorColor);
    DrawCylinderEx(helmet,
        Vector3 { helmet.x + player.facing.x * 0.31F * bodyScale,
            helmet.y, helmet.z + player.facing.y * 0.31F * bodyScale },
        0.13F * bodyScale, 0.09F * bodyScale, 7,
        isPlayerAlive(player) ? ENERGY_CYAN : armorEdge);
    const Vector3 backpack {
        player.position.x - player.facing.x * 0.43F * bodyScale,
        player.position.y + 0.02F,
        player.position.z - player.facing.y * 0.43F * bodyScale
    };
    DrawCube(backpack, 0.38F * bodyScale, 0.5F * bodyScale,
        0.38F * bodyScale, armorColor);
    for (const float sign : { -1.0F, 1.0F }) {
        const Vector3 shoulder {
            player.position.x + side.x * 0.5F * sign,
            player.position.y + 0.04F,
            player.position.z + side.y * 0.5F * sign
        };
        DrawSphere(shoulder, 0.22F * bodyScale, armorColor);
        DrawSphereWires(shoulder, 0.225F * bodyScale,
            6, 8, armorEdge);
    }
    if (player.invulnerabilityRemaining > 0.0F) {
        const float shieldScale = 1.12F
            + 0.16F * (player.invulnerabilityRemaining
                / PLAYER_INVULNERABILITY_DURATION);
        DrawSphereWires(player.position, PLAYER_RADIUS * shieldScale,
            10, 14, Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                ENERGY_CYAN.b, 190 });
    }
    if (!isPlayerAlive(player)) {
        DrawCircle3D(Vector3 {
                         player.position.x, 0.025F, player.position.z },
            PLAYER_RADIUS * 1.8F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 255, 113, 74, 210 });
        return;
    }

    const Vector3 noseStart {
        player.position.x + player.facing.x * PLAYER_RADIUS * 0.42F,
        player.position.y + 0.06F,
        player.position.z + player.facing.y * PLAYER_RADIUS * 0.42F
    };
    const Vector3 noseEnd {
        player.position.x + player.facing.x * PLAYER_FACING_MARKER_DISTANCE,
        player.position.y + 0.06F,
        player.position.z + player.facing.y * PLAYER_FACING_MARKER_DISTANCE
    };
    DrawCylinderEx(noseStart, noseEnd, 0.17F, 0.05F, 8, armorColor);
    DrawCylinderEx(Vector3 {
                       noseStart.x + side.x * 0.16F,
                       noseStart.y + 0.07F,
                       noseStart.z + side.y * 0.16F },
        Vector3 {
            noseEnd.x + side.x * 0.16F,
            noseEnd.y + 0.07F,
            noseEnd.z + side.y * 0.16F },
        0.07F, 0.025F, 7, MACHINE_GOLD);
    DrawCylinderEx(Vector3 {
                       noseStart.x + side.x * 0.22F,
                       noseStart.y + 0.2F,
                       noseStart.z + side.y * 0.22F },
        Vector3 {
            noseStart.x - side.x * 0.22F,
            noseStart.y + 0.2F,
            noseStart.z - side.y * 0.22F },
        0.075F, 0.075F, 7, ENERGY_CYAN);
    DrawSphere(noseEnd, 0.09F, ENERGY_CYAN);
}

void GameRenderer::drawProjectiles(const Camera3D& camera,
    const ProjectilePool& projectiles, float interpolationAmount,
    float height, Color headColor, Color trailColor) const
{
    constexpr float trailLength = 0.62F;
    const float inverseSpeed = 1.0F / projectiles.profile().speed;

    for (const Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const Vector2 position = interpolateProjectilePosition(
            projectile, interpolationAmount);
        const Vector3 head { position.x, height, position.y };
        const Vector3 tail {
            head.x - projectile.velocity.x * inverseSpeed * trailLength,
            height,
            head.z - projectile.velocity.y * inverseSpeed * trailLength
        };
        DrawLine3D(tail, head, trailColor);
    }

    BeginBlendMode(BLEND_ADDITIVE);
    for (const Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const Vector2 position = interpolateProjectilePosition(
            projectile, interpolationAmount);
        const Vector3 head { position.x, height, position.y };
        DrawBillboard(camera, resources.projectileGlow, head,
            projectiles.profile().radius * 3.4F, headColor);
    }
    EndBlendMode();
}

void GameRenderer::drawTarget(const Target& target) const
{
    lighting.setMaterial(0.0F);
    const Vector3 targetCenter {
        target.position.x,
        TARGET_RADIUS,
        target.position.y
    };
    const Vector3 groundCenter {
        target.position.x,
        0.025F,
        target.position.y
    };
    drawActorShadow(targetCenter, TARGET_RADIUS);

    if (target.health <= 0) {
        const float resetProgress = 1.0F
            - target.resetRemaining / TARGET_RESET_DELAY;
        DrawCircle3D(groundCenter,
            TARGET_RADIUS * (0.35F + resetProgress * 0.9F),
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 255, 174, 92, 220 });
        DrawSphereWires(targetCenter, TARGET_RADIUS * 0.4F,
            8, 12, Color { 255, 210, 140, 150 });
        return;
    }

    const float flashAmount = target.hitFlashRemaining
        / TARGET_HIT_FLASH_DURATION;
    const float targetScale = 1.0F + flashAmount * 0.18F;
    const Color targetColor = flashAmount > 0.0F
        ? Color { 255, 238, 194, 255 }
        : Color { 207, 75, 72, 255 };

    DrawCircle3D(groundCenter, TARGET_RADIUS * 1.05F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { 91, 31, 35, 230 });
    DrawModel(resources.targetModel, targetCenter, targetScale, targetColor);
    if (flashAmount > 0.0F) {
        DrawSphereWires(targetCenter,
            TARGET_RADIUS * (1.15F + (1.0F - flashAmount) * 0.45F),
            8, 12, Color { 255, 245, 210, 210 });
    }
}

void GameRenderer::drawEnemy(
    const Enemy& enemy, float interpolationAmount) const
{
    lighting.setMaterial(0.0F);
    const Vector2 position = interpolateEnemyPosition(
        enemy, interpolationAmount);
    const Vector3 center { position.x, ENEMY_RADIUS, position.y };
    const Vector3 groundCenter { position.x, 0.025F, position.y };
    if (!isEnemyAlive(enemy)) {
        DrawCircle3D(groundCenter, ENEMY_RADIUS * 1.8F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 151, 231, 190, 210 });
        DrawModel(resources.enemyModel, center, 0.55F, Color { 72, 74, 81, 255 });
        DrawSphereWires(center, ENEMY_RADIUS * 1.2F,
            8, 12, Color { 186, 244, 211, 190 });
        return;
    }

    drawActorShadow(center, ENEMY_RADIUS);
    const float chargeAmount = 1.0F - std::clamp(
        enemy.shotCooldownRemaining / ENEMY_SHOT_INTERVAL, 0.0F, 1.0F);
    const Color bodyColor = enemy.hitFlashRemaining > 0.0F
        ? Color { 244, 222, 255, 255 }
        : Color { 143, 81, 184, 255 };

    DrawCircle3D(groundCenter,
        ENEMY_RADIUS * (1.15F + chargeAmount * 0.35F),
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { 80, 48, 112, 220 });
    DrawModelEx(resources.enemyModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, 0.0F,
        Vector3 { 1.0F, 1.18F, 1.0F }, bodyColor);
    if (enemy.hitFlashRemaining > 0.0F) {
        DrawSphereWires(center, ENEMY_RADIUS * 1.18F,
            8, 12, Color { 255, 238, 194, 210 });
    }
    const Vector3 aimEnd {
        center.x + enemy.facing.x * (ENEMY_RADIUS + 0.6F),
        center.y,
        center.z + enemy.facing.y * (ENEMY_RADIUS + 0.6F)
    };
    DrawCylinderEx(center, aimEnd, 0.18F, 0.07F, 8,
        Color { 255, 177, 92, 255 });
}

void GameRenderer::drawHordeEnemy(
    const HordeEnemy& entry, float interpolationAmount) const
{
    lighting.setMaterial(0.0F);
    const Enemy& enemy = entry.enemy;
    const Vector2 position = interpolateEnemyPosition(
        enemy, interpolationAmount);
    const Vector3 center { position.x, ENEMY_RADIUS, position.y };
    const Vector3 ground { position.x, 0.026F, position.y };

    Color color { 188, 73, 57, 255 };
    Color accent { 255, 116, 61, 255 };
    Vector3 scale { 0.88F, 0.88F, 0.88F };
    switch (entry.role) {
    case HordeEnemyRole::Drifter:
        break;
    case HordeEnemyRole::Runner:
        color = Color { 213, 104, 43, 255 };
        accent = Color { 255, 180, 62, 255 };
        scale = Vector3 { 0.62F, 0.58F, 1.05F };
        break;
    case HordeEnemyRole::Caster:
        color = Color { 112, 61, 153, 255 };
        accent = Color { 210, 109, 239, 255 };
        scale = Vector3 { 0.72F, 1.12F, 0.72F };
        break;
    case HordeEnemyRole::Elite:
        color = Color { 157, 69, 148, 255 };
        accent = Color { 255, 103, 205, 255 };
        scale = Vector3 { 1.22F, 1.3F, 1.22F };
        break;
    }
    if (!isEnemyAlive(enemy)) {
        return;
    }
    drawActorShadow(center, ENEMY_RADIUS);
    if (enemy.hitFlashRemaining > 0.0F) {
        color = Color { 255, 238, 194, 255 };
        accent = WHITE;
    }
    const float footprint = std::max(scale.x, scale.z);
    DrawCircle3D(ground, ENEMY_RADIUS * footprint * 1.16F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { accent.r, accent.g, accent.b, 115 });
    const float facingAngle = -std::atan2(
        enemy.facing.y, enemy.facing.x) * RAD2DEG;
    const Model* bodyModel = &resources.enemyModel;
    switch (entry.role) {
    case HordeEnemyRole::Drifter:
        break;
    case HordeEnemyRole::Runner:
        bodyModel = &resources.runnerModel;
        break;
    case HordeEnemyRole::Caster:
        bodyModel = &resources.casterModel;
        break;
    case HordeEnemyRole::Elite:
        bodyModel = &resources.eliteModel;
        break;
    }
    DrawModelEx(*bodyModel, center, Vector3 { 0.0F, 1.0F, 0.0F },
        facingAngle, scale, color);

    if (entry.role == HordeEnemyRole::Drifter) {
        const Vector3 eye {
            center.x + enemy.facing.x * 0.66F,
            center.y + 0.08F,
            center.z + enemy.facing.y * 0.66F
        };
        DrawCylinderEx(center, eye, 0.13F, 0.045F, 7, accent);
        DrawSphere(eye, 0.085F, accent);
    } else if (entry.role == HordeEnemyRole::Runner) {
        const Vector3 snout {
            center.x + enemy.facing.x * 0.85F,
            center.y * 0.72F,
            center.z + enemy.facing.y * 0.85F
        };
        DrawCylinderEx(center, snout, 0.2F, 0.04F, 7, accent);
        const Vector3 tail {
            center.x - enemy.facing.x * 0.92F,
            center.y * 0.72F,
            center.z - enemy.facing.y * 0.92F
        };
        DrawCylinderEx(center, tail, 0.11F, 0.025F, 6,
            Color { accent.r, accent.g, accent.b, 170 });
    } else if (entry.role == HordeEnemyRole::Caster) {
        const Vector3 crown { center.x, center.y + 0.64F, center.z };
        DrawCircle3D(crown, 0.55F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F, accent);
        DrawSphere(crown, 0.12F, accent);
        const float orbit = static_cast<float>(GetTime()) * 2.2F
            + static_cast<float>(entry.id % 11U);
        for (int mote = 0; mote < 3; ++mote) {
            const float angle = orbit + static_cast<float>(mote) * 2.0F * PI / 3.0F;
            DrawSphere(Vector3 {
                           crown.x + std::cos(angle) * 0.52F,
                           crown.y + std::sin(angle * 2.0F) * 0.11F,
                           crown.z + std::sin(angle) * 0.52F },
                0.075F, accent);
        }
    } else if (entry.role == HordeEnemyRole::Elite) {
        DrawCircle3D(Vector3 { center.x, center.y + 0.12F, center.z },
            ENEMY_RADIUS * 1.38F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F, accent);
        DrawSphereWires(center, ENEMY_RADIUS * 1.4F,
            7, 10, Color { accent.r, accent.g, accent.b, 180 });
        const Vector2 side { -enemy.facing.y, enemy.facing.x };
        for (const float sign : { -1.0F, 1.0F }) {
            const Vector3 hornBase {
                center.x + side.x * 0.7F * sign,
                center.y + 0.5F,
                center.z + side.y * 0.7F * sign
            };
            DrawCylinderEx(hornBase,
                Vector3 { hornBase.x + side.x * 0.38F * sign,
                    hornBase.y + 0.48F,
                    hornBase.z + side.y * 0.38F * sign },
                0.13F, 0.025F, 6, accent);
        }
    }

    if (entry.role == HordeEnemyRole::Caster
        || entry.role == HordeEnemyRole::Elite) {
        const Vector3 aimEnd {
            center.x + enemy.facing.x * 1.3F,
            center.y,
            center.z + enemy.facing.y * 1.3F
        };
        DrawCylinderEx(center, aimEnd, 0.13F, 0.035F, 8, accent);
    }
}
