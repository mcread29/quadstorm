#pragma once

#include "raylib.h"

class WorldLighting {
public:
    WorldLighting();
    ~WorldLighting();

    WorldLighting(const WorldLighting&) = delete;
    WorldLighting& operator=(const WorldLighting&) = delete;

    [[nodiscard]] Shader shader() const { return lightingShader; }
    [[nodiscard]] Shader shadowShader() const { return depthShader; }
    [[nodiscard]] bool shadowsAvailable() const { return shadowMap.id != 0; }

    bool beginShadowPass(Vector3 focus);
    void endShadowPass();
    void update(const Camera3D& camera, Color fogColor) const;
    void setPointLights(Vector3 positionA, Vector3 colorA,
        Vector3 positionB, Vector3 colorB) const;
    void clearPointLights() const;
    void setMaterial(float kind) const;

private:
    Shader lightingShader {};
    Shader depthShader {};
    RenderTexture2D shadowMap {};
    int cameraPositionLocation = -1;
    int cameraTargetLocation = -1;
    int fogColorLocation = -1;
    int materialKindLocation = -1;
    int pointLightPositionALocation = -1;
    int pointLightColorALocation = -1;
    int pointLightPositionBLocation = -1;
    int pointLightColorBLocation = -1;
    int lightViewProjectionLocation = -1;
    int shadowMapLocation = -1;
    int shadowTexelSizeLocation = -1;
    int shadowsEnabledLocation = -1;
};
