#pragma once

#include "raylib.h"

inline constexpr float PLAYER_RADIUS = 0.65F;
inline constexpr float PLAYER_FACING_MARKER_DISTANCE = PLAYER_RADIUS + 0.65F;

struct PlayerInput {
    Vector2 movement {};
    Vector3 aimPoint {};
    bool hasAimPoint = false;
    bool fireHeld = false;
};

struct Player {
    Vector3 position { 0.0F, PLAYER_RADIUS, 0.0F };
    Vector2 velocity {};
    Vector2 facing { 0.0F, -1.0F };
};

void updatePlayer(Player& player, const PlayerInput& input, float stepTime);
Player interpolatePlayer(const Player& previous, const Player& current, float amount);
