#pragma once

#include "raylib.h"

inline constexpr float PLAYER_RADIUS = 0.65F;
inline constexpr float PLAYER_FACING_MARKER_DISTANCE = PLAYER_RADIUS + 0.65F;
inline constexpr int PLAYER_MAX_HEALTH = 5;
inline constexpr float PLAYER_INVULNERABILITY_DURATION = 0.8F;
inline constexpr float PLAYER_HIT_FLASH_DURATION = 0.18F;
inline constexpr float PLAYER_DASH_SPEED = 18.0F;
inline constexpr float PLAYER_DASH_DURATION = 0.18F;
inline constexpr float PLAYER_DASH_COOLDOWN = 0.8F;

struct PlayerInput {
    Vector2 movement {};
    Vector3 aimPoint {};
    bool hasAimPoint = false;
    bool fireHeld = false;
    bool dashPressed = false;
    bool restartPressed = false;
    bool toggleViewPressed = false;
    bool interactPressed = false;
    bool startRoundPressed = false;
    bool buyDamagePressed = false;
    bool buyFireRatePressed = false;
    bool buyDashPressed = false;
};

struct Player {
    Vector3 position { 0.0F, PLAYER_RADIUS, 0.0F };
    Vector2 velocity {};
    Vector2 facing { 0.0F, -1.0F };
    int health = PLAYER_MAX_HEALTH;
    float invulnerabilityRemaining = 0.0F;
    float hitFlashRemaining = 0.0F;
    Vector2 dashDirection {};
    float dashRemaining = 0.0F;
    float dashCooldownRemaining = 0.0F;
    float dashCooldownScale = 1.0F;
};

enum class PlayerDamageResult {
    none,
    hit,
    died
};

class ProjectilePool;

void updatePlayer(Player& player, const PlayerInput& input, float stepTime);
void updatePlayerEffects(Player& player, float stepTime);
PlayerDamageResult updatePlayerDamage(Player& player,
    ProjectilePool& enemyProjectiles, Vector2 previousPlayerPosition,
    float stepTime);
bool isPlayerAlive(const Player& player);
Player interpolatePlayer(const Player& previous, const Player& current, float amount);
