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

varying vec2 fragTexCoord;
varying vec3 fragNormal;
varying vec3 fragWorldPosition;
varying vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragWorldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
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
    float distanceSquared = max(dot(delta, delta), 0.01);
    float diffuse = max(dot(normal, normalize(delta)), 0.0);
    return color * diffuse / (1.0 + distanceSquared * 0.22);
}

void main()
{
    vec4 surface = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    vec3 light = -normalize(lightDirection);
    float diffuse = max(dot(normal, light), 0.0);
    float skyAmount = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(groundAmbient, skyAmbient, skyAmount);
    vec3 halfDirection = normalize(light + viewDirection);
    float materialSpecular = materialKind < 0.5 ? 0.2
        : materialKind < 1.5 ? 0.11 : 0.065;
    float specular = pow(max(dot(normal, halfDirection), 0.0), 28.0)
        * materialSpecular;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)
        * (materialKind < 0.5 ? 0.2 : 0.12);

    float floorMaterial = 1.0 - step(0.45, abs(materialKind - 1.0));
    float wallMaterial = 1.0 - step(0.45, abs(materialKind - 2.0));
    float panel = max(panelLine(fragWorldPosition.x * 0.42),
        panelLine(fragWorldPosition.z * 0.42)) * floorMaterial;
    float subPanel = max(panelLine(fragWorldPosition.x * 0.84),
        panelLine(fragWorldPosition.z * 0.84)) * floorMaterial;
    float floorNoise = panelNoise(floor(fragWorldPosition.xz * 0.84));
    float brushed = sin(fragWorldPosition.x * 31.0
        + sin(fragWorldPosition.z * 4.0)) * 0.5 + 0.5;
    surface.rgb *= mix(1.0, 0.62, panel * 0.68);
    surface.rgb *= mix(1.0, 0.88, subPanel * 0.18);
    surface.rgb *= mix(1.0,
        0.9 + floorNoise * 0.12 + brushed * 0.025, floorMaterial);
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
    vec3 litColor = surface.rgb
        * (ambient + lightColor * diffuse + localLight);
    litColor += surface.rgb * rim + lightColor * specular;
    litColor += vec3(0.005, 0.14, 0.16) * conduit;
    litColor += vec3(0.01, 0.08, 0.085)
        * wallBand * wallMaterial;
    litColor = mix(litColor, litColor * vec3(0.88, 1.02, 1.04), 0.28);
    float fogDistance = length(fragWorldPosition.xz - cameraTarget.xz);
    float fogAmount = smoothstep(13.0, 28.0, fogDistance);
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

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorldPosition;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragWorldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

inline constexpr const char* LIGHTING_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragWorldPosition;
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
    float distanceSquared = max(dot(delta, delta), 0.01);
    float diffuse = max(dot(normal, normalize(delta)), 0.0);
    return color * diffuse / (1.0 + distanceSquared * 0.22);
}

void main()
{
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    vec3 light = -normalize(lightDirection);
    float diffuse = max(dot(normal, light), 0.0);
    float skyAmount = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(groundAmbient, skyAmbient, skyAmount);
    vec3 halfDirection = normalize(light + viewDirection);
    float materialSpecular = materialKind < 0.5 ? 0.2
        : materialKind < 1.5 ? 0.11 : 0.065;
    float specular = pow(max(dot(normal, halfDirection), 0.0), 28.0)
        * materialSpecular;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0)
        * (materialKind < 0.5 ? 0.2 : 0.12);

    float floorMaterial = 1.0 - step(0.45, abs(materialKind - 1.0));
    float wallMaterial = 1.0 - step(0.45, abs(materialKind - 2.0));
    float panel = max(panelLine(fragWorldPosition.x * 0.42),
        panelLine(fragWorldPosition.z * 0.42)) * floorMaterial;
    float subPanel = max(panelLine(fragWorldPosition.x * 0.84),
        panelLine(fragWorldPosition.z * 0.84)) * floorMaterial;
    float floorNoise = panelNoise(floor(fragWorldPosition.xz * 0.84));
    float brushed = sin(fragWorldPosition.x * 31.0
        + sin(fragWorldPosition.z * 4.0)) * 0.5 + 0.5;
    surface.rgb *= mix(1.0, 0.62, panel * 0.68);
    surface.rgb *= mix(1.0, 0.88, subPanel * 0.18);
    surface.rgb *= mix(1.0,
        0.9 + floorNoise * 0.12 + brushed * 0.025, floorMaterial);
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
    vec3 litColor = surface.rgb
        * (ambient + lightColor * diffuse + localLight);
    litColor += surface.rgb * rim + lightColor * specular;
    litColor += vec3(0.005, 0.14, 0.16) * conduit;
    litColor += vec3(0.01, 0.08, 0.085)
        * wallBand * wallMaterial;
    litColor = mix(litColor, litColor * vec3(0.88, 1.02, 1.04), 0.28);
    float fogDistance = length(fragWorldPosition.xz - cameraTarget.xz);
    float fogAmount = smoothstep(13.0, 28.0, fogDistance);
    finalColor = vec4(mix(litColor, fogColor, fogAmount), surface.a);
}
)";
#endif
