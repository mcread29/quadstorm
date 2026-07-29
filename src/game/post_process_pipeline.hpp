#pragma once

#include "raylib.h"

struct PostProcessEffects {
    float damageAmount = 0.0F;
    float dashAmount = 0.0F;
    float energyPulse = 0.0F;
    Vector2 playerScreenPosition {};
    Vector2 objectiveScreenPosition {};
};

class PostProcessPipeline {
public:
    PostProcessPipeline();
    ~PostProcessPipeline();

    PostProcessPipeline(const PostProcessPipeline&) = delete;
    PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

    void beginScene(Color background);
    void endScene() const;
    void beginEmissive();
    void endEmissive() const;
    void beginActorMask();
    void endActorMask() const;
    void process(PostProcessEffects effects);
    void present() const;
    Shader actorMaskShader() const { return maskShader; }

private:
    void ensureTargets();

    Shader maskShader {};
    Shader bloomExtractShader {};
    Shader bloomBlurShader {};
    Shader compositeShader {};
    Shader finalShader {};
    RenderTexture2D sceneTarget {};
    RenderTexture2D emissiveTarget {};
    RenderTexture2D actorMaskTarget {};
    RenderTexture2D bloomTargetA {};
    RenderTexture2D bloomTargetB {};
    RenderTexture2D compositeTarget {};
    int bloomEmissiveTextureLocation = -1;
    int blurDirectionLocation = -1;
    int compositeResolutionLocation = -1;
    int compositeDepthTextureLocation = -1;
    int compositeBloomTextureLocation = -1;
    int compositeActorMaskTextureLocation = -1;
    int compositeDepthEnabledLocation = -1;
    int compositeEnergyLocation = -1;
    int finalResolutionLocation = -1;
    int finalTimeLocation = -1;
    int finalDamageLocation = -1;
    int finalDashLocation = -1;
    int finalEnergyLocation = -1;
    int finalPlayerCenterLocation = -1;
    int finalObjectiveCenterLocation = -1;
    int width = 0;
    int height = 0;
    bool depthTextureAvailable = false;
};
