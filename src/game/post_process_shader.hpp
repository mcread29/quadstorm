#pragma once

#if defined(PLATFORM_WEB)
inline constexpr const char* BLOOM_EXTRACT_FRAGMENT_SHADER = R"(
#version 100
precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;

void main()
{
    vec4 sampleColor = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;
    float maximum = max(sampleColor.r, max(sampleColor.g, sampleColor.b));
    float minimum = min(sampleColor.r, min(sampleColor.g, sampleColor.b));
    float saturation = maximum - minimum;
    float brightness = smoothstep(0.52, 0.92, maximum);
    float selective = smoothstep(0.08, 0.32, saturation);
    float contribution = brightness * mix(0.18, 1.0, selective);
    gl_FragColor = vec4(sampleColor.rgb * contribution, contribution);
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
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float time;
uniform float damageAmount;
uniform float dashAmount;
uniform float energyPulse;

float hash(vec2 point)
{
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453);
}

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

void main()
{
    vec2 uv = fragTexCoord;
    vec2 centered = uv - 0.5;
    float edge = smoothstep(0.18, 0.72, length(centered));
    float aberration = 0.00045 + damageAmount * 0.0045
        + dashAmount * 0.0018;
    vec2 offset = normalize(centered + vec2(0.0001)) * aberration * edge;

    vec3 color;
    color.r = texture2D(texture0, uv + offset).r;
    color.g = texture2D(texture0, uv).g;
    color.b = texture2D(texture0, uv - offset).b;
    vec2 texel = 1.0 / resolution;
    vec3 neighborAverage = (
        texture2D(texture0, uv + vec2(texel.x, 0.0)).rgb
        + texture2D(texture0, uv - vec2(texel.x, 0.0)).rgb
        + texture2D(texture0, uv + vec2(0.0, texel.y)).rgb
        + texture2D(texture0, uv - vec2(0.0, texel.y)).rgb) * 0.25;
    color += (color - neighborAverage) * 0.16;

    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 shadowGrade = vec3(0.84, 0.98, 1.06);
    vec3 highlightGrade = vec3(1.07, 1.0, 0.88);
    color *= mix(shadowGrade, highlightGrade,
        smoothstep(0.12, 0.82, luminance));
    color *= 0.88 + dashAmount * 0.08 + energyPulse * 0.025;
    color = acesToneMap(color) * 0.86;

    float vignette = 1.0 - smoothstep(0.43, 0.79, length(centered));
    color *= mix(0.68, 1.0, vignette);
    float damageEdge = edge * damageAmount;
    color = mix(color, vec3(0.86, 0.08, 0.025), damageEdge * 0.48);
    color += vec3(0.0, 0.12, 0.14) * dashAmount * edge * 0.22;

    float grain = hash(floor(uv * resolution) + floor(time * 24.0)) - 0.5;
    color += grain * (1.6 / 255.0);
    float scanline = sin(uv.y * resolution.y * 1.5708) * 0.5 + 0.5;
    color *= 0.994 + scanline * 0.006;
    float signalSweep = 1.0 - smoothstep(0.0, 0.018,
        abs(fract(uv.y - time * 0.045) - 0.5));
    color += vec3(0.01, 0.09, 0.085)
        * signalSweep * energyPulse * 0.09;
    gl_FragColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
)";
#else
inline constexpr const char* BLOOM_EXTRACT_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;

void main()
{
    vec4 sampleColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    float maximum = max(sampleColor.r, max(sampleColor.g, sampleColor.b));
    float minimum = min(sampleColor.r, min(sampleColor.g, sampleColor.b));
    float saturation = maximum - minimum;
    float brightness = smoothstep(0.52, 0.92, maximum);
    float selective = smoothstep(0.08, 0.32, saturation);
    float contribution = brightness * mix(0.18, 1.0, selective);
    finalColor = vec4(sampleColor.rgb * contribution, contribution);
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
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float time;
uniform float damageAmount;
uniform float dashAmount;
uniform float energyPulse;
out vec4 finalColor;

float hash(vec2 point)
{
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453);
}

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

void main()
{
    vec2 uv = fragTexCoord;
    vec2 centered = uv - 0.5;
    float edge = smoothstep(0.18, 0.72, length(centered));
    float aberration = 0.00045 + damageAmount * 0.0045
        + dashAmount * 0.0018;
    vec2 offset = normalize(centered + vec2(0.0001)) * aberration * edge;

    vec3 color;
    color.r = texture(texture0, uv + offset).r;
    color.g = texture(texture0, uv).g;
    color.b = texture(texture0, uv - offset).b;
    vec2 texel = 1.0 / resolution;
    vec3 neighborAverage = (
        texture(texture0, uv + vec2(texel.x, 0.0)).rgb
        + texture(texture0, uv - vec2(texel.x, 0.0)).rgb
        + texture(texture0, uv + vec2(0.0, texel.y)).rgb
        + texture(texture0, uv - vec2(0.0, texel.y)).rgb) * 0.25;
    color += (color - neighborAverage) * 0.16;

    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 shadowGrade = vec3(0.84, 0.98, 1.06);
    vec3 highlightGrade = vec3(1.07, 1.0, 0.88);
    color *= mix(shadowGrade, highlightGrade,
        smoothstep(0.12, 0.82, luminance));
    color *= 0.88 + dashAmount * 0.08 + energyPulse * 0.025;
    color = acesToneMap(color) * 0.86;

    float vignette = 1.0 - smoothstep(0.43, 0.79, length(centered));
    color *= mix(0.68, 1.0, vignette);
    float damageEdge = edge * damageAmount;
    color = mix(color, vec3(0.86, 0.08, 0.025), damageEdge * 0.48);
    color += vec3(0.0, 0.12, 0.14) * dashAmount * edge * 0.22;

    float grain = hash(floor(uv * resolution) + floor(time * 24.0)) - 0.5;
    color += grain * (1.6 / 255.0);
    float scanline = sin(uv.y * resolution.y * 1.5708) * 0.5 + 0.5;
    color *= 0.994 + scanline * 0.006;
    float signalSweep = 1.0 - smoothstep(0.0, 0.018,
        abs(fract(uv.y - time * 0.045) - 0.5));
    color += vec3(0.01, 0.09, 0.085)
        * signalSweep * energyPulse * 0.09;
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
}
)";
#endif
