#pragma once

#include "generated_level.hpp"

#include "raylib.h"

class RenderResources {
public:
    RenderResources(const GeneratedLevel& level, Shader lightingShader);
    ~RenderResources();

    RenderResources(const RenderResources&) = delete;
    RenderResources& operator=(const RenderResources&) = delete;

    Model groundModel {};
    Model generatedFloorModel {};
    Model generatedWallModel {};
    Model wallModel {};
    Model playerModel {};
    Model targetModel {};
    Model enemyModel {};
    Model runnerModel {};
    Model casterModel {};
    Model eliteModel {};
    Model shadowModel {};
    Texture2D projectileGlow {};
};
