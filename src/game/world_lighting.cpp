#include "world_lighting.hpp"

#include "directional_shader.hpp"
#include "shadow_map_shader.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "raymath.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#include "rlgl.h"

namespace {

constexpr int SHADOW_MAP_RESOLUTION = 1024;
constexpr float SHADOW_CAMERA_SIZE = 56.0F;
constexpr float SHADOW_CAMERA_DISTANCE = 42.0F;
constexpr int SHADOW_TEXTURE_SLOT = 7;

RenderTexture2D loadShadowMap()
{
    RenderTexture2D target {};
    target.id = rlLoadFramebuffer();
    target.texture.width = SHADOW_MAP_RESOLUTION;
    target.texture.height = SHADOW_MAP_RESOLUTION;
    if (target.id == 0) {
        return target;
    }

    target.depth.id = rlLoadTextureDepth(
        SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION, false);
    target.depth.width = SHADOW_MAP_RESOLUTION;
    target.depth.height = SHADOW_MAP_RESOLUTION;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id,
        RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(target.id)) {
        UnloadRenderTexture(target);
        return RenderTexture2D {};
    }

    SetTextureFilter(target.depth, TEXTURE_FILTER_POINT);
    SetTextureWrap(target.depth, TEXTURE_WRAP_CLAMP);
    return target;
}

Shader loadDirectionalShader()
{
    Shader shader = LoadShaderFromMemory(
        LIGHTING_VERTEX_SHADER, LIGHTING_FRAGMENT_SHADER);
    shader.locs[SHADER_LOC_MATRIX_MODEL]
        = GetShaderLocation(shader, "matModel");
    const int lightDirectionLocation = GetShaderLocation(shader, "lightDirection");
    const int lightColorLocation = GetShaderLocation(shader, "lightColor");
    const int groundAmbientLocation = GetShaderLocation(shader, "groundAmbient");
    const int skyAmbientLocation = GetShaderLocation(shader, "skyAmbient");
    const float lightDirection[3] {
        DIRECTIONAL_LIGHT.x,
        DIRECTIONAL_LIGHT.y,
        DIRECTIONAL_LIGHT.z
    };
    const float lightColor[3] { 1.05F, 0.78F, 0.52F };
    const float groundAmbient[3] { 0.025F, 0.042F, 0.055F };
    const float skyAmbient[3] { 0.19F, 0.27F, 0.31F };
    SetShaderValue(shader, lightDirectionLocation,
        lightDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, lightColorLocation,
        lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, groundAmbientLocation,
        groundAmbient, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyAmbientLocation,
        skyAmbient, SHADER_UNIFORM_VEC3);
    return shader;
}

} // namespace

