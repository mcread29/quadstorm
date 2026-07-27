#pragma once

#include "raylib.h"

inline constexpr Vector3 DIRECTIONAL_LIGHT { 0.45F, -1.0F, 0.25F };

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
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

inline constexpr const char* LIGHTING_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;

out vec4 finalColor;

void main()
{
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    float diffuse = max(dot(normal, -normalize(lightDirection)), 0.0);
    vec3 lighting = ambientColor + lightColor * diffuse;
    finalColor = vec4(surface.rgb * lighting, surface.a);
}
)";
