#pragma once

#include "combat.hpp"
#include "level_session.hpp"

#include <optional>

struct GeneratedEncounterStepResult {
    LevelSessionStepResult levelSession;
    CombatStepResult combat;
    bool encounterStarted = false;
    bool encounterCleared = false;
    bool floorCompleted = false;
};

class GeneratedEncounterCoordinator {
public:
    std::optional<int> encounterRoom() const { return roomId; }
    bool isFighting() const { return fighting; }
    bool floorIsComplete() const { return floorComplete; }

    CombatState* activeCombat();
    const CombatState* activeCombat() const;
    const CombatState* combatForRoom(std::optional<int> room) const;

private:
    friend void resetGeneratedEncounter(GeneratedEncounterCoordinator&);
    friend GeneratedEncounterStepResult updateGeneratedEncounter(
        GeneratedEncounterCoordinator&, LevelSession&,
        const GeneratedLevel&, const PlayerInput&, float);

    CombatState combatState;
    std::optional<int> roomId;
    bool fighting = false;
    bool floorComplete = false;
};

void resetGeneratedEncounter(GeneratedEncounterCoordinator& coordinator);
GeneratedEncounterStepResult updateGeneratedEncounter(
    GeneratedEncounterCoordinator& coordinator, LevelSession& session,
    const GeneratedLevel& level, const PlayerInput& input, float stepTime);
std::optional<Vector2> selectGeneratedEnemySpawn(
    const GeneratedLevel& level, int room, Vector2 playerPosition);
