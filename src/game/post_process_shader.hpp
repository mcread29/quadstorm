#pragma once

#if defined(PLATFORM_WEB)
inline constexpr const char* BLOOM_EXTRACT_FRAGMENT_SHADER = R"(
#version 100
precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D emissiveTexture;
uniform vec4 colDiffuse;

void main()
{
    vec3 scene = texture2D(texture0, fragTexCoord).rgb;
    vec3 emissive = texture2D(emissiveTexture, fragTexCoord).rgb;
    float maximum = max(scene.r, max(scene.g, scene.b));
    float minimum = min(scene.r, min(scene.g, scene.b));
    float saturation = maximum - minimum;
    float selectiveHighlight = smoothstep(0.72, 1.0, maximum)
        * smoothstep(0.12, 0.42, saturation) * 0.18;
    vec3 bloom = emissive * 1.35 + scene * selectiveHighlight;
    gl_FragColor = vec4(bloom, 1.0) * colDiffuse * fragColor;
}
)";

inline constexpr const char* BLOOM_BLUR_FRAGMENT_SHADER = R"(
#version 100
precision mediump float;

varying vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 blurDirection;

void main()
{
    vec4 color = texture2D(texture0, fragTexCoord) * 0.227027;
    color += texture2D(texture0, fragTexCoord + blurDirection * 1.384615) * 0.316216;
    color += texture2D(texture0, fragTexCoord - blurDirection * 1.384615) * 0.316216;
    color += texture2D(texture0, fragTexCoord + blurDirection * 3.230769) * 0.070270;
    color += texture2D(texture0, fragTexCoord - blurDirection * 3.230769) * 0.070270;
    gl_FragColor = color;
}
)";

inline constexpr const char* COMPOSITE_FRAGMENT_SHADER = R"(
#version 100
precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D depthTexture;
uniform sampler2D bloomTexture;
uniform sampler2D actorMaskTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float depthEnabled;
uniform float energyPulse;

vec3 acesToneMap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b))
        / (color * (c * color + d) + e), 0.0, 1.0);
}

float occluder(float centerDepth, vec2 sampleUv)
{
    float difference = centerDepth - texture2D(depthTexture, sampleUv).r;
    return smoothstep(0.00007, 0.0017, difference)
        * (1.0 - smoothstep(0.007, 0.025, difference));
}

float ambientOcclusion(vec2 uv, vec2 texel)
{
    if (depthEnabled < 0.5) {
        return 0.0;
    }
    float centerDepth = texture2D(depthTexture, uv).r;
    if (centerDepth > 0.9998) {
        return 0.0;
    }
    vec2 diagonal = vec2(0.707107);
    float nearAmount = 0.0;
    vec2 nearStep = texel * 2.2;
    nearAmount += occluder(centerDepth, uv + vec2(nearStep.x, 0.0));
    nearAmount += occluder(centerDepth, uv - vec2(nearStep.x, 0.0));
    nearAmount += occluder(centerDepth, uv + vec2(0.0, nearStep.y));
    nearAmount += occluder(centerDepth, uv - vec2(0.0, nearStep.y));
    nearAmount += occluder(centerDepth, uv + nearStep * diagonal);
    nearAmount += occluder(centerDepth, uv - nearStep * diagonal);
    nearAmount += occluder(centerDepth,
        uv + vec2(nearStep.x, -nearStep.y) * diagonal);
    nearAmount += occluder(centerDepth,
        uv + vec2(-nearStep.x, nearStep.y) * diagonal);

    float wideAmount = 0.0;
    vec2 wideStep = texel * 6.4;
    wideAmount += occluder(centerDepth, uv + vec2(wideStep.x, 0.0));
    wideAmount += occluder(centerDepth, uv - vec2(wideStep.x, 0.0));
    wideAmount += occluder(centerDepth, uv + vec2(0.0, wideStep.y));
    wideAmount += occluder(centerDepth, uv - vec2(0.0, wideStep.y));
    return clamp(nearAmount * 0.095 + wideAmount * 0.075, 0.0, 1.0);
}

