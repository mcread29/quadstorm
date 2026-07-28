#include "prototype_renderer.hpp"

#include "arena.hpp"
#include "directional_shader.hpp"

#include <cmath>

namespace {

Shader loadDirectionalShader()
{
    Shader shader = LoadShaderFromMemory(
        LIGHTING_VERTEX_SHADER, LIGHTING_FRAGMENT_SHADER);
    const int lightDirectionLocation = GetShaderLocation(shader, "lightDirection");
    const int lightColorLocation = GetShaderLocation(shader, "lightColor");
    const int ambientColorLocation = GetShaderLocation(shader, "ambientColor");
    const float lightDirection[3] {
        DIRECTIONAL_LIGHT.x,
        DIRECTIONAL_LIGHT.y,
        DIRECTIONAL_LIGHT.z
    };
    const float lightColor[3] { 0.78F, 0.75F, 0.68F };
    const float ambientColor[3] { 0.38F, 0.43F, 0.46F };
    SetShaderValue(shader, lightDirectionLocation,
        lightDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, lightColorLocation,
        lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, ambientColorLocation,
        ambientColor, SHADER_UNIFORM_VEC3);
    return shader;
}

} // namespace

PrototypeRenderer::PrototypeRenderer()
    : lightingShader(loadDirectionalShader())
    , groundModel(LoadModelFromMesh(GenMeshPlane(80.0F, 80.0F, 1, 1)))
    , wallModel(LoadModelFromMesh(GenMeshCube(1.0F, 2.0F, 0.18F)))
    , playerModel(LoadModelFromMesh(GenMeshSphere(PLAYER_RADIUS, 16, 24)))
    , projectileModel(LoadModelFromMesh(GenMeshSphere(1.0F, 8, 12)))
    , targetModel(LoadModelFromMesh(GenMeshSphere(TARGET_RADIUS, 16, 24)))
    , shadowModel(LoadModelFromMesh(
          GenMeshCylinder(PLAYER_RADIUS * 1.05F, 0.01F, 32)))
{
    groundModel.materials[0].shader = lightingShader;
    wallModel.materials[0].shader = lightingShader;
    playerModel.materials[0].shader = lightingShader;
    projectileModel.materials[0].shader = lightingShader;
    targetModel.materials[0].shader = lightingShader;
}

PrototypeRenderer::~PrototypeRenderer()
{
    UnloadModel(shadowModel);
    UnloadModel(targetModel);
    UnloadModel(projectileModel);
    UnloadModel(playerModel);
    UnloadModel(wallModel);
    UnloadModel(groundModel);
    UnloadShader(lightingShader);
}

void PrototypeRenderer::draw(const Camera3D& camera, const Player& player,
    Vector3 aimPoint, const ProjectilePool& projectiles, const Target& target,
    float interpolationAmount) const
{
    BeginDrawing();
    ClearBackground(Color { 27, 39, 45, 255 });

    BeginMode3D(camera);
    DrawModel(groundModel, Vector3 { 0.0F, -0.015F, 0.0F }, 1.0F,
        Color { 64, 104, 105, 255 });
    DrawGrid(80, 1.0F);
    drawArena();
    drawPlayerShadow(player);
    DrawSphere(Vector3 { aimPoint.x, 0.06F, aimPoint.z }, 0.12F,
        Color { 225, 241, 232, 210 });
    drawTarget(target);
    drawProjectiles(projectiles, interpolationAmount);
    drawPlayer(player);
    EndMode3D();

    DrawRectangle(16, 16, 530, 108, Color { 8, 25, 30, 220 });
    DrawText("WALLED TARGET PRACTICE", 28, 27, 22,
        Color { 225, 241, 232, 255 });
    DrawText("WASD move  |  mouse aim  |  hold LMB fire", 28, 60, 17,
        Color { 151, 193, 190, 255 });
    if (target.health > 0) {
        DrawText(TextFormat(
                     "Target %i/%i  |  active %i  |  10/s  %.0fu/s  r%.2f",
                     target.health, TARGET_MAX_HEALTH,
                     static_cast<int>(projectiles.activeCount()),
                     projectiles.profile().speed, projectiles.profile().radius),
            28, 91, 17, Color { 225, 241, 232, 255 });
    } else {
        DrawText(TextFormat("Target resetting in %.1fs  |  active %i",
                     target.resetRemaining,
                     static_cast<int>(projectiles.activeCount())),
            28, 91, 17, Color { 255, 197, 121, 255 });
    }
    DrawFPS(GetScreenWidth() - 96, 20);

    EndDrawing();
}

