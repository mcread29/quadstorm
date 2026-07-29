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

Vector2 normalizedScreenPosition(Vector2 position, int width, int height)
{
    return Vector2 {
        position.x / static_cast<float>(width),
        1.0F - position.y / static_cast<float>(height)
    };
}

} // namespace

PostProcessPipeline::PostProcessPipeline()
    : maskShader(LoadShaderFromMemory(
          nullptr, ACTOR_MASK_FRAGMENT_SHADER))
    , bloomExtractShader(LoadShaderFromMemory(
          nullptr, BLOOM_EXTRACT_FRAGMENT_SHADER))
    , bloomBlurShader(LoadShaderFromMemory(
          nullptr, BLOOM_BLUR_FRAGMENT_SHADER))
    , compositeShader(LoadShaderFromMemory(
          nullptr, COMPOSITE_FRAGMENT_SHADER))
    , finalShader(LoadShaderFromMemory(nullptr, FINAL_FRAGMENT_SHADER))
    , bloomEmissiveTextureLocation(GetShaderLocation(
          bloomExtractShader, "emissiveTexture"))
    , blurDirectionLocation(GetShaderLocation(
          bloomBlurShader, "blurDirection"))
    , compositeResolutionLocation(GetShaderLocation(
          compositeShader, "resolution"))
    , compositeDepthTextureLocation(GetShaderLocation(
          compositeShader, "depthTexture"))
    , compositeBloomTextureLocation(GetShaderLocation(
          compositeShader, "bloomTexture"))
    , compositeActorMaskTextureLocation(GetShaderLocation(
          compositeShader, "actorMaskTexture"))
    , compositeDepthEnabledLocation(GetShaderLocation(
          compositeShader, "depthEnabled"))
    , compositeEnergyLocation(GetShaderLocation(
          compositeShader, "energyPulse"))
    , finalResolutionLocation(GetShaderLocation(finalShader, "resolution"))
    , finalTimeLocation(GetShaderLocation(finalShader, "time"))
    , finalDamageLocation(GetShaderLocation(finalShader, "damageAmount"))
    , finalDashLocation(GetShaderLocation(finalShader, "dashAmount"))
    , finalEnergyLocation(GetShaderLocation(finalShader, "energyPulse"))
    , finalPlayerCenterLocation(GetShaderLocation(
          finalShader, "playerEffectCenter"))
    , finalObjectiveCenterLocation(GetShaderLocation(
          finalShader, "objectiveEffectCenter"))
{
    ensureTargets();
}

PostProcessPipeline::~PostProcessPipeline()
{
    if (sceneTarget.id != 0) {
        UnloadRenderTexture(compositeTarget);
        UnloadRenderTexture(bloomTargetB);
        UnloadRenderTexture(bloomTargetA);
        UnloadRenderTexture(actorMaskTarget);
        UnloadRenderTexture(emissiveTarget);
        UnloadRenderTexture(sceneTarget);
    }
    UnloadShader(finalShader);
    UnloadShader(compositeShader);
    UnloadShader(bloomBlurShader);
    UnloadShader(bloomExtractShader);
    UnloadShader(maskShader);
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

void PostProcessPipeline::beginEmissive()
{
    ensureTargets();
    BeginTextureMode(emissiveTarget);
    ClearBackground(BLACK);
}

void PostProcessPipeline::endEmissive() const
{
    EndTextureMode();
}

void PostProcessPipeline::beginActorMask()
{
    ensureTargets();
    BeginTextureMode(actorMaskTarget);
    ClearBackground(BLACK);
}

void PostProcessPipeline::endActorMask() const
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
        UnloadRenderTexture(compositeTarget);
        UnloadRenderTexture(bloomTargetB);
        UnloadRenderTexture(bloomTargetA);
        UnloadRenderTexture(actorMaskTarget);
        UnloadRenderTexture(emissiveTarget);
        UnloadRenderTexture(sceneTarget);
    }

    width = targetWidth;
    height = targetHeight;
    sceneTarget = loadDepthTextureTarget(
        width, height, depthTextureAvailable);
    emissiveTarget = LoadRenderTexture(width, height);
    actorMaskTarget = LoadRenderTexture(width, height);
    bloomTargetA = LoadRenderTexture(
        std::max(width / 2, 1), std::max(height / 2, 1));
    bloomTargetB = LoadRenderTexture(
        std::max(width / 2, 1), std::max(height / 2, 1));
    compositeTarget = LoadRenderTexture(width, height);
    for (Texture2D texture : { sceneTarget.texture,
             emissiveTarget.texture, actorMaskTarget.texture,
             bloomTargetA.texture, bloomTargetB.texture,
             compositeTarget.texture }) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
}

