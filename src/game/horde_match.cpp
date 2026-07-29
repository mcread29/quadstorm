#include "horde_match.hpp"

#include "arena.hpp"
#include "collision_2d.hpp"
#include "generated_level_queries.hpp"
#include "vector2_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <queue>
#include <ranges>

namespace {

constexpr float INTERACTION_DISTANCE = 1.8F;
constexpr float DEVICE_INTERACTION_DISTANCE = 2.2F;
constexpr float RELAY_TARGET_RADIUS = 0.42F;
constexpr float CONTACT_DAMAGE_DISTANCE = ENEMY_RADIUS + PLAYER_RADIUS;

using CellIndex = stalberg::rooms::CellIndex;
using vector2::distanceSquared;
using vector2::length;
using vector2::normalized;

bool gateForPurposeIsOpen(const HordeMatch& match, GatePurpose purpose)
{
    const auto gate = std::ranges::find(
        match.plan().gates, purpose, &MapGate::purpose);
    return gate == match.plan().gates.end() || gate->open;
}

struct EnemyArchetype {
    int maximumHealth;
    float speed;
    int killReward;
    bool firesProjectiles;
};

constexpr std::array<int, HORDE_MAX_UPGRADE_LEVEL> DAMAGE_UPGRADE_COSTS {
    1200, 2400, 4200
};
constexpr std::array<int, HORDE_MAX_UPGRADE_LEVEL> FIRE_RATE_UPGRADE_COSTS {
    1000, 2200, 3800
};
constexpr std::array<int, HORDE_MAX_UPGRADE_LEVEL> DASH_UPGRADE_COSTS {
    900, 1800, 3000
};
constexpr std::array<float, HORDE_MAX_UPGRADE_LEVEL> FIRE_RATE_LEVELS {
    0.075F, 0.06F, 0.05F
};
constexpr std::array<float, HORDE_MAX_UPGRADE_LEVEL> DASH_COOLDOWN_LEVELS {
    0.7F, 0.55F, 0.45F
};

constexpr auto enemyArchetype(HordeEnemyRole role)
{
    switch (role) {
    case HordeEnemyRole::Drifter:
        return EnemyArchetype { 3, 1.9F, 60, false };
    case HordeEnemyRole::Runner:
        return EnemyArchetype { 2, 3.5F, 70, false };
    case HordeEnemyRole::Caster:
        return EnemyArchetype { 6, 1.55F, 100, true };
    case HordeEnemyRole::Elite:
        return EnemyArchetype { 12, 1.35F, 250, true };
    }
    return EnemyArchetype { 3, 1.9F, 60, false };
}

std::size_t livingEnemyCount(std::span<const HordeEnemy> enemies)
{
    return static_cast<std::size_t>(std::ranges::count_if(enemies,
        [](const HordeEnemy& enemy) {
            return isEnemyAlive(enemy.enemy);
        }));
}

std::vector<bool> reachableCells(const GeneratedLevel& level,
    const LevelSession& session, CellIndex start)
{
    std::vector<bool> reached(level.roomGrid().getCellCount(), false);
    if (start >= reached.size()) {
        return reached;
    }
    std::queue<CellIndex> frontier;
    reached[start] = true;
    frontier.push(start);
    while (!frontier.empty()) {
        const CellIndex cell = frontier.front();
        frontier.pop();
        for (const CellIndex neighbor : level.traversableNeighbors(cell)) {
            if (!reached[neighbor] && session.canTraverse(cell, neighbor)) {
                reached[neighbor] = true;
                frontier.push(neighbor);
            }
        }
    }
    return reached;
}

bool spawnEnemy(HordeMatch& match, const LevelSession& session,
    HordeEnemyRole role)
{
    const GeneratedLevel& level = *match.level;
    const Vector2 playerPosition {
        session.player().position.x, session.player().position.z
    };
    const auto playerCell = level.cellAtWorldPoint(playerPosition);
    if (!playerCell.has_value()) {
        return false;
    }
    const std::vector<bool> reached
        = reachableCells(level, session, *playerCell);
    std::vector<CellIndex> candidates;
    for (const auto& room : level.roomLayout().getRooms()) {
        for (const CellIndex cell : room.enemySpawnCandidates) {
            if (cell < reached.size() && reached[cell]) {
                candidates.push_back(cell);
            }
        }
    }
    if (candidates.empty()) {
        const auto assignments = level.roomLayout().getCellAssignments();
        for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
            if (assignments[cell] != stalberg::rooms::EMPTY_CELL
                && reached[cell]) {
                candidates.push_back(cell);
            }
        }
    }
    std::ranges::sort(candidates);
    if (candidates.empty()) {
        return false;
    }