void PrototypeRenderer::drawArena() const
{
    constexpr float wallHeight = 2.0F;
    constexpr float wallThickness = 0.18F;
    constexpr Color wallColor { 94, 116, 120, 255 };

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
                + outwardNormal.x * wallThickness * 0.5F,
            wallHeight * 0.5F,
            (wall.start.y + wall.end.y) * 0.5F
                + outwardNormal.y * wallThickness * 0.5F
        };
        const float angle = -std::atan2(direction.y, direction.x) * RAD2DEG;
        DrawModelEx(wallModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, angle,
            Vector3 { wallLength, 1.0F, 1.0F }, wallColor);
    }
}

void PrototypeRenderer::drawPlayerShadow(const Player& player) const
{
    const float groundDistance = player.position.y / -DIRECTIONAL_LIGHT.y;
    const Vector3 shadowPosition {
        player.position.x + DIRECTIONAL_LIGHT.x * groundDistance,
        0.006F,
        player.position.z + DIRECTIONAL_LIGHT.z * groundDistance
    };
    const float shadowAngle = std::atan2(
        DIRECTIONAL_LIGHT.z, DIRECTIONAL_LIGHT.x) * RAD2DEG;
    DrawModelEx(shadowModel, shadowPosition, Vector3 { 0.0F, 1.0F, 0.0F },
        shadowAngle, Vector3 { 1.35F, 1.0F, 0.78F },
        Color { 7, 15, 17, 82 });
}

void PrototypeRenderer::drawTarget(const Target& target) const
{
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
    DrawModel(targetModel, targetCenter, targetScale, targetColor);
    if (flashAmount > 0.0F) {
        DrawSphereWires(targetCenter,
            TARGET_RADIUS * (1.15F + (1.0F - flashAmount) * 0.45F),
            8, 12, Color { 255, 245, 210, 210 });
    }
}

void PrototypeRenderer::drawProjectiles(
    const ProjectilePool& projectiles, float interpolationAmount) const
{
    constexpr Color projectileColor { 117, 226, 255, 255 };
    constexpr Color trailColor { 117, 226, 255, 155 };
    constexpr float projectileHeight = PLAYER_RADIUS;
    constexpr float trailLength = 0.5F;

    for (const Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }

        const Vector2 position = interpolateProjectilePosition(
            projectile, interpolationAmount);
        const float inverseSpeed = 1.0F / projectiles.profile().speed;
        const Vector3 head { position.x, projectileHeight, position.y };
        const Vector3 tail {
            head.x - projectile.velocity.x * inverseSpeed * trailLength,
            projectileHeight,
            head.z - projectile.velocity.y * inverseSpeed * trailLength
        };
        DrawLine3D(tail, head, trailColor);
        DrawModel(projectileModel, head,
            projectiles.profile().radius, projectileColor);
    }
}

void PrototypeRenderer::drawPlayer(const Player& player) const
{
    constexpr Color bodyColor { 239, 180, 74, 255 };
    constexpr Color facingColor { 255, 231, 145, 255 };

    DrawModel(playerModel, player.position, 1.0F, bodyColor);

    const Vector3 noseStart {
        player.position.x + player.facing.x * PLAYER_RADIUS * 0.55F,
        player.position.y,
        player.position.z + player.facing.y * PLAYER_RADIUS * 0.55F
    };
    const Vector3 noseEnd {
        player.position.x + player.facing.x * PLAYER_FACING_MARKER_DISTANCE,
        player.position.y,
        player.position.z + player.facing.y * PLAYER_FACING_MARKER_DISTANCE
    };
    BeginShaderMode(lightingShader);
    DrawCylinderEx(noseStart, noseEnd, 0.18F, 0.05F, 12, facingColor);
    EndShaderMode();
}