void PostProcessPipeline::process(PostProcessEffects effects)
{
    const float bloomWidth = static_cast<float>(bloomTargetA.texture.width);
    const float bloomHeight = static_cast<float>(bloomTargetA.texture.height);

    BeginTextureMode(bloomTargetA);
    ClearBackground(BLACK);
    BeginShaderMode(bloomExtractShader);
    SetShaderValueTexture(bloomExtractShader,
        bloomEmissiveTextureLocation, emissiveTarget.texture);
    drawRenderTexture(sceneTarget, bloomWidth, bloomHeight);
    EndShaderMode();
    EndTextureMode();

    for (int pass = 0; pass < 3; ++pass) {
        const float radius = 1.0F + static_cast<float>(pass) * 0.62F;
        const float horizontal[2] { radius / bloomWidth, 0.0F };
        SetShaderValue(bloomBlurShader, blurDirectionLocation,
            horizontal, SHADER_UNIFORM_VEC2);
        BeginTextureMode(bloomTargetB);
        ClearBackground(BLACK);
        BeginShaderMode(bloomBlurShader);
        drawRenderTexture(bloomTargetA, bloomWidth, bloomHeight);
        EndShaderMode();
        EndTextureMode();

        const float vertical[2] { 0.0F, radius / bloomHeight };
        SetShaderValue(bloomBlurShader, blurDirectionLocation,
            vertical, SHADER_UNIFORM_VEC2);
        BeginTextureMode(bloomTargetA);
        ClearBackground(BLACK);
        BeginShaderMode(bloomBlurShader);
        drawRenderTexture(bloomTargetB, bloomWidth, bloomHeight);
        EndShaderMode();
        EndTextureMode();
    }

    const float resolution[2] {
        static_cast<float>(width),
        static_cast<float>(height)
    };
    const float depthEnabled = depthTextureAvailable ? 1.0F : 0.0F;
    SetShaderValue(compositeShader, compositeResolutionLocation,
        resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(compositeShader, compositeDepthEnabledLocation,
        &depthEnabled, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeEnergyLocation,
        &effects.energyPulse, SHADER_UNIFORM_FLOAT);
    BeginTextureMode(compositeTarget);
    ClearBackground(BLACK);
    BeginShaderMode(compositeShader);
    if (depthTextureAvailable) {
        SetShaderValueTexture(compositeShader,
            compositeDepthTextureLocation, sceneTarget.depth);
    }
    SetShaderValueTexture(compositeShader,
        compositeBloomTextureLocation, bloomTargetA.texture);
    SetShaderValueTexture(compositeShader,
        compositeActorMaskTextureLocation, actorMaskTarget.texture);
    drawRenderTexture(sceneTarget,
        static_cast<float>(width), static_cast<float>(height));
    EndShaderMode();
    EndTextureMode();

    const float time = static_cast<float>(GetTime());
    const Vector2 playerCenter = normalizedScreenPosition(
        effects.playerScreenPosition, width, height);
    const Vector2 objectiveCenter = normalizedScreenPosition(
        effects.objectiveScreenPosition, width, height);
    SetShaderValue(finalShader, finalResolutionLocation,
        resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(finalShader, finalTimeLocation,
        &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(finalShader, finalDamageLocation,
        &effects.damageAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(finalShader, finalDashLocation,
        &effects.dashAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(finalShader, finalEnergyLocation,
        &effects.energyPulse, SHADER_UNIFORM_FLOAT);
    SetShaderValue(finalShader, finalPlayerCenterLocation,
        &playerCenter.x, SHADER_UNIFORM_VEC2);
    SetShaderValue(finalShader, finalObjectiveCenterLocation,
        &objectiveCenter.x, SHADER_UNIFORM_VEC2);
}

void PostProcessPipeline::present() const
{
    BeginShaderMode(finalShader);
    drawRenderTexture(compositeTarget,
        static_cast<float>(width), static_cast<float>(height));
    EndShaderMode();
}
