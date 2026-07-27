#include "prototype_renderer.hpp"

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
    , playerModel(LoadModelFromMesh(GenMeshSphere(PLAYER_RADIUS, 16, 24)))
    , projectileModel(LoadModelFromMesh(
          GenMeshSphere(PROJECTILE_RADIUS, 8, 12)))
    , shadowModel(LoadModelFromMesh(
          GenMeshCylinder(PLAYER_RADIUS * 1.05F, 0.01F, 32)))
{
    groundModel.materials[0].shader = lightingShader;
    playerModel.materials[0].shader = lightingShader;
    projectileModel.materials[0].shader = lightingShader;
}

PrototypeRenderer::~PrototypeRenderer()
{
    UnloadModel(shadowModel);
    UnloadModel(projectileModel);
    UnloadModel(playerModel);
    UnloadModel(groundModel);
    UnloadShader(lightingShader);
}

void PrototypeRenderer::draw(const Camera3D& camera, const Player& player,
    Vector3 aimPoint, const ProjectilePool& projectiles,
    float interpolationAmount) const
{
    BeginDrawing();
    ClearBackground(Color { 27, 39, 45, 255 });

    BeginMode3D(camera);
    DrawModel(groundModel, Vector3 { 0.0F, -0.015F, 0.0F }, 1.0F,
        Color { 64, 104, 105, 255 });
    DrawGrid(80, 1.0F);
    drawPlayerShadow(player);
    DrawSphere(Vector3 { aimPoint.x, 0.06F, aimPoint.z }, 0.12F,
        Color { 225, 241, 232, 210 });
    drawProjectiles(projectiles, interpolationAmount);
    drawPlayer(player);
    EndMode3D();

    DrawRectangle(16, 16, 420, 76, Color { 8, 25, 30, 220 });
    DrawText("SHOOTING PROTOTYPE", 28, 27, 22, Color { 225, 241, 232, 255 });
    DrawText("WASD move  |  mouse aim  |  hold LMB fire", 28, 60, 17,
        Color { 151, 193, 190, 255 });
    DrawFPS(GetScreenWidth() - 96, 20);

    EndDrawing();
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
        const float inverseSpeed = 1.0F / PROJECTILE_SPEED;
        const Vector3 head { position.x, projectileHeight, position.y };
        const Vector3 tail {
            head.x - projectile.velocity.x * inverseSpeed * trailLength,
            projectileHeight,
            head.z - projectile.velocity.y * inverseSpeed * trailLength
        };
        DrawLine3D(tail, head, trailColor);
        DrawModel(projectileModel, head, 1.0F, projectileColor);
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
