#include "encounter.hpp"

#include "arena.hpp"

void resetEncounter(Encounter& encounter)
{
    encounter = Encounter {};
}

EncounterStepResult updateEncounter(
    Encounter& encounter, const PlayerInput& input, float stepTime)
{
    return updateEncounter(encounter, input, stepTime, ARENA_WALLS);
}

EncounterStepResult updateEncounter(Encounter& encounter,
    const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls)
{
    EncounterStepResult result;
    if ((!isPlayerAlive(encounter.player)
            || !isEnemyAlive(encounter.combat.enemy))
        && input.restartPressed) {
        resetEncounter(encounter);
        result.restarted = true;
        return result;
    }

    const CombatStepResult combatResult = updateCombat(encounter.combat,
        encounter.player, encounter.target, input, stepTime, walls);
    result.playerDamage = combatResult.playerDamage;
    result.enemyDamage = combatResult.enemyDamage;
    result.enemyFired = combatResult.enemyFired;
    return result;
}
