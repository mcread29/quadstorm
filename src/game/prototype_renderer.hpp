#pragma once

#include "player.hpp"

#include "raylib.h"

class PrototypeRenderer {
public:
    PrototypeRenderer();
    ~PrototypeRenderer();

    PrototypeRenderer(const PrototypeRenderer&) = delete;
    PrototypeRenderer& operator=(const PrototypeRenderer&) = delete;

    void draw(const Camera3D& camera, const Player& player, Vector3 aimPoint) const;

private:
    void drawPlayerShadow(const Player& player) const;
    void drawPlayer(const Player& player) const;

    Shader lightingShader {};
    Model groundModel {};
    Model playerModel {};
    Model shadowModel {};
};