float depthEdge(vec2 uv, vec2 texel)
{
    if (depthEnabled < 0.5) {
        return 0.0;
    }
    float center = texture2D(depthTexture, uv).r;
    float difference = 0.0;
    difference = max(difference, abs(center
        - texture2D(depthTexture, uv + vec2(texel.x, 0.0)).r));
    difference = max(difference, abs(center
        - texture2D(depthTexture, uv - vec2(texel.x, 0.0)).r));
    difference = max(difference, abs(center
        - texture2D(depthTexture, uv + vec2(0.0, texel.y)).r));
    difference = max(difference, abs(center
        - texture2D(depthTexture, uv - vec2(0.0, texel.y)).r));
    return smoothstep(0.00055, 0.0038, difference);
}

void main()
{
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / resolution;
    vec3 color = texture2D(texture0, uv).rgb;

    float grounding = ambientOcclusion(uv, texel);
    color *= mix(vec3(1.0), vec3(0.48, 0.62, 0.67), grounding * 0.48);
    color *= 1.0 - depthEdge(uv, texel) * 0.1;

    vec3 maskCenter = texture2D(actorMaskTexture, uv).rgb;
    vec3 maskNeighbor = vec3(0.0);
    vec2 outlineStep = texel * 2.1;
    maskNeighbor = max(maskNeighbor,
        texture2D(actorMaskTexture, uv + vec2(outlineStep.x, 0.0)).rgb);
    maskNeighbor = max(maskNeighbor,
        texture2D(actorMaskTexture, uv - vec2(outlineStep.x, 0.0)).rgb);
    maskNeighbor = max(maskNeighbor,
        texture2D(actorMaskTexture, uv + vec2(0.0, outlineStep.y)).rgb);
    maskNeighbor = max(maskNeighbor,
        texture2D(actorMaskTexture, uv - vec2(0.0, outlineStep.y)).rgb);
    float centerMask = max(maskCenter.r, max(maskCenter.g, maskCenter.b));
    float neighborMask = max(maskNeighbor.r,
        max(maskNeighbor.g, maskNeighbor.b));
    float actorOutline = smoothstep(0.04, 0.3, neighborMask - centerMask);
    vec3 outlineColor = maskNeighbor / max(neighborMask, 0.001);
    color += outlineColor * actorOutline * 0.42;

    color += texture2D(bloomTexture, uv).rgb * 0.72;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color *= mix(vec3(0.88, 0.99, 1.055), vec3(1.055, 1.0, 0.91),
        smoothstep(0.1, 0.82, luminance));
    color *= 1.08 + energyPulse * 0.018;
    color = acesToneMap(color) * 0.94;
    gl_FragColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
)";

inline constexpr const char* FINAL_FRAGMENT_SHADER = R"(
#version 100
precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float time;
uniform float damageAmount;
uniform float dashAmount;
uniform float energyPulse;
uniform vec2 playerEffectCenter;
uniform vec2 objectiveEffectCenter;