    const std::size_t firstCandidate = static_cast<std::size_t>(
        match.nextEnemyId % candidates.size());
    std::optional<CellIndex> selected;
    for (std::size_t offset = 0; offset < candidates.size(); ++offset) {
        const CellIndex cell
            = candidates[(firstCandidate + offset) % candidates.size()];
        const Vector2 position = generated_level::worldCellCenter(level, cell);
        const float clearance
            = level.dualGrid().cells[cell].clearance * level.worldScale();
        if (distanceSquared(position, playerPosition) < 16.0F
            || clearance < ENEMY_RADIUS + 0.08F) {
            continue;
        }
        const bool wallBlocked = std::ranges::any_of(session.activeWalls(),
            [&](const Segment2D& wall) {
                const Vector2 closest
                    = closestPointOnSegment(position, wall).position;
                return distanceSquared(position, closest)
                    < (ENEMY_RADIUS + 0.04F) * (ENEMY_RADIUS + 0.04F);
            });
        const bool occupied = std::ranges::any_of(match.enemyEntries,
            [&](const HordeEnemy& enemy) {
                constexpr float separation = ENEMY_RADIUS * 2.0F + 0.12F;
                return isEnemyAlive(enemy.enemy)
                    && distanceSquared(enemy.enemy.position, position)
                        < separation * separation;
            });
        if (!wallBlocked && !occupied) {
            selected = cell;
            break;
        }
    }
    if (!selected.has_value()) {
        return false;
    }

    HordeEnemy entry;
    entry.id = match.nextEnemyId++;
    entry.role = role;
    entry.enemy.position = generated_level::worldCellCenter(level, *selected);
    entry.enemy.previousPosition = entry.enemy.position;
    const EnemyArchetype archetype = enemyArchetype(role);
    entry.maximumHealth = std::max(1, static_cast<int>(std::ceil(
        static_cast<float>(archetype.maximumHealth)
            * match.difficultyProfile.healthScale)));
    entry.enemy.health = entry.maximumHealth;
    entry.killReward = std::max(1,
        archetype.killReward * match.difficultyProfile.rewardPercent / 100);
    entry.movementSpeed
        = archetype.speed * match.difficultyProfile.movementSpeedScale;
    entry.shotInterval
        = ENEMY_SHOT_INTERVAL * match.difficultyProfile.firingIntervalScale;
    entry.enemy.shotCooldownRemaining = archetype.firesProjectiles
        ? ENEMY_FIRST_SHOT_DELAY
        : std::numeric_limits<float>::infinity();
    match.enemyEntries.push_back(entry);
    return true;
}

void resolveEnemySeparation(
    HordeMatch& match, const LevelSession& session)
{
    for (std::size_t first = 0; first < match.enemyEntries.size(); ++first) {
        if (!isEnemyAlive(match.enemyEntries[first].enemy)) {
            continue;
        }
        for (std::size_t second = first + 1;
             second < match.enemyEntries.size(); ++second) {
            if (!isEnemyAlive(match.enemyEntries[second].enemy)) {
                continue;
            }
            Enemy& firstEnemy = match.enemyEntries[first].enemy;
            Enemy& secondEnemy = match.enemyEntries[second].enemy;
            const Vector2 difference {
                secondEnemy.position.x - firstEnemy.position.x,
                secondEnemy.position.y - firstEnemy.position.y
            };
            const float distance = length(difference);
            const float minimumDistance = ENEMY_RADIUS * 2.0F;
            if (distance >= minimumDistance) {
                continue;
            }
            const Vector2 direction = distance <= 0.001F
                ? Vector2 {
                    (match.enemyEntries[first].id
                            < match.enemyEntries[second].id)
                        ? 1.0F : -1.0F,
                    0.0F
                }
                : Vector2 {
                    difference.x / distance, difference.y / distance
                };
            const float correction = (minimumDistance - distance) * 0.25F;
            firstEnemy.position.x -= direction.x * correction;
            firstEnemy.position.y -= direction.y * correction;
            secondEnemy.position.x += direction.x * correction;
            secondEnemy.position.y += direction.y * correction;
        }
    }
    for (HordeEnemy& entry : match.enemyEntries) {
        if (isEnemyAlive(entry.enemy)) {
            resolveCircleWallCollisions(entry.enemy.position,
                entry.enemy.velocity, ENEMY_RADIUS,
                entry.enemy.previousPosition, session.activeWalls());
        }
    }
}

