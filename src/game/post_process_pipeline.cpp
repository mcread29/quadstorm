#include "post_process_pipeline.hpp"

#include "post_process_shader.hpp"

#include "rlgl.h"

#include <algorithm>

namespace {

void drawRenderTexture(RenderTexture2D target, float width, float height)
{
    DrawTexturePro(target.texture,
        Rectangle { 0.0F, 0.0F,
            static_cast<float>(target.texture.width),
            -static_cast<float>(target.texture.height) },
        Rectangle { 0.0F, 0.0F, width, height },
        Vector2 {}, 0.0F, WHITE);
}

RenderTexture2D loadDepthTextureTarget(
    int width, int height, bool& depthTextureAvailable)
{
    RenderTexture2D target {};
    target.id = rlLoadFramebuffer();
    if (target.id == 0) {
        depthTextureAvailable = false;
        return LoadRenderTexture(width, height);
    }

    target.texture.id = rlLoadTexture(nullptr, width, height,
        PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    target.texture.mipmaps = 1;

    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;

    rlFramebufferAttach(target.id, target.texture.id,
        RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id,
        RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    depthTextureAvailable = rlFramebufferComplete(target.id);
    if (!depthTextureAvailable) {
        UnloadRenderTexture(target);
        return LoadRenderTexture(width, height);
    }

    SetTextureFilter(target.depth, TEXTURE_FILTER_POINT);
    SetTextureWrap(target.depth, TEXTURE_WRAP_CLAMP);
    return target;
}

} // namespace

PostProcessPipeline::PostProcessPipeline()
    : bloomExtractShader(LoadShaderFromMemory(
          nullptr, BLOOM_EXTRACT_FRAGMENT_SHADER))
    , bloomBlurShader(LoadShaderFromMemory(
          nullptr, BLOOM_BLUR_FRAGMENT_SHADER))
    , compositeShader(LoadShaderFromMemory(
          nullptr, COMPOSITE_FRAGMENT_SHADER))
    , blurDirectionLocation(GetShaderLocation(
          bloomBlurShader, "blurDirection"))
    , compositeResolutionLocation(GetShaderLocation(
          compositeShader, "resolution"))
    , compositeDepthTextureLocation(GetShaderLocation(
          compositeShader, "depthTexture"))
    , compositeDepthEnabledLocation(GetShaderLocation(
          compositeShader, "depthEnabled"))
    , compositeTimeLocation(GetShaderLocation(compositeShader, "time"))
    , compositeDamageLocation(GetShaderLocation(
          compositeShader, "damageAmount"))
    , compositeDashLocation(GetShaderLocation(
          compositeShader, "dashAmount"))
    , compositeEnergyLocation(GetShaderLocation(
          compositeShader, "energyPulse"))
{
    ensureTargets();
}

PostProcessPipeline::~PostProcessPipeline()
{
    if (sceneTarget.id != 0) {
        UnloadRenderTexture(sceneTarget);
        UnloadRenderTexture(bloomTargetA);
        UnloadRenderTexture(bloomTargetB);
    }
    UnloadShader(compositeShader);
    UnloadShader(bloomBlurShader);
    UnloadShader(bloomExtractShader);
}

void PostProcessPipeline::beginScene(Color background)
{
    ensureTargets();
    BeginTextureMode(sceneTarget);
    ClearBackground(background);
}

void PostProcessPipeline::endScene() const
{
    EndTextureMode();
}

void PostProcessPipeline::ensureTargets()
{
    const int targetWidth = std::max(GetScreenWidth(), 1);
    const int targetHeight = std::max(GetScreenHeight(), 1);
    if (sceneTarget.id != 0
        && targetWidth == width && targetHeight == height) {
        return;
    }
    if (sceneTarget.id != 0) {
        UnloadRenderTexture(sceneTarget);
        UnloadRenderTexture(bloomTargetA);
        UnloadRenderTexture(bloomTargetB);
    }

    width = targetWidth;
    height = targetHeight;
    sceneTarget = loadDepthTextureTarget(
        width, height, depthTextureAvailable);
    bloomTargetA = LoadRenderTexture(
        std::max(width / 2, 1), std::max(height / 2, 1));
    bloomTargetB = LoadRenderTexture(
        std::max(width / 2, 1), std::max(height / 2, 1));
    for (Texture2D texture : { sceneTarget.texture,
             bloomTargetA.texture, bloomTargetB.texture }) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
}

void PostProcessPipeline::process()
{
    const float bloomWidth = static_cast<float>(bloomTargetA.texture.width);
    const float bloomHeight = static_cast<float>(bloomTargetA.texture.height);

    BeginTextureMode(bloomTargetA);
    ClearBackground(BLANK);
    BeginShaderMode(bloomExtractShader);
    drawRenderTexture(sceneTarget, bloomWidth, bloomHeight);
    EndShaderMode();
    EndTextureMode();

    for (int pass = 0; pass < 3; ++pass) {
        const float radius = 1.0F + static_cast<float>(pass) * 0.55F;
        const float horizontal[2] { radius / bloomWidth, 0.0F };
        SetShaderValue(bloomBlurShader, blurDirectionLocation,
            horizontal, SHADER_UNIFORM_VEC2);
        BeginTextureMode(bloomTargetB);
        ClearBackground(BLANK);
        BeginShaderMode(bloomBlurShader);
        drawRenderTexture(bloomTargetA, bloomWidth, bloomHeight);
        EndShaderMode();
        EndTextureMode();

        const float vertical[2] { 0.0F, radius / bloomHeight };
        SetShaderValue(bloomBlurShader, blurDirectionLocation,
            vertical, SHADER_UNIFORM_VEC2);
        BeginTextureMode(bloomTargetA);
        ClearBackground(BLANK);
        BeginShaderMode(bloomBlurShader);
        drawRenderTexture(bloomTargetB, bloomWidth, bloomHeight);
        EndShaderMode();
        EndTextureMode();
    }
}

void PostProcessPipeline::present(PostProcessEffects effects) const
{
    const float resolution[2] {
        static_cast<float>(width),
        static_cast<float>(height)
    };
    const float time = static_cast<float>(GetTime());
    SetShaderValue(compositeShader, compositeResolutionLocation,
        resolution, SHADER_UNIFORM_VEC2);
    const float depthEnabled = depthTextureAvailable ? 1.0F : 0.0F;
    SetShaderValue(compositeShader, compositeDepthEnabledLocation,
        &depthEnabled, SHADER_UNIFORM_FLOAT);
    if (depthTextureAvailable) {
        SetShaderValueTexture(compositeShader,
            compositeDepthTextureLocation, sceneTarget.depth);
    }
    SetShaderValue(compositeShader, compositeTimeLocation,
        &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeDamageLocation,
        &effects.damageAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeDashLocation,
        &effects.dashAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeEnergyLocation,
        &effects.energyPulse, SHADER_UNIFORM_FLOAT);

    BeginShaderMode(compositeShader);
    drawRenderTexture(sceneTarget,
        static_cast<float>(width), static_cast<float>(height));
    EndShaderMode();

    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bloomTargetA.texture,
        Rectangle { 0.0F, 0.0F,
            static_cast<float>(bloomTargetA.texture.width),
            -static_cast<float>(bloomTargetA.texture.height) },
        Rectangle { 0.0F, 0.0F,
            static_cast<float>(width), static_cast<float>(height) },
        Vector2 {}, 0.0F, Color { 255, 255, 255, 145 });
    EndBlendMode();
}