float luminance(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec2 effectDistortion(vec2 uv)
{
    vec2 playerDelta = uv - playerEffectCenter;
    playerDelta.x *= resolution.x / resolution.y;
    float playerDistance = length(playerDelta);
    vec2 direction = playerDelta / max(playerDistance, 0.0001);
    direction.x *= resolution.y / resolution.x;

    float dashProgress = 1.0 - dashAmount;
    float dashRadius = 0.035 + dashProgress * 0.19;
    float dashRing = 1.0 - smoothstep(0.0, 0.018,
        abs(playerDistance - dashRadius));
    vec2 offset = direction * dashRing * dashAmount * 0.008;

    float hitProgress = 1.0 - damageAmount;
    float hitRadius = 0.025 + hitProgress * 0.16;
    float hitRing = 1.0 - smoothstep(0.0, 0.014,
        abs(playerDistance - hitRadius));
    offset -= direction * hitRing * damageAmount * 0.006;

    vec2 objectiveDelta = uv - objectiveEffectCenter;
    objectiveDelta.x *= resolution.x / resolution.y;
    float objectiveDistance = length(objectiveDelta);
    vec2 objectiveDirection = objectiveDelta
        / max(objectiveDistance, 0.0001);
    objectiveDirection.x *= resolution.y / resolution.x;
    float objectiveRadius = fract(time * 0.18) * 0.24;
    float objectiveRing = 1.0 - smoothstep(0.0, 0.012,
        abs(objectiveDistance - objectiveRadius));
    offset += objectiveDirection * objectiveRing
        * energyPulse * 0.0018;
    return offset;
}

vec3 fxaa(vec2 uv)
{
    vec2 texel = 1.0 / resolution;
    vec3 rgbNorthWest = texture2D(texture0,
        uv + vec2(-texel.x, -texel.y)).rgb;
    vec3 rgbNorthEast = texture2D(texture0,
        uv + vec2(texel.x, -texel.y)).rgb;
    vec3 rgbSouthWest = texture2D(texture0,
        uv + vec2(-texel.x, texel.y)).rgb;
    vec3 rgbSouthEast = texture2D(texture0,
        uv + vec2(texel.x, texel.y)).rgb;
    vec3 rgbMiddle = texture2D(texture0, uv).rgb;

    float lumaNorthWest = luminance(rgbNorthWest);
    float lumaNorthEast = luminance(rgbNorthEast);
    float lumaSouthWest = luminance(rgbSouthWest);
    float lumaSouthEast = luminance(rgbSouthEast);
    float lumaMiddle = luminance(rgbMiddle);
    float lumaMinimum = min(lumaMiddle, min(min(lumaNorthWest,
        lumaNorthEast), min(lumaSouthWest, lumaSouthEast)));
    float lumaMaximum = max(lumaMiddle, max(max(lumaNorthWest,
        lumaNorthEast), max(lumaSouthWest, lumaSouthEast)));
    if (lumaMaximum - lumaMinimum
        < max(lumaMaximum * 0.0312, 0.0078)) {
        return rgbMiddle;
    }

    vec2 direction;
    direction.x = -((lumaNorthWest + lumaNorthEast)
        - (lumaSouthWest + lumaSouthEast));
    direction.y = (lumaNorthWest + lumaSouthWest)
        - (lumaNorthEast + lumaSouthEast);
    float reduction = max((lumaNorthWest + lumaNorthEast
        + lumaSouthWest + lumaSouthEast) * 0.03125, 0.0078125);
    float inverseMinimum = 1.0
        / (min(abs(direction.x), abs(direction.y)) + reduction);
    direction = clamp(direction * inverseMinimum,
        vec2(-8.0), vec2(8.0)) * texel;

    vec3 resultA = 0.5 * (
        texture2D(texture0, uv + direction * (1.0 / 3.0 - 0.5)).rgb
        + texture2D(texture0, uv + direction * (2.0 / 3.0 - 0.5)).rgb);
    vec3 resultB = resultA * 0.5 + 0.25 * (
        texture2D(texture0, uv + direction * -0.5).rgb
        + texture2D(texture0, uv + direction * 0.5).rgb);
    float lumaResultB = luminance(resultB);
    return lumaResultB < lumaMinimum || lumaResultB > lumaMaximum
        ? resultA : resultB;
}

float hash(vec2 point)
{
    vec2 wrapped = mod(point, 256.0);
    return fract(sin(dot(wrapped, vec2(12.9898, 78.233))) * 437.585);
}

void main()
{
    vec2 uv = clamp(fragTexCoord + effectDistortion(fragTexCoord),
        vec2(0.001), vec2(0.999));
    vec3 color = fxaa(uv);
    vec2 centered = fragTexCoord - 0.5;
    float edge = smoothstep(0.2, 0.72, length(centered));
    color *= mix(1.0, 0.84, edge);
    color = mix(color, vec3(0.88, 0.055, 0.02),
        edge * damageAmount * 0.38);
    color += vec3(0.0, 0.09, 0.11) * dashAmount * edge * 0.12;
    float grain = hash(floor(fragTexCoord * resolution)
        + floor(time * 20.0)) - 0.5;
    color += grain * (0.8 / 255.0);
    gl_FragColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
)";
#else
inline constexpr const char* BLOOM_EXTRACT_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D emissiveTexture;
uniform vec4 colDiffuse;
out vec4 finalColor;

void main()
{
    vec3 scene = texture(texture0, fragTexCoord).rgb;
    vec3 emissive = texture(emissiveTexture, fragTexCoord).rgb;
    float maximum = max(scene.r, max(scene.g, scene.b));
    float minimum = min(scene.r, min(scene.g, scene.b));
    float saturation = maximum - minimum;
    float selectiveHighlight = smoothstep(0.72, 1.0, maximum)
        * smoothstep(0.12, 0.42, saturation) * 0.18;
    vec3 bloom = emissive * 1.35 + scene * selectiveHighlight;
    finalColor = vec4(bloom, 1.0) * colDiffuse * fragColor;
}
)";

