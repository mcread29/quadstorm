#pragma once

#include "enemy.hpp"
#include "player.hpp"

#include "raylib.h"

class CombatAudio {
public:
    CombatAudio();
    ~CombatAudio();

    CombatAudio(const CombatAudio&) = delete;
    CombatAudio& operator=(const CombatAudio&) = delete;

    void playEnemyShot() const;
    void playPlayerDamage(PlayerDamageResult result) const;
    void playEnemyDamage(EnemyDamageResult result) const;
    void playRestart() const;

private:
    bool ready = false;
    Sound enemyShot {};
    Sound playerHit {};
    Sound playerDeath {};
    Sound enemyHit {};
    Sound enemyDeath {};
    Sound restart {};
};