void updateHordeMovement(HordeMatch& match, const LevelSession& session,
    float stepTime)
{
    const GeneratedLevel& level = *match.level;
    const Vector2 playerPosition {
        session.player().position.x, session.player().position.z
    };
    const auto playerCell = level.cellAtWorldPoint(playerPosition);
    for (HordeEnemy& entry : match.enemyEntries) {
        Enemy& enemy = entry.enemy;
        enemy.previousPosition = enemy.position;
        if (!isEnemyAlive(enemy) || !playerCell.has_value()) {
            enemy.velocity = Vector2 {};
            continue;
        }
        const auto enemyCell = level.cellAtWorldPoint(enemy.position);
        Vector2 destination = playerPosition;
        if (enemyCell.has_value() && *enemyCell != *playerCell) {
            const auto nextCell = nextHordeNavigationCell(
                level, session, *enemyCell, *playerCell);
            if (nextCell.has_value()) {
                destination = generated_level::worldCellCenter(level, *nextCell);
            }
        }
        const Vector2 toDestination {
            destination.x - enemy.position.x,
            destination.y - enemy.position.y
        };
        const float destinationDistance = length(toDestination);
        const float playerDistance = std::sqrt(
            distanceSquared(enemy.position, playerPosition));
        if ((entry.role == HordeEnemyRole::Caster && playerDistance < 4.2F)
            || destinationDistance <= 0.001F) {
            enemy.velocity = Vector2 {};
            continue;
        }
        const Vector2 direction = normalized(toDestination);
        enemy.facing = direction;
        const float speed = entry.movementSpeed;
        enemy.velocity = Vector2 { direction.x * speed, direction.y * speed };
        enemy.position.x += enemy.velocity.x * stepTime;
        enemy.position.y += enemy.velocity.y * stepTime;
        resolveCircleWallCollisions(enemy.position, enemy.velocity,
            ENEMY_RADIUS, enemy.previousPosition, session.activeWalls());
    }

    resolveEnemySeparation(match, session);
}

bool updateRelayHits(HordeMatch& match)
{
    if (match.relayComplete || !match.hubPowered
        || !gateForPurposeIsOpen(match, GatePurpose::Reward)) {
        return false;
    }
    bool advanced = false;
    for (Projectile& projectile : match.playerAttack.projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        for (std::size_t target = 0;
             target < match.mapPlan.relayTargets.size(); ++target) {
            const auto hit = sweepCircleAgainstCircle(
                projectile.previousPosition, projectile.position,
                match.playerAttack.projectiles.profile().radius,
                match.mapPlan.relayTargets[target].position,
                RELAY_TARGET_RADIUS);
            if (!hit.has_value()) {
                continue;
            }
            projectile.active = false;
            advanced |= match.hitRelay(target);
            break;
        }
    }
    return advanced;
}

