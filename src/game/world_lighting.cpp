#include "world_lighting.hpp"

#include "directional_shader.hpp"

namespace {

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
    const float lightColor[3] { 0.92F, 0.74F, 0.54F };
    const float groundAmbient[3] { 0.055F, 0.075F, 0.095F };
    const float skyAmbient[3] { 0.22F, 0.29F, 0.32F };
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
{
    clearPointLights();
    setMaterial(0.0F);
}

WorldLighting::~WorldLighting()
{
    UnloadShader(lightingShader);
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
