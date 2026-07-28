#pragma once

#include "combat.hpp"
#include "enemy.hpp"
#include "generated_level.hpp"
#include "level_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

inline constexpr int HORDE_FINAL_ROUND = 5;
inline constexpr float ANCHOR_HOLDOUT_DURATION = 8.0F;
inline constexpr float ANCHOR_HOLDOUT_RADIUS = 3.2F;

enum class HordeEnemyRole : std::uint8_t {
    Drifter,
    Runner,
    Caster,
    Elite
};

enum class RoundPhase : std::uint8_t {
    Intermission,
    Buildup,
    Peak,
    Cleanup
};

enum class GatePurpose : std::uint8_t {
    Expansion,
    Anchor,
    Reward,
    Exit
};

enum class MatchUpgrade : std::uint8_t {
    Damage,
    FireRate,
    Dash
};

struct MapGate {
    std::size_t doorway = 0;
    GatePurpose purpose = GatePurpose::Expansion;
    int cost = 0;
    bool open = false;
};

struct RelayTarget {
    stalberg::rooms::CellIndex cell = 0;
    Vector2 position {};
};

struct SmallMapPlan {
    stalberg::rooms::SmallMapRecipe recipe
        = stalberg::rooms::SmallMapRecipe::HubCircuit;
    int startRoom = stalberg::rooms::EMPTY_CELL;
    int hubRoom = stalberg::rooms::EMPTY_CELL;
    int anchorRoom = stalberg::rooms::EMPTY_CELL;
    int rewardRoom = stalberg::rooms::EMPTY_CELL;
    int exitRoom = stalberg::rooms::EMPTY_CELL;
    stalberg::rooms::CellIndex hubCell = 0;
    stalberg::rooms::CellIndex anchorCell = 0;
    stalberg::rooms::CellIndex exitCell = 0;
    Vector2 hubPosition {};
    Vector2 anchorPosition {};
    Vector2 exitPosition {};
    std::vector<MapGate> gates;
    std::vector<RelayTarget> relayTargets;
};

struct HordeEnemy {
    std::uint64_t id = 0;
    HordeEnemyRole role = HordeEnemyRole::Drifter;
    Enemy enemy;
};

struct HordeMatchStepResult {
    EnemyDamageResult enemyDamage = EnemyDamageResult::none;
    PlayerDamageResult playerDamage = PlayerDamageResult::none;
    bool enemyFired = false;
    bool reset = false;
    bool gatePurchased = false;
    bool objectiveAdvanced = false;
    bool roundStarted = false;
    bool roundCompleted = false;
    bool victory = false;
};

SmallMapPlan buildSmallMapPlan(const GeneratedLevel& level);
std::vector<HordeEnemyRole> hordeCompositionForRound(int round);
std::optional<stalberg::rooms::CellIndex> nextHordeNavigationCell(
    const GeneratedLevel& level, const LevelSession& session,
    stalberg::rooms::CellIndex start, stalberg::rooms::CellIndex destination);

class HordeMatch {
public:
    HordeMatch(const GeneratedLevel& level, LevelSession& session);

    const SmallMapPlan& plan() const { return mapPlan; }
    int points() const { return pointTotal; }
    int round() const { return currentRound; }
    RoundPhase phase() const { return roundPhase; }
    bool anchorIsActive() const { return anchorActive; }
    bool anchorIsComplete() const { return anchorComplete; }
    float anchorProgress() const { return anchorProgressSeconds; }
    bool hubIsPowered() const { return hubPowered; }
    bool relayIsComplete() const { return relayComplete; }
    std::size_t relayProgress() const { return relaySequenceProgress; }
    bool matchIsComplete() const { return victory; }
    int weaponDamage() const { return damagePerShot; }
    bool hasUpgrade(MatchUpgrade upgrade) const;
    std::span<const HordeEnemy> enemies() const { return enemyEntries; }
    std::span<HordeEnemy> enemies() { return enemyEntries; }
    const ProjectilePool& playerProjectiles() const
    {
        return playerAttack.projectiles;
    }
    const ProjectilePool& enemyProjectiles() const
    {
        return hostileProjectiles;
    }

    void grantPoints(int amount);
    bool purchaseGate(LevelSession& session, std::size_t gateIndex);
    bool purchaseUpgrade(LevelSession& session, MatchUpgrade upgrade);
    bool startNextRound();
    bool activateAnchor();
    bool advanceAnchor(float seconds);
    bool activateHub(LevelSession& session);
    bool hitRelay(std::size_t targetIndex);
    void reset(LevelSession& session);

    // Stable simulation state is public for deterministic headless scenarios
    // and presentation, matching the existing Encounter state boundary.
    const GeneratedLevel* level = nullptr;
    SmallMapPlan mapPlan;
    PlayerAttackState playerAttack;
    ProjectilePool hostileProjectiles { ENEMY_PROJECTILE_PROFILE };
    std::vector<HordeEnemy> enemyEntries;
    std::vector<HordeEnemyRole> pendingSpawns;
    std::size_t nextPendingSpawn = 0;
    std::uint64_t nextEnemyId = 1;
    int pointTotal = 0;
    int currentRound = 0;
    int damagePerShot = 1;
    RoundPhase roundPhase = RoundPhase::Intermission;
    float phaseElapsed = 0.0F;
    float spawnCooldown = 0.0F;
    bool damageUpgrade = false;
    bool fireRateUpgrade = false;
    bool dashUpgrade = false;
    bool anchorActive = false;
    bool anchorComplete = false;
    float anchorProgressSeconds = 0.0F;
    bool hubPowered = false;
    std::size_t relaySequenceProgress = 0;
    bool relayComplete = false;
    bool victory = false;
};

HordeMatchStepResult updateHordeMatch(HordeMatch& match,
    LevelSession& session, const PlayerInput& input, float stepTime);