inline constexpr const char* BLOOM_BLUR_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 blurDirection;
out vec4 finalColor;

void main()
{
    vec4 color = texture(texture0, fragTexCoord) * 0.227027;
    color += texture(texture0, fragTexCoord + blurDirection * 1.384615) * 0.316216;
    color += texture(texture0, fragTexCoord - blurDirection * 1.384615) * 0.316216;
    color += texture(texture0, fragTexCoord + blurDirection * 3.230769) * 0.070270;
    color += texture(texture0, fragTexCoord - blurDirection * 3.230769) * 0.070270;
    finalColor = color;
}
)";

inline constexpr const char* COMPOSITE_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D depthTexture;
uniform sampler2D bloomTexture;
uniform sampler2D actorMaskTexture;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float depthEnabled;
uniform float energyPulse;
out vec4 finalColor;

vec3 acesToneMap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b))
        / (color * (c * color + d) + e), 0.0, 1.0);
}

float occluder(float centerDepth, vec2 sampleUv)
{
    float difference = centerDepth - texture(depthTexture, sampleUv).r;
    return smoothstep(0.00007, 0.0017, difference)
        * (1.0 - smoothstep(0.007, 0.025, difference));
}

float ambientOcclusion(vec2 uv, vec2 texel)
{
    if (depthEnabled < 0.5) {
        return 0.0;
    }
    float centerDepth = texture(depthTexture, uv).r;
    if (centerDepth > 0.9998) {
        return 0.0;
    }
    vec2 diagonal = vec2(0.707107);
    float nearAmount = 0.0;
    vec2 nearStep = texel * 2.2;
    nearAmount += occluder(centerDepth, uv + vec2(nearStep.x, 0.0));
    nearAmount += occluder(centerDepth, uv - vec2(nearStep.x, 0.0));
    nearAmount += occluder(centerDepth, uv + vec2(0.0, nearStep.y));
    nearAmount += occluder(centerDepth, uv - vec2(0.0, nearStep.y));
    nearAmount += occluder(centerDepth, uv + nearStep * diagonal);
    nearAmount += occluder(centerDepth, uv - nearStep * diagonal);
    nearAmount += occluder(centerDepth,
        uv + vec2(nearStep.x, -nearStep.y) * diagonal);
    nearAmount += occluder(centerDepth,
        uv + vec2(-nearStep.x, nearStep.y) * diagonal);

    float wideAmount = 0.0;
    vec2 wideStep = texel * 6.4;
    wideAmount += occluder(centerDepth, uv + vec2(wideStep.x, 0.0));
    wideAmount += occluder(centerDepth, uv - vec2(wideStep.x, 0.0));
    wideAmount += occluder(centerDepth, uv + vec2(0.0, wideStep.y));
    wideAmount += occluder(centerDepth, uv - vec2(0.0, wideStep.y));
    return clamp(nearAmount * 0.095 + wideAmount * 0.075, 0.0, 1.0);
}