EnemyDamageResult updateHordeDamage(HordeMatch& match, float stepTime)
{
    for (HordeEnemy& entry : match.enemyEntries) {
        entry.enemy.hitFlashRemaining = std::max(
            0.0F, entry.enemy.hitFlashRemaining - stepTime);
    }
    EnemyDamageResult result = EnemyDamageResult::none;
    for (Projectile& projectile : match.playerAttack.projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        HordeEnemy* selected = nullptr;
        float selectedHit = std::numeric_limits<float>::infinity();
        for (HordeEnemy& entry : match.enemyEntries) {
            if (!isEnemyAlive(entry.enemy)) {
                continue;
            }
            const Vector2 relativeStart {
                projectile.previousPosition.x - entry.enemy.previousPosition.x,
                projectile.previousPosition.y - entry.enemy.previousPosition.y
            };
            const Vector2 relativeEnd {
                projectile.position.x - entry.enemy.position.x,
                projectile.position.y - entry.enemy.position.y
            };
            const auto hit = sweepCircleAgainstCircle(relativeStart,
                relativeEnd, match.playerAttack.projectiles.profile().radius,
                Vector2 {}, ENEMY_RADIUS);
            if (!hit.has_value() || *hit > selectedHit) {
                continue;
            }
            if (selected == nullptr || *hit < selectedHit
                || entry.id < selected->id) {
                selected = &entry;
                selectedHit = *hit;
            }
        }
        if (selected == nullptr) {
            continue;
        }
        projectile.active = false;
        selected->enemy.health -= match.damagePerShot;
        selected->enemy.hitFlashRemaining = ENEMY_HIT_FLASH_DURATION;
        match.grantPoints(
            10 * match.difficultyProfile.rewardPercent / 100);
        if (selected->enemy.health <= 0) {
            selected->enemy.health = 0;
            selected->enemy.velocity = Vector2 {};
            match.grantPoints(selected->killReward);
            result = EnemyDamageResult::died;
        } else if (result == EnemyDamageResult::none) {
            result = EnemyDamageResult::hit;
        }
    }
    return result;
}

PlayerDamageResult updateContactDamage(
    HordeMatch& match, LevelSession& session)
{
    Player& player = session.player();
    if (!isPlayerAlive(player) || player.invulnerabilityRemaining > 0.0F) {
        return PlayerDamageResult::none;
    }
    const Vector2 playerPosition { player.position.x, player.position.z };
    const float contactDistanceSquared
        = CONTACT_DAMAGE_DISTANCE * CONTACT_DAMAGE_DISTANCE;
    const bool touching = std::ranges::any_of(match.enemyEntries,
        [&](const HordeEnemy& enemy) {
            return isEnemyAlive(enemy.enemy)
                && distanceSquared(enemy.enemy.position, playerPosition)
                    <= contactDistanceSquared;
        });
    if (!touching) {
        return PlayerDamageResult::none;
    }
    player.health -= match.difficultyProfile.enemyDamage;
    player.hitFlashRemaining = PLAYER_HIT_FLASH_DURATION;
    player.invulnerabilityRemaining = PLAYER_INVULNERABILITY_DURATION;
    if (player.health <= 0) {
        player.health = 0;
        player.velocity = Vector2 {};
        return PlayerDamageResult::died;
    }
    return PlayerDamageResult::hit;
}

float distanceToThreshold(Vector2 point, const DoorwayThreshold& threshold)
{
    const Vector2 closest = closestPointOnSegment(
        point, threshold.segment).position;
    return std::sqrt(distanceSquared(point, closest));
}

bool closeTo(Vector2 first, Vector2 second, float distance)
{
    return distanceSquared(first, second) <= distance * distance;
}

std::optional<std::size_t> nearestClosedGate(
    const HordeMatch& match, Vector2 playerPosition)
{
    std::optional<std::size_t> selected;
    float selectedDistance = INTERACTION_DISTANCE;
    const auto thresholds = match.level->doorwayThresholds();
    for (std::size_t gate = 0; gate < match.mapPlan.gates.size(); ++gate) {
        const MapGate& candidate = match.mapPlan.gates[gate];
        if (candidate.open || candidate.doorway >= thresholds.size()) {
            continue;
        }
        const float distance = distanceToThreshold(
            playerPosition, thresholds[candidate.doorway]);
        if (distance <= selectedDistance) {
            selected = gate;
            selectedDistance = distance;
        }
    }
    return selected;
}

