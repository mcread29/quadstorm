#pragma once

#include "raylib.h"

inline constexpr Vector3 DIRECTIONAL_LIGHT { 0.45F, -1.0F, 0.25F };

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

void main()
{
    vec4 surface = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float diffuse = max(dot(normal, -normalize(lightDirection)), 0.0);
    float skyAmount = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(groundAmbient, skyAmbient, skyAmount);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0) * 0.12;
    vec3 litColor = surface.rgb * (ambient + lightColor * diffuse) + surface.rgb * rim;
    float fogDistance = length(fragWorldPosition.xz - cameraTarget.xz);
    float fogAmount = smoothstep(12.0, 25.0, fogDistance);
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

out vec4 finalColor;

void main()
{
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float diffuse = max(dot(normal, -normalize(lightDirection)), 0.0);
    float skyAmount = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(groundAmbient, skyAmbient, skyAmount);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 3.0) * 0.12;
    vec3 litColor = surface.rgb * (ambient + lightColor * diffuse) + surface.rgb * rim;
    float fogDistance = length(fragWorldPosition.xz - cameraTarget.xz);
    float fogAmount = smoothstep(12.0, 25.0, fogDistance);
    finalColor = vec4(mix(litColor, fogColor, fogAmount), surface.a);
}
)";
#endif
