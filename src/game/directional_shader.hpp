#pragma once

#include "raylib.h"

inline constexpr Vector3 DIRECTIONAL_LIGHT { 0.42F, -1.0F, 0.28F };

#if defined(PLATFORM_WEB)
inline constexpr const char* LIGHTING_VERTEX_SHADER = R"(
#version 100

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightViewProjection;

varying vec2 fragTexCoord;
varying vec3 fragNormal;
varying vec3 fragWorldPosition;
varying vec4 fragLightPosition;
varying vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragWorldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragLightPosition = lightViewProjection
        * vec4(fragWorldPosition, 1.0);
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

inline constexpr const char* LIGHTING_FRAGMENT_SHADER = R"(
#version 100

precision mediump float;

varying vec2 fragTexCoord;
varying vec3 fragNormal;
varying vec3 fragWorldPosition;
varying vec4 fragLightPosition;
varying vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 groundAmbient;
uniform vec3 skyAmbient;
uniform vec3 cameraPosition;
uniform vec3 cameraTarget;
uniform vec3 fogColor;
uniform float materialKind;
uniform vec3 pointLightPositionA;
uniform vec3 pointLightColorA;
uniform vec3 pointLightPositionB;
uniform vec3 pointLightColorB;
uniform sampler2D shadowMap;
uniform vec2 shadowTexelSize;
uniform float shadowsEnabled;

float panelLine(float coordinate)
{
    float edge = min(fract(coordinate), 1.0 - fract(coordinate));
    return 1.0 - smoothstep(0.018, 0.045, edge);
}

float panelNoise(vec2 cell)
{
    return fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 pointLight(vec3 position, vec3 color, vec3 normal)
{
    vec3 delta = position - fragWorldPosition;
    float distanceToLight = max(length(delta), 0.01);
    vec3 directionToLight = delta / distanceToLight;
    float wrappedDiffuse = max(
        (dot(normal, directionToLight) + 0.22) / 1.22, 0.0);
    float range = clamp(1.0 - distanceToLight / 8.5, 0.0, 1.0);
    float attenuation = range * range * (3.0 - 2.0 * range);
    return color * (0.12 + wrappedDiffuse * 0.88) * attenuation;
}

float directionalShadow(vec3 normal, vec3 light)
{
    if (shadowsEnabled < 0.5) {
        return 0.0;
    }
    vec3 projected = fragLightPosition.xyz / fragLightPosition.w;
    projected = projected * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0
        || projected.y <= 0.0 || projected.y >= 1.0
        || projected.z <= 0.0 || projected.z >= 1.0) {
        return 0.0;
    }

    float bias = max(0.00042 * (1.0 - dot(normal, light)), 0.00007);
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float sampleDepth = texture2D(shadowMap,
                projected.xy + vec2(float(x), float(y))
                    * shadowTexelSize).r;
            shadow += step(sampleDepth + bias, projected.z);
        }
    }
    return shadow / 9.0;
}

