#pragma once

#include "combat.hpp"
#include "enemy_collection.hpp"
#include "level_session.hpp"

#include <cstddef>
#include <optional>
#include <vector>

inline constexpr std::size_t GENERATED_ENEMIES_PER_ENCOUNTER = 3;

struct GeneratedEnemySpawn {
    EnemyId id = 0;
    Vector2 position {};
};

struct GeneratedCombatState {
    EnemyCollection enemies;
    ProjectilePool enemyProjectiles { ENEMY_PROJECTILE_PROFILE };
};

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

    GeneratedCombatState* activeCombat();
    const GeneratedCombatState* activeCombat() const;
    const GeneratedCombatState* combatForRoom(std::optional<int> room) const;
    ProjectilePool& playerProjectiles()
    {
        return playerAttack.projectiles;
    }
    const ProjectilePool& playerProjectiles() const
    {
        return playerAttack.projectiles;
    }

private:
    friend void resetGeneratedEncounter(GeneratedEncounterCoordinator&);
    friend GeneratedEncounterStepResult updateGeneratedEncounter(
        GeneratedEncounterCoordinator&, LevelSession&,
        const GeneratedLevel&, const PlayerInput&, float);

    PlayerAttackState playerAttack;
    GeneratedCombatState combatState;
    std::optional<int> roomId;
    bool fighting = false;
    bool floorComplete = false;
};

void resetGeneratedEncounter(GeneratedEncounterCoordinator& coordinator);
GeneratedEncounterStepResult updateGeneratedEncounter(
    GeneratedEncounterCoordinator& coordinator, LevelSession& session,
    const GeneratedLevel& level, const PlayerInput& input, float stepTime);
std::vector<GeneratedEnemySpawn> selectGeneratedEnemySpawns(
    const GeneratedLevel& level, int room, Vector2 playerPosition);
std::optional<Vector2> selectGeneratedEnemySpawn(
    const GeneratedLevel& level, int room, Vector2 playerPosition);
