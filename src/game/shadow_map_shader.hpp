#pragma once

#if defined(PLATFORM_WEB)
inline constexpr const char* SHADOW_DEPTH_VERTEX_SHADER = R"(
#version 100

attribute vec3 vertexPosition;
uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

inline constexpr const char* SHADOW_DEPTH_FRAGMENT_SHADER = R"(
#version 100
precision mediump float;

void main()
{
    gl_FragColor = vec4(1.0);
}
)";
#else
inline constexpr const char* SHADOW_DEPTH_VERTEX_SHADER = R"(
#version 330

in vec3 vertexPosition;
uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

inline constexpr const char* SHADOW_DEPTH_FRAGMENT_SHADER = R"(
#version 330

out vec4 finalColor;

void main()
{
    finalColor = vec4(1.0);
}
)";
#endif