void handleInteraction(HordeMatch& match, LevelSession& session,
    Vector2 playerPosition, HordeMatchStepResult& result)
{
    const auto nearbyGate = nearestClosedGate(match, playerPosition);
    if (nearbyGate.has_value()) {
        result.gatePurchased = match.purchaseGate(session, *nearbyGate);
        return;
    }
    if (closeTo(playerPosition, match.mapPlan.anchorPosition,
            DEVICE_INTERACTION_DISTANCE)) {
        const auto anchorGate = std::ranges::find(match.mapPlan.gates,
            GatePurpose::Anchor, &MapGate::purpose);
        if (anchorGate != match.mapPlan.gates.end() && !anchorGate->open) {
            const auto anchorGateIndex = static_cast<std::size_t>(
                std::distance(match.mapPlan.gates.begin(), anchorGate));
            result.gatePurchased
                = match.purchaseGate(session, anchorGateIndex);
        }
        result.objectiveAdvanced = match.activateAnchor();
        return;
    }
    if (closeTo(playerPosition, match.mapPlan.hubPosition,
            DEVICE_INTERACTION_DISTANCE)) {
        result.objectiveAdvanced = match.activateHub(session)
            || match.purchaseHubRepair(session);
        return;
    }
    if (closeTo(playerPosition, match.mapPlan.exitPosition,
            DEVICE_INTERACTION_DISTANCE)
        && match.hubPowered
        && match.currentRound >= HORDE_EXTRACTION_MINIMUM_ROUND) {
        match.victory = true;
        result.victory = true;
    }
}

void handleUpgradeInput(HordeMatch& match, LevelSession& session,
    const PlayerInput& input, Vector2 playerPosition,
    HordeMatchStepResult& result)
{
    if (!closeTo(playerPosition, match.mapPlan.hubPosition,
            DEVICE_INTERACTION_DISTANCE)) {
        return;
    }
    if (input.buyDamagePressed) {
        result.objectiveAdvanced |= match.purchaseUpgrade(
            session, MatchUpgrade::Damage);
    }
    if (input.buyFireRatePressed) {
        result.objectiveAdvanced |= match.purchaseUpgrade(
            session, MatchUpgrade::FireRate);
    }
    if (input.buyDashPressed) {
        result.objectiveAdvanced |= match.purchaseUpgrade(
            session, MatchUpgrade::Dash);
    }
}

ProjectileProfile hostileProjectileProfile(
    const HordeDifficultyProfile& difficulty)
{
    return ProjectileProfile {
        .speed = ENEMY_PROJECTILE_SPEED * difficulty.projectileSpeedScale,
        .lifetime = ENEMY_PROJECTILE_LIFETIME,
        .radius = ENEMY_PROJECTILE_RADIUS
    };
}

bool beginNextRound(HordeMatch& match)
{
    if (match.victory || match.roundPhase != RoundPhase::Intermission) {
        return false;
    }
    if (match.currentRound < std::numeric_limits<std::uint64_t>::max()) {
        ++match.currentRound;
    }
    match.difficultyProfile = hordeDifficultyForRound(
        match.currentRound, match.mapPlan.recipe);
    match.pendingSpawns = hordeCompositionForRound(
        match.currentRound, match.mapPlan.recipe);
    match.nextPendingSpawn = 0;
    match.enemyEntries.clear();
    match.hostileProjectiles = ProjectilePool {
        hostileProjectileProfile(match.difficultyProfile)
    };
    match.roundPhase = RoundPhase::Buildup;
    match.phaseElapsed = 0.0F;
    match.spawnCooldown = 0.0F;
    return true;
}

bool updateRoundDirector(HordeMatch& match, float stepTime)
{
    if (match.roundPhase != RoundPhase::Intermission) {
        return false;
    }
    const float duration = match.currentRound == 0
        ? HORDE_INITIAL_COUNTDOWN_DURATION
        : HORDE_INTERMISSION_DURATION;
    match.phaseElapsed = std::min(duration,
        match.phaseElapsed + std::max(0.0F, stepTime));
    return match.phaseElapsed >= duration && beginNextRound(match);
}

