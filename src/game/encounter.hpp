#pragma once

#include "combat.hpp"
#include "target.hpp"

#include <span>

struct Encounter {
    CombatState combat {};
    Player player {};
    Target target {};
};

struct EncounterStepResult {
    PlayerDamageResult playerDamage = PlayerDamageResult::none;
    EnemyDamageResult enemyDamage = EnemyDamageResult::none;
    bool enemyFired = false;
    bool restarted = false;
};

void resetEncounter(Encounter& encounter);
EncounterStepResult updateEncounter(
    Encounter& encounter, const PlayerInput& input, float stepTime);
EncounterStepResult updateEncounter(Encounter& encounter,
    const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls);