WorldLighting::WorldLighting()
    : lightingShader(loadDirectionalShader())
    , depthShader(LoadShaderFromMemory(
          SHADOW_DEPTH_VERTEX_SHADER, SHADOW_DEPTH_FRAGMENT_SHADER))
    , shadowMap(loadShadowMap())
    , cameraPositionLocation(GetShaderLocation(
          lightingShader, "cameraPosition"))
    , cameraTargetLocation(GetShaderLocation(lightingShader, "cameraTarget"))
    , fogColorLocation(GetShaderLocation(lightingShader, "fogColor"))
    , materialKindLocation(GetShaderLocation(lightingShader, "materialKind"))
    , pointLightPositionALocation(GetShaderLocation(
          lightingShader, "pointLightPositionA"))
    , pointLightColorALocation(GetShaderLocation(
          lightingShader, "pointLightColorA"))
    , pointLightPositionBLocation(GetShaderLocation(
          lightingShader, "pointLightPositionB"))
    , pointLightColorBLocation(GetShaderLocation(
          lightingShader, "pointLightColorB"))
    , lightViewProjectionLocation(GetShaderLocation(
          lightingShader, "lightViewProjection"))
    , shadowMapLocation(GetShaderLocation(lightingShader, "shadowMap"))
    , shadowTexelSizeLocation(GetShaderLocation(
          lightingShader, "shadowTexelSize"))
    , shadowsEnabledLocation(GetShaderLocation(
          lightingShader, "shadowsEnabled"))
{
    clearPointLights();
    setMaterial(0.0F);
    const float shadowTexelSize[2] {
        1.0F / static_cast<float>(SHADOW_MAP_RESOLUTION),
        1.0F / static_cast<float>(SHADOW_MAP_RESOLUTION)
    };
    const float shadowsEnabled = shadowsAvailable() ? 1.0F : 0.0F;
    SetShaderValue(lightingShader, shadowTexelSizeLocation,
        shadowTexelSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(lightingShader, shadowsEnabledLocation,
        &shadowsEnabled, SHADER_UNIFORM_FLOAT);
}

WorldLighting::~WorldLighting()
{
    if (shadowMap.id != 0) {
        UnloadRenderTexture(shadowMap);
    }
    UnloadShader(depthShader);
    UnloadShader(lightingShader);
}

bool WorldLighting::beginShadowPass(Vector3 focus)
{
    if (!shadowsAvailable()) {
        return false;
    }

    const Vector3 lightDirection = Vector3Normalize(DIRECTIONAL_LIGHT);
    Camera3D lightCamera {};
    lightCamera.position = Vector3Subtract(focus,
        Vector3Scale(lightDirection, SHADOW_CAMERA_DISTANCE));
    lightCamera.target = focus;
    lightCamera.up = Vector3 { 0.0F, 1.0F, 0.0F };
    lightCamera.fovy = SHADOW_CAMERA_SIZE;
    lightCamera.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(shadowMap);
    ClearBackground(WHITE);
    BeginMode3D(lightCamera);
    BeginShaderMode(depthShader);
    return true;
}

void WorldLighting::endShadowPass()
{
    EndShaderMode();
    const Matrix lightView = rlGetMatrixModelview();
    const Matrix lightProjection = rlGetMatrixProjection();
    EndMode3D();
    EndTextureMode();

    const Matrix lightViewProjection = MatrixMultiply(
        lightView, lightProjection);
    SetShaderValueMatrix(lightingShader,
        lightViewProjectionLocation, lightViewProjection);
    rlEnableShader(lightingShader.id);
    rlActiveTextureSlot(SHADOW_TEXTURE_SLOT);
    rlEnableTexture(shadowMap.depth.id);
    rlSetUniform(shadowMapLocation, &SHADOW_TEXTURE_SLOT,
        SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
}

void WorldLighting::update(const Camera3D& camera, Color fogColor) const
{
    const float cameraPosition[3] {
        camera.position.x, camera.position.y, camera.position.z
    };
    const float cameraTarget[3] {
        camera.target.x, camera.target.y, camera.target.z
    };
    const float normalizedFogColor[3] {
        static_cast<float>(fogColor.r) / 255.0F,
        static_cast<float>(fogColor.g) / 255.0F,
        static_cast<float>(fogColor.b) / 255.0F
    };
    SetShaderValue(lightingShader, cameraPositionLocation,
        cameraPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, cameraTargetLocation,
        cameraTarget, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, fogColorLocation,
        normalizedFogColor, SHADER_UNIFORM_VEC3);
}

void WorldLighting::setPointLights(Vector3 positionA, Vector3 colorA,
    Vector3 positionB, Vector3 colorB) const
{
    SetShaderValue(lightingShader, pointLightPositionALocation,
        &positionA.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightColorALocation,
        &colorA.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightPositionBLocation,
        &positionB.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightColorBLocation,
        &colorB.x, SHADER_UNIFORM_VEC3);
}

void WorldLighting::clearPointLights() const
{
    setPointLights(Vector3 {}, Vector3 {}, Vector3 {}, Vector3 {});
}

void WorldLighting::setMaterial(float kind) const
{
    SetShaderValue(lightingShader, materialKindLocation,
        &kind, SHADER_UNIFORM_FLOAT);
}