void updateRoundSpawning(
    HordeMatch& match, const LevelSession& session, float stepTime)
{
    if (match.roundPhase == RoundPhase::Intermission) {
        return;
    }
    match.phaseElapsed += stepTime;
    if (match.roundPhase == RoundPhase::Buildup
        && match.phaseElapsed >= match.difficultyProfile.buildupDuration) {
        match.roundPhase = RoundPhase::Peak;
        match.phaseElapsed = 0.0F;
    }
    match.spawnCooldown = std::max(0.0F, match.spawnCooldown - stepTime);
    if (match.nextPendingSpawn < match.pendingSpawns.size()
        && livingEnemyCount(match.enemyEntries)
            < match.difficultyProfile.maximumLiving
        && match.spawnCooldown <= 0.0F
        && spawnEnemy(match, session,
            match.pendingSpawns[match.nextPendingSpawn])) {
        ++match.nextPendingSpawn;
        match.spawnCooldown = match.roundPhase == RoundPhase::Buildup
            ? match.difficultyProfile.buildupSpawnInterval
            : match.difficultyProfile.peakSpawnInterval;
    }
    if (match.nextPendingSpawn == match.pendingSpawns.size()) {
        match.roundPhase = RoundPhase::Cleanup;
    }
}

void updateHostileAttacks(HordeMatch& match, LevelSession& session,
    Vector2 playerPosition, Vector2 previousPlayerPosition, float stepTime,
    HordeMatchStepResult& result)
{
    for (HordeEnemy& entry : match.enemyEntries) {
        if (enemyArchetype(entry.role).firesProjectiles
            && isEnemyAlive(entry.enemy)) {
            result.enemyFired |= updateEnemyPattern(entry.enemy,
                match.hostileProjectiles, playerPosition, stepTime,
                session.activeWalls(), entry.shotInterval);
        }
    }
    match.hostileProjectiles.update(stepTime);
    resolveProjectileWallCollisions(
        match.hostileProjectiles, session.activeWalls());
    result.playerDamage = updatePlayerDamage(session.player(),
        match.hostileProjectiles, previousPlayerPosition, stepTime,
        match.difficultyProfile.enemyDamage);
    const PlayerDamageResult contactDamage
        = updateContactDamage(match, session);
    if (contactDamage == PlayerDamageResult::died
        || (contactDamage == PlayerDamageResult::hit
            && result.playerDamage == PlayerDamageResult::none)) {
        result.playerDamage = contactDamage;
    }
    match.hostileProjectiles.retireExpired();
}

bool completeRoundIfCleared(HordeMatch& match)
{
    if (match.roundPhase != RoundPhase::Cleanup
        || match.nextPendingSpawn != match.pendingSpawns.size()
        || livingEnemyCount(match.enemyEntries) != 0) {
        return false;
    }
    match.enemyEntries.clear();
    match.hostileProjectiles = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
    match.roundPhase = RoundPhase::Intermission;
    match.phaseElapsed = 0.0F;
    return true;
}

} // namespace

HordeMatch::HordeMatch(const GeneratedLevel& sourceLevel, LevelSession& session)
    : level(&sourceLevel)
    , mapPlan(buildSmallMapPlan(sourceLevel))
{
    reset(session);
}

bool HordeMatch::hasUpgrade(MatchUpgrade upgrade) const
{
    return upgradeLevel(upgrade) > 0;
}

std::uint8_t HordeMatch::upgradeLevel(MatchUpgrade upgrade) const
{
    switch (upgrade) {
    case MatchUpgrade::Damage:
        return damageUpgradeLevel;
    case MatchUpgrade::FireRate:
        return fireRateUpgradeLevel;
    case MatchUpgrade::Dash:
        return dashUpgradeLevel;
    }
    return 0;
}

int HordeMatch::nextUpgradeCost(MatchUpgrade upgrade) const
{
    const std::size_t level = upgradeLevel(upgrade);
    if (level >= HORDE_MAX_UPGRADE_LEVEL) {
        return 0;
    }
    switch (upgrade) {
    case MatchUpgrade::Damage:
        return DAMAGE_UPGRADE_COSTS[level];
    case MatchUpgrade::FireRate:
        return FIRE_RATE_UPGRADE_COSTS[level];
    case MatchUpgrade::Dash:
        return DASH_UPGRADE_COSTS[level];
    }
    return 0;
}