void main()
{
    vec4 surface = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    vec3 light = -normalize(lightDirection);
    float normalLight = dot(normal, light);
    float diffuse = max(normalLight, 0.0);
    float wrappedDiffuse = max((normalLight + 0.18) / 1.18, 0.0);
    float directionalAmount = mix(wrappedDiffuse, diffuse, 0.72);
    float skyAmount = normal.y * 0.5 + 0.5;
    float horizonOcclusion = mix(0.7, 1.0,
        smoothstep(-0.12, 0.58, normal.y));
    vec3 ambient = mix(groundAmbient, skyAmbient, skyAmount)
        * horizonOcclusion;
    vec3 halfDirection = normalize(light + viewDirection);
    float materialSpecular = materialKind < 0.5 ? 0.2
        : materialKind < 1.5 ? 0.11
        : materialKind < 2.5 ? 0.065 : 0.0;
    float specular = pow(max(dot(normal, halfDirection), 0.0), 28.0)
        * materialSpecular;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)
        * (materialKind < 0.5 ? 0.2
            : materialKind < 2.5 ? 0.12 : 0.0);

    float floorMaterial = 1.0 - step(0.45, abs(materialKind - 1.0));
    float wallMaterial = 1.0 - step(0.45, abs(materialKind - 2.0));
    float detailMaterial = 1.0 - step(0.45, abs(materialKind - 3.0));
    float shadowMaterial = 1.0 - step(0.45, abs(materialKind - 4.0));
    float panel = max(panelLine(fragWorldPosition.x * 0.42),
        panelLine(fragWorldPosition.z * 0.42)) * floorMaterial;
    float subPanel = max(panelLine(fragWorldPosition.x * 0.84),
        panelLine(fragWorldPosition.z * 0.84)) * floorMaterial;
    float floorNoise = panelNoise(floor(fragWorldPosition.xz * 0.84));
    float brushed = sin(fragWorldPosition.x * 31.0
        + sin(fragWorldPosition.z * 4.0)) * 0.5 + 0.5;
    surface.rgb *= mix(1.0, 0.88, panel * 0.16);
    surface.rgb *= mix(1.0, 0.97, subPanel * 0.04);
    surface.rgb *= mix(1.0,
        0.94 + floorNoise * 0.07 + brushed * 0.012, floorMaterial);
    vec2 conduitCoordinate = vec2(
        fragWorldPosition.x + fragWorldPosition.z,
        fragWorldPosition.x - fragWorldPosition.z) * 0.105;
    vec2 conduitFraction = fract(conduitCoordinate);
    vec2 conduitEdge = min(conduitFraction, 1.0 - conduitFraction);
    float conduit = 1.0 - smoothstep(0.006, 0.017,
        min(conduitEdge.x, conduitEdge.y));
    conduit *= floorMaterial * (1.0 - panel * 0.72);

    float wallBand = max(panelLine(fragWorldPosition.y * 0.72),
        panelLine((fragWorldPosition.x + fragWorldPosition.z) * 0.19));
    float wallRib = panelLine(
        (fragWorldPosition.x - fragWorldPosition.z) * 0.48);
    float wallNoise = panelNoise(floor(vec2(
        fragWorldPosition.x + fragWorldPosition.z,
        fragWorldPosition.y) * 1.7));
    float lowerGrime = 1.0 - smoothstep(0.05, 1.05, fragWorldPosition.y);
    surface.rgb *= mix(1.0,
        mix(0.88 + wallNoise * 0.1,
            0.58, max(wallBand * 0.5, wallRib * 0.34)),
        wallMaterial);
    surface.rgb *= mix(1.0, 0.72,
        lowerGrime * wallMaterial * (0.55 + wallNoise * 0.25));

    vec3 localLight = pointLight(pointLightPositionA,
        pointLightColorA, normal) + pointLight(pointLightPositionB,
        pointLightColorB, normal);
    float shadow = directionalShadow(normal, light);
    float directionalVisibility = 1.0 - shadow * 0.68;
    vec3 litColor = surface.rgb
        * (ambient + lightColor * directionalAmount
            * directionalVisibility + localLight);
    litColor += surface.rgb * rim
        + lightColor * specular * directionalVisibility;
    litColor += vec3(0.002, 0.012, 0.014) * conduit;
    litColor += vec3(0.008, 0.055, 0.06)
        * wallBand * wallMaterial;
    litColor = mix(litColor, surface.rgb * 1.08, detailMaterial * 0.84);
    litColor = mix(litColor, surface.rgb, shadowMaterial);
    litColor = mix(litColor, litColor * vec3(0.88, 1.02, 1.04), 0.28);
    float fogDistance = length(fragWorldPosition.xz - cameraTarget.xz);
    float fogAmount = smoothstep(13.0, 28.0, fogDistance)
        * (1.0 - shadowMaterial);
    gl_FragColor = vec4(mix(litColor, fogColor, fogAmount), surface.a);
}
)";
#else
inline constexpr const char* LIGHTING_VERTEX_SHADER = R"(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightViewProjection;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorldPosition;
out vec4 fragLightPosition;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragWorldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragLightPosition = lightViewProjection
        * vec4(fragWorldPosition, 1.0);
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

inline constexpr const char* LIGHTING_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPosition;
in vec4 fragLightPosition;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 groundAmbient;
uniform vec3 skyAmbient;
uniform vec3 cameraPosition;
uniform vec3 cameraTarget;
uniform vec3 fogColor;
uniform float materialKind;
uniform vec3 pointLightPositionA;
uniform vec3 pointLightColorA;
uniform vec3 pointLightPositionB;
uniform vec3 pointLightColorB;
uniform sampler2D shadowMap;
uniform vec2 shadowTexelSize;
uniform float shadowsEnabled;

out vec4 finalColor;

float panelLine(float coordinate)
{
    float edge = min(fract(coordinate), 1.0 - fract(coordinate));
    return 1.0 - smoothstep(0.018, 0.045, edge);
}

float panelNoise(vec2 cell)
{
    return fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 pointLight(vec3 position, vec3 color, vec3 normal)
{
    vec3 delta = position - fragWorldPosition;
    float distanceToLight = max(length(delta), 0.01);
    vec3 directionToLight = delta / distanceToLight;
    float wrappedDiffuse = max(
        (dot(normal, directionToLight) + 0.22) / 1.22, 0.0);
    float range = clamp(1.0 - distanceToLight / 8.5, 0.0, 1.0);
    float attenuation = range * range * (3.0 - 2.0 * range);
    return color * (0.12 + wrappedDiffuse * 0.88) * attenuation;
}

float directionalShadow(vec3 normal, vec3 light)
{
    if (shadowsEnabled < 0.5) {
        return 0.0;
    }
    vec3 projected = fragLightPosition.xyz / fragLightPosition.w;
    projected = projected * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0
        || projected.y <= 0.0 || projected.y >= 1.0
        || projected.z <= 0.0 || projected.z >= 1.0) {
        return 0.0;
    }

    float bias = max(0.00042 * (1.0 - dot(normal, light)), 0.00007);
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float sampleDepth = texture(shadowMap,
                projected.xy + vec2(float(x), float(y))
                    * shadowTexelSize).r;
            shadow += step(sampleDepth + bias, projected.z);
        }
    }
    return shadow / 9.0;
}