float depthEdge(vec2 uv, vec2 texel)
{
    if (depthEnabled < 0.5) {
        return 0.0;
    }
    float center = texture(depthTexture, uv).r;
    float difference = 0.0;
    difference = max(difference, abs(center
        - texture(depthTexture, uv + vec2(texel.x, 0.0)).r));
    difference = max(difference, abs(center
        - texture(depthTexture, uv - vec2(texel.x, 0.0)).r));
    difference = max(difference, abs(center
        - texture(depthTexture, uv + vec2(0.0, texel.y)).r));
    difference = max(difference, abs(center
        - texture(depthTexture, uv - vec2(0.0, texel.y)).r));
    return smoothstep(0.00055, 0.0038, difference);
}

void main()
{
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / resolution;
    vec3 color = texture(texture0, uv).rgb;

    float grounding = ambientOcclusion(uv, texel);
    color *= mix(vec3(1.0), vec3(0.48, 0.62, 0.67), grounding * 0.48);
    color *= 1.0 - depthEdge(uv, texel) * 0.1;

    vec3 maskCenter = texture(actorMaskTexture, uv).rgb;
    vec3 maskNeighbor = vec3(0.0);
    vec2 outlineStep = texel * 2.1;
    maskNeighbor = max(maskNeighbor,
        texture(actorMaskTexture, uv + vec2(outlineStep.x, 0.0)).rgb);
    maskNeighbor = max(maskNeighbor,
        texture(actorMaskTexture, uv - vec2(outlineStep.x, 0.0)).rgb);
    maskNeighbor = max(maskNeighbor,
        texture(actorMaskTexture, uv + vec2(0.0, outlineStep.y)).rgb);
    maskNeighbor = max(maskNeighbor,
        texture(actorMaskTexture, uv - vec2(0.0, outlineStep.y)).rgb);
    float centerMask = max(maskCenter.r, max(maskCenter.g, maskCenter.b));
    float neighborMask = max(maskNeighbor.r,
        max(maskNeighbor.g, maskNeighbor.b));
    float actorOutline = smoothstep(0.04, 0.3, neighborMask - centerMask);
    vec3 outlineColor = maskNeighbor / max(neighborMask, 0.001);
    color += outlineColor * actorOutline * 0.42;

    color += texture(bloomTexture, uv).rgb * 0.72;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color *= mix(vec3(0.88, 0.99, 1.055), vec3(1.055, 1.0, 0.91),
        smoothstep(0.1, 0.82, luminance));
    color *= 1.08 + energyPulse * 0.018;
    color = acesToneMap(color) * 0.94;
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
)";

inline constexpr const char* FINAL_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float time;
uniform float damageAmount;
uniform float dashAmount;
uniform float energyPulse;
uniform vec2 playerEffectCenter;
uniform vec2 objectiveEffectCenter;
out vec4 finalColor;