int HordeMatch::hubRepairCost() const
{
    return 500 + static_cast<int>(difficultyProfile.pressureTier) * 75;
}

float HordeMatch::timeUntilNextRound() const
{
    if (roundPhase != RoundPhase::Intermission) {
        return 0.0F;
    }
    const float duration = currentRound == 0
        ? HORDE_INITIAL_COUNTDOWN_DURATION
        : HORDE_INTERMISSION_DURATION;
    return std::max(0.0F, duration - phaseElapsed);
}

void HordeMatch::grantPoints(int amount)
{
    if (amount <= 0) {
        return;
    }
    if (pointTotal > std::numeric_limits<int>::max() - amount) {
        pointTotal = std::numeric_limits<int>::max();
        return;
    }
    pointTotal += amount;
}

bool HordeMatch::purchaseGate(LevelSession& session, std::size_t gateIndex)
{
    if (gateIndex >= mapPlan.gates.size()) {
        return false;
    }
    MapGate& gate = mapPlan.gates[gateIndex];
    if (gate.open || gate.purpose == GatePurpose::Exit
        || (gate.purpose == GatePurpose::Reward
            && !gateForPurposeIsOpen(*this, GatePurpose::Anchor))
        || gate.cost < 0 || pointTotal < gate.cost) {
        return false;
    }
    pointTotal -= gate.cost;
    gate.open = true;
    session.setDoorwayLocked(gate.doorway, false);
    return true;
}

bool HordeMatch::purchaseUpgrade(
    LevelSession& session, MatchUpgrade upgrade)
{
    std::uint8_t* level = nullptr;
    switch (upgrade) {
    case MatchUpgrade::Damage:
        level = &damageUpgradeLevel;
        break;
    case MatchUpgrade::FireRate:
        level = &fireRateUpgradeLevel;
        break;
    case MatchUpgrade::Dash:
        level = &dashUpgradeLevel;
        break;
    }
    const int cost = nextUpgradeCost(upgrade);
    if (level == nullptr || *level >= HORDE_MAX_UPGRADE_LEVEL
        || cost <= 0 || pointTotal < cost
        || !gateForPurposeIsOpen(*this, GatePurpose::Anchor)) {
        return false;
    }
    pointTotal -= cost;
    ++*level;
    if (upgrade == MatchUpgrade::Damage) {
        damagePerShot = 1 + damageUpgradeLevel;
    } else if (upgrade == MatchUpgrade::FireRate) {
        playerAttack.weapon.fireInterval
            = FIRE_RATE_LEVELS[fireRateUpgradeLevel - 1];
    } else {
        session.player().dashCooldownScale
            = DASH_COOLDOWN_LEVELS[dashUpgradeLevel - 1];
    }
    return true;
}

bool HordeMatch::purchaseHubRepair(LevelSession& session)
{
    Player& player = session.player();
    const int cost = hubRepairCost();
    if (!hubPowered || !isPlayerAlive(player)
        || player.health >= PLAYER_MAX_HEALTH || pointTotal < cost) {
        return false;
    }
    pointTotal -= cost;
    ++player.health;
    return true;
}

bool HordeMatch::activateAnchor()
{
    if (anchorActive || anchorComplete || currentRound < 2
        || !gateForPurposeIsOpen(*this, GatePurpose::Anchor)) {
        return false;
    }
    anchorActive = true;
    anchorProgressSeconds = 0.0F;
    return true;
}

bool HordeMatch::advanceAnchor(float seconds)
{
    if (!anchorActive || anchorComplete || seconds <= 0.0F) {
        return false;
    }
    anchorProgressSeconds = std::min(
        ANCHOR_HOLDOUT_DURATION, anchorProgressSeconds + seconds);
    if (anchorProgressSeconds < ANCHOR_HOLDOUT_DURATION) {
        return true;
    }
    anchorActive = false;
    anchorComplete = true;
    grantPoints(250);
    return true;
}

bool HordeMatch::activateHub(LevelSession& session)
{
    if (!anchorComplete || hubPowered) {
        return false;
    }
    hubPowered = true;
    const auto exitGate = std::ranges::find(
        mapPlan.gates, GatePurpose::Exit, &MapGate::purpose);
    if (exitGate != mapPlan.gates.end()) {
        exitGate->open = true;
        session.setDoorwayLocked(exitGate->doorway, false);
    }
    return true;
}

