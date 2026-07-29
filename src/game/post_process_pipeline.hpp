#pragma once

#include "raylib.h"

struct PostProcessEffects {
    float damageAmount = 0.0F;
    float dashAmount = 0.0F;
    float energyPulse = 0.0F;
};

class PostProcessPipeline {
public:
    PostProcessPipeline();
    ~PostProcessPipeline();

    PostProcessPipeline(const PostProcessPipeline&) = delete;
    PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;

    void beginScene(Color background);
    void endScene() const;
    void process();
    void present(PostProcessEffects effects) const;

private:
    void ensureTargets();

    Shader bloomExtractShader {};
    Shader bloomBlurShader {};
    Shader compositeShader {};
    RenderTexture2D sceneTarget {};
    RenderTexture2D bloomTargetA {};
    RenderTexture2D bloomTargetB {};
    int blurDirectionLocation = -1;
    int compositeResolutionLocation = -1;
    int compositeTimeLocation = -1;
    int compositeDamageLocation = -1;
    int compositeDashLocation = -1;
    int compositeEnergyLocation = -1;
    int width = 0;
    int height = 0;
};