float luminance(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec2 effectDistortion(vec2 uv)
{
    vec2 playerDelta = uv - playerEffectCenter;
    playerDelta.x *= resolution.x / resolution.y;
    float playerDistance = length(playerDelta);
    vec2 direction = playerDelta / max(playerDistance, 0.0001);
    direction.x *= resolution.y / resolution.x;

    float dashProgress = 1.0 - dashAmount;
    float dashRadius = 0.035 + dashProgress * 0.19;
    float dashRing = 1.0 - smoothstep(0.0, 0.018,
        abs(playerDistance - dashRadius));
    vec2 offset = direction * dashRing * dashAmount * 0.008;

    float hitProgress = 1.0 - damageAmount;
    float hitRadius = 0.025 + hitProgress * 0.16;
    float hitRing = 1.0 - smoothstep(0.0, 0.014,
        abs(playerDistance - hitRadius));
    offset -= direction * hitRing * damageAmount * 0.006;

    vec2 objectiveDelta = uv - objectiveEffectCenter;
    objectiveDelta.x *= resolution.x / resolution.y;
    float objectiveDistance = length(objectiveDelta);
    vec2 objectiveDirection = objectiveDelta
        / max(objectiveDistance, 0.0001);
    objectiveDirection.x *= resolution.y / resolution.x;
    float objectiveRadius = fract(time * 0.18) * 0.24;
    float objectiveRing = 1.0 - smoothstep(0.0, 0.012,
        abs(objectiveDistance - objectiveRadius));
    offset += objectiveDirection * objectiveRing
        * energyPulse * 0.0018;
    return offset;
}

vec3 fxaa(vec2 uv)
{
    vec2 texel = 1.0 / resolution;
    vec3 rgbNorthWest = texture(texture0,
        uv + vec2(-texel.x, -texel.y)).rgb;
    vec3 rgbNorthEast = texture(texture0,
        uv + vec2(texel.x, -texel.y)).rgb;
    vec3 rgbSouthWest = texture(texture0,
        uv + vec2(-texel.x, texel.y)).rgb;
    vec3 rgbSouthEast = texture(texture0,
        uv + vec2(texel.x, texel.y)).rgb;
    vec3 rgbMiddle = texture(texture0, uv).rgb;

    float lumaNorthWest = luminance(rgbNorthWest);
    float lumaNorthEast = luminance(rgbNorthEast);
    float lumaSouthWest = luminance(rgbSouthWest);
    float lumaSouthEast = luminance(rgbSouthEast);
    float lumaMiddle = luminance(rgbMiddle);
    float lumaMinimum = min(lumaMiddle, min(min(lumaNorthWest,
        lumaNorthEast), min(lumaSouthWest, lumaSouthEast)));
    float lumaMaximum = max(lumaMiddle, max(max(lumaNorthWest,
        lumaNorthEast), max(lumaSouthWest, lumaSouthEast)));
    if (lumaMaximum - lumaMinimum
        < max(lumaMaximum * 0.0312, 0.0078)) {
        return rgbMiddle;
    }

    vec2 direction;
    direction.x = -((lumaNorthWest + lumaNorthEast)
        - (lumaSouthWest + lumaSouthEast));
    direction.y = (lumaNorthWest + lumaSouthWest)
        - (lumaNorthEast + lumaSouthEast);
    float reduction = max((lumaNorthWest + lumaNorthEast
        + lumaSouthWest + lumaSouthEast) * 0.03125, 0.0078125);
    float inverseMinimum = 1.0
        / (min(abs(direction.x), abs(direction.y)) + reduction);
    direction = clamp(direction * inverseMinimum,
        vec2(-8.0), vec2(8.0)) * texel;

    vec3 resultA = 0.5 * (
        texture(texture0, uv + direction * (1.0 / 3.0 - 0.5)).rgb
        + texture(texture0, uv + direction * (2.0 / 3.0 - 0.5)).rgb);
    vec3 resultB = resultA * 0.5 + 0.25 * (
        texture(texture0, uv + direction * -0.5).rgb
        + texture(texture0, uv + direction * 0.5).rgb);
    float lumaResultB = luminance(resultB);
    return lumaResultB < lumaMinimum || lumaResultB > lumaMaximum
        ? resultA : resultB;
}

float hash(vec2 point)
{
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec2 uv = clamp(fragTexCoord + effectDistortion(fragTexCoord),
        vec2(0.001), vec2(0.999));
    vec3 color = fxaa(uv);
    vec2 centered = fragTexCoord - 0.5;
    float edge = smoothstep(0.2, 0.72, length(centered));
    color *= mix(1.0, 0.84, edge);
    color = mix(color, vec3(0.88, 0.055, 0.02),
        edge * damageAmount * 0.38);
    color += vec3(0.0, 0.09, 0.11) * dashAmount * edge * 0.12;
    float grain = hash(floor(fragTexCoord * resolution)
        + floor(time * 20.0)) - 0.5;
    color += grain * (0.8 / 255.0);
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
)";
#endif