bool HordeMatch::hitRelay(std::size_t targetIndex)
{
    if (relayComplete || !hubPowered || mapPlan.relayTargets.empty()
        || !gateForPurposeIsOpen(*this, GatePurpose::Reward)) {
        return false;
    }
    if (targetIndex != relaySequenceProgress) {
        relaySequenceProgress = 0;
        return false;
    }
    ++relaySequenceProgress;
    if (relaySequenceProgress == mapPlan.relayTargets.size()) {
        relayComplete = true;
        if (fireRateUpgradeLevel < HORDE_MAX_UPGRADE_LEVEL) {
            ++fireRateUpgradeLevel;
            playerAttack.weapon.fireInterval
                = FIRE_RATE_LEVELS[fireRateUpgradeLevel - 1];
        }
        grantPoints(400);
    }
    return true;
}

void HordeMatch::reset(LevelSession& session)
{
    playerAttack = PlayerAttackState {};
    hostileProjectiles = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
    enemyEntries.clear();
    pendingSpawns.clear();
    nextPendingSpawn = 0;
    nextEnemyId = 1;
    pointTotal = 0;
    currentRound = 0;
    damagePerShot = 1;
    roundPhase = RoundPhase::Intermission;
    difficultyProfile = hordeDifficultyForRound(1, mapPlan.recipe);
    phaseElapsed = 0.0F;
    spawnCooldown = 0.0F;
    damageUpgradeLevel = 0;
    fireRateUpgradeLevel = 0;
    dashUpgradeLevel = 0;
    anchorActive = false;
    anchorComplete = false;
    anchorProgressSeconds = 0.0F;
    hubPowered = false;
    relaySequenceProgress = 0;
    relayComplete = false;
    victory = false;
    std::vector<std::size_t> gateDoorways;
    gateDoorways.reserve(mapPlan.gates.size());
    for (MapGate& gate : mapPlan.gates) {
        gate.open = false;
        gateDoorways.push_back(gate.doorway);
    }
    session.setDoorwaysLocked(gateDoorways, true);
}

HordeMatchStepResult updateHordeMatch(HordeMatch& match,
    LevelSession& session, const PlayerInput& input, float stepTime)
{
    HordeMatchStepResult result;
    if (input.restartPressed) {
        session.reset();
        match.reset(session);
        result.reset = true;
        return result;
    }
    if (match.victory) {
        return result;
    }

    const Vector2 previousPlayerPosition {
        session.player().position.x, session.player().position.z
    };
    session.update(input, stepTime);
    if (!isPlayerAlive(session.player()) || match.victory) {
        return result;
    }

    updatePlayerAttack(match.playerAttack, session.player(),
        input.fireHeld, stepTime, session.activeWalls());
    result.objectiveAdvanced |= updateRelayHits(match);

    const Vector2 playerPosition {
        session.player().position.x, session.player().position.z
    };
    if (input.interactPressed) {
        handleInteraction(match, session, playerPosition, result);
    }
    if (match.victory) {
        return result;
    }
    handleUpgradeInput(match, session, input, playerPosition, result);
    result.roundStarted = updateRoundDirector(match, stepTime);

    if (!result.roundStarted) {
        updateRoundSpawning(match, session, stepTime);
    }
    updateHordeMovement(match, session, stepTime);
    result.enemyDamage = updateHordeDamage(match, stepTime);
    match.playerAttack.projectiles.retireExpired();
    updateHostileAttacks(match, session, playerPosition,
        previousPlayerPosition, stepTime, result);

    if (isPlayerAlive(session.player()) && match.anchorActive
        && match.roundPhase != RoundPhase::Intermission
        && closeTo(playerPosition, match.mapPlan.anchorPosition,
            ANCHOR_HOLDOUT_RADIUS)) {
        result.objectiveAdvanced |= match.advanceAnchor(stepTime);
    }
    result.roundCompleted = completeRoundIfCleared(match);
    return result;
}