void main()
{
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    vec3 light = -normalize(lightDirection);
    float normalLight = dot(normal, light);
    float diffuse = max(normalLight, 0.0);
    float wrappedDiffuse = max((normalLight + 0.18) / 1.18, 0.0);
    float directionalAmount = mix(wrappedDiffuse, diffuse, 0.72);
    float skyAmount = normal.y * 0.5 + 0.5;
    float horizonOcclusion = mix(0.7, 1.0,
        smoothstep(-0.12, 0.58, normal.y));
    vec3 ambient = mix(groundAmbient, skyAmbient, skyAmount)
        * horizonOcclusion;
    vec3 halfDirection = normalize(light + viewDirection);
    float materialSpecular = materialKind < 0.5 ? 0.2
        : materialKind < 1.5 ? 0.11
        : materialKind < 2.5 ? 0.065 : 0.0;
    float specular = pow(max(dot(normal, halfDirection), 0.0), 28.0)
        * materialSpecular;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)
        * (materialKind < 0.5 ? 0.2
            : materialKind < 2.5 ? 0.12 : 0.0);

    float floorMaterial = 1.0 - step(0.45, abs(materialKind - 1.0));
    float wallMaterial = 1.0 - step(0.45, abs(materialKind - 2.0));
    float detailMaterial = 1.0 - step(0.45, abs(materialKind - 3.0));
    float shadowMaterial = 1.0 - step(0.45, abs(materialKind - 4.0));
    float panel = max(panelLine(fragWorldPosition.x * 0.42),
        panelLine(fragWorldPosition.z * 0.42)) * floorMaterial;
    float subPanel = max(panelLine(fragWorldPosition.x * 0.84),
        panelLine(fragWorldPosition.z * 0.84)) * floorMaterial;
    float floorNoise = panelNoise(floor(fragWorldPosition.xz * 0.84));
    float brushed = sin(fragWorldPosition.x * 31.0
        + sin(fragWorldPosition.z * 4.0)) * 0.5 + 0.5;
    surface.rgb *= mix(1.0, 0.88, panel * 0.16);
    surface.rgb *= mix(1.0, 0.97, subPanel * 0.04);
    surface.rgb *= mix(1.0,
        0.94 + floorNoise * 0.07 + brushed * 0.012, floorMaterial);
    vec2 conduitCoordinate = vec2(
        fragWorldPosition.x + fragWorldPosition.z,
        fragWorldPosition.x - fragWorldPosition.z) * 0.105;
    vec2 conduitFraction = fract(conduitCoordinate);
    vec2 conduitEdge = min(conduitFraction, 1.0 - conduitFraction);
    float conduit = 1.0 - smoothstep(0.006, 0.017,
        min(conduitEdge.x, conduitEdge.y));
    conduit *= floorMaterial * (1.0 - panel * 0.72);

    float wallBand = max(panelLine(fragWorldPosition.y * 0.72),
        panelLine((fragWorldPosition.x + fragWorldPosition.z) * 0.19));
    float wallRib = panelLine(
        (fragWorldPosition.x - fragWorldPosition.z) * 0.48);
    float wallNoise = panelNoise(floor(vec2(
        fragWorldPosition.x + fragWorldPosition.z,
        fragWorldPosition.y) * 1.7));
    float lowerGrime = 1.0 - smoothstep(0.05, 1.05, fragWorldPosition.y);
    surface.rgb *= mix(1.0,
        mix(0.88 + wallNoise * 0.1,
            0.58, max(wallBand * 0.5, wallRib * 0.34)),
        wallMaterial);
    surface.rgb *= mix(1.0, 0.72,
        lowerGrime * wallMaterial * (0.55 + wallNoise * 0.25));

    vec3 localLight = pointLight(pointLightPositionA,
        pointLightColorA, normal) + pointLight(pointLightPositionB,
        pointLightColorB, normal);
    float shadow = directionalShadow(normal, light);
    float directionalVisibility = 1.0 - shadow * 0.68;
    vec3 litColor = surface.rgb
        * (ambient + lightColor * directionalAmount
            * directionalVisibility + localLight);
    litColor += surface.rgb * rim
        + lightColor * specular * directionalVisibility;
    litColor += vec3(0.002, 0.012, 0.014) * conduit;
    litColor += vec3(0.008, 0.055, 0.06)
        * wallBand * wallMaterial;
    litColor = mix(litColor, surface.rgb * 1.08, detailMaterial * 0.84);
    litColor = mix(litColor, surface.rgb, shadowMaterial);
    litColor = mix(litColor, litColor * vec3(0.88, 1.02, 1.04), 0.28);
    float fogDistance = length(fragWorldPosition.xz - cameraTarget.xz);
    float fogAmount = smoothstep(13.0, 28.0, fogDistance)
        * (1.0 - shadowMaterial);
    finalColor = vec4(mix(litColor, fogColor, fogAmount), surface.a);
}
)";
#endif
