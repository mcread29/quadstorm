#include "generated_encounter.hpp"

#include "arena.hpp"
#include "generated_level_queries.hpp"
#include "vector2_math.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float MINIMUM_PLAYER_SPAWN_DISTANCE = 4.0F;
constexpr float MINIMUM_ENEMY_CLEARANCE = ENEMY_RADIUS + 0.2F;
constexpr float MINIMUM_DOORWAY_SPAWN_DISTANCE = ENEMY_RADIUS + 0.75F;
constexpr float MINIMUM_WALL_SPAWN_DISTANCE = ENEMY_RADIUS + 0.05F;
constexpr float MINIMUM_ENEMY_SEPARATION = ENEMY_RADIUS * 2.0F + 0.2F;

using vector2::distanceSquared;

float distanceToSegment(Vector2 point, const Segment2D& segment)
{
    const Vector2 closest = closestPointOnSegment(point, segment).position;
    return std::sqrt(distanceSquared(point, closest));
}

bool roomStartsEncounter(const stalberg::rooms::GeneratedRoom& room)
{
    return room.role == stalberg::rooms::RoomRole::Combat
        || room.role == stalberg::rooms::RoomRole::Hub;
}

bool farEnoughFromRoomDoorways(
    const GeneratedLevel& level, int room, Vector2 position)
{
    return std::ranges::none_of(level.doorwayThresholds(),
        [&](const DoorwayThreshold& doorway) {
            return (doorway.firstRegion == room
                       || doorway.secondRegion == room)
                && distanceToSegment(position, doorway.segment)
                    < MINIMUM_DOORWAY_SPAWN_DISTANCE;
        });
}

bool hasWallClearance(const GeneratedLevel& level, Vector2 position)
{
    return std::ranges::none_of(level.walls(),
        [&](const Segment2D& wall) {
            return distanceToSegment(position, wall)
                < MINIMUM_WALL_SPAWN_DISTANCE;
        });
}

bool hasEnemySeparation(std::span<const GeneratedEnemySpawn> selected,
    Vector2 position)
{
    return std::ranges::none_of(selected,
        [&](const GeneratedEnemySpawn& spawn) {
            return distanceSquared(position, spawn.position)
                < MINIMUM_ENEMY_SEPARATION * MINIMUM_ENEMY_SEPARATION;
        });
}

void freezeProjectileInterpolation(ProjectilePool& projectiles)
{
    for (Projectile& projectile : projectiles.projectiles()) {
        projectile.previousPosition = projectile.position;
    }
}

void freezeGeneratedCombatInterpolation(
    PlayerAttackState& attack, GeneratedCombatState& combat)
{
    freezeEnemyCollectionInterpolation(combat.enemies);
    freezeProjectileInterpolation(attack.projectiles);
    freezeProjectileInterpolation(combat.enemyProjectiles);
}

void updateGeneratedPlayerWeapon(PlayerAttackState& attack,
    const LevelSession& session, const PlayerInput& input, float stepTime)
{
    if (!isPlayerAlive(session.player())) {
        return;
    }

    updatePlayerAttack(attack, session.player(),
        input.fireHeld, stepTime, session.activeWalls());
    attack.projectiles.retireExpired();
}

void beginGeneratedCombat(GeneratedCombatState& combat,
    std::span<const GeneratedEnemySpawn> spawns)
{
    combat.enemies.clear();
    for (const GeneratedEnemySpawn& spawn : spawns) {
        combat.enemies.add(spawn.id, spawn.position);
    }
    combat.enemyProjectiles = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
}

CombatStepResult updateGeneratedCombat(PlayerAttackState& attack,
    GeneratedCombatState& combat, Player& player,
    const PlayerInput& input, float stepTime,
    std::span<const Segment2D> walls)
{
    CombatStepResult result;
    updatePlayerEffects(player, stepTime);
    if (!isPlayerAlive(player) || combat.enemies.allDefeated()) {
        freezeGeneratedCombatInterpolation(attack, combat);
        return result;
    }

    const Vector2 previousPlayerPosition {
        player.position.x,
        player.position.z
    };
    updatePlayer(player, input, stepTime);
    resolvePlayerWallCollisions(player, previousPlayerPosition, walls);

    updatePlayerAttack(attack, player, input.fireHeld, stepTime, walls);

    const Vector2 playerPosition { player.position.x, player.position.z };
    updateEnemyCollectionMovement(
        combat.enemies, playerPosition, stepTime, walls);
    result.enemyDamage = updateEnemyCollectionDamage(
        combat.enemies, attack.projectiles, stepTime);
    attack.projectiles.retireExpired();
    if (combat.enemies.allDefeated()) {
        freezeGeneratedCombatInterpolation(attack, combat);
        return result;
    }

    result.enemyFired = updateEnemyCollectionPatterns(combat.enemies,
        combat.enemyProjectiles, playerPosition, stepTime, walls);
    combat.enemyProjectiles.update(stepTime);
    resolveProjectileWallCollisions(combat.enemyProjectiles, walls);
    result.playerDamage = updatePlayerDamage(player,
        combat.enemyProjectiles, previousPlayerPosition, stepTime);
    combat.enemyProjectiles.retireExpired();
    if (result.playerDamage == PlayerDamageResult::died) {
        freezeGeneratedCombatInterpolation(attack, combat);
    }
    return result;
}

} // namespace

GeneratedCombatState* GeneratedEncounterCoordinator::activeCombat()
{
    return fighting ? &combatState : nullptr;
}

const GeneratedCombatState* GeneratedEncounterCoordinator::activeCombat() const
{
    return fighting ? &combatState : nullptr;
}

const GeneratedCombatState* GeneratedEncounterCoordinator::combatForRoom(
    std::optional<int> room) const
{
    return roomId.has_value() && room == roomId ? &combatState : nullptr;
}

void resetGeneratedEncounter(GeneratedEncounterCoordinator& coordinator)
{
    coordinator = GeneratedEncounterCoordinator {};
}

std::vector<GeneratedEnemySpawn> selectGeneratedEnemySpawns(
    const GeneratedLevel& level, int room, Vector2 playerPosition)
{
    const auto* generatedRoom = generated_level::findRoomById(level, room);
    if (generatedRoom == nullptr) {
        return {};
    }

    const auto playerCell = level.cellAtWorldPoint(playerPosition);
    const auto assignments = level.roomLayout().getCellAssignments();
    std::vector<stalberg::rooms::CellIndex> candidates(
        generatedRoom->enemySpawnCandidates.begin(),
        generatedRoom->enemySpawnCandidates.end());
    std::ranges::sort(candidates);

    std::vector<GeneratedEnemySpawn> selected;
    selected.reserve(GENERATED_ENEMIES_PER_ENCOUNTER);
    for (const stalberg::rooms::CellIndex cell : candidates) {
        if (cell >= assignments.size() || assignments[cell] != room
            || (playerCell.has_value() && cell == *playerCell)) {
            continue;
        }

        const stalberg::DualCell& dualCell = level.dualGrid().cells[cell];
        const float clearance = dualCell.clearance * level.worldScale();
        const Vector2 position {
            dualCell.center.x * level.worldScale(),
            dualCell.center.y * level.worldScale()
        };
        const float playerDistance = distanceSquared(position, playerPosition);
        if (!std::isfinite(clearance) || !std::isfinite(position.x)
            || !std::isfinite(position.y) || !std::isfinite(playerDistance)
            || clearance < MINIMUM_ENEMY_CLEARANCE
            || playerDistance
                < MINIMUM_PLAYER_SPAWN_DISTANCE
                    * MINIMUM_PLAYER_SPAWN_DISTANCE
            || !farEnoughFromRoomDoorways(level, room, position)
            || !hasWallClearance(level, position)
            || !hasEnemySeparation(selected, position)) {
            continue;
        }

        selected.push_back(GeneratedEnemySpawn {
            .id = static_cast<EnemyId>(cell),
            .position = position
        });
        if (selected.size() == GENERATED_ENEMIES_PER_ENCOUNTER) {
            break;
        }
    }
    return selected;
}

std::optional<Vector2> selectGeneratedEnemySpawn(
    const GeneratedLevel& level, int room, Vector2 playerPosition)
{
    const auto spawns = selectGeneratedEnemySpawns(
        level, room, playerPosition);
    return spawns.empty()
        ? std::nullopt
        : std::optional<Vector2> { spawns.front().position };
}

GeneratedEncounterStepResult updateGeneratedEncounter(
    GeneratedEncounterCoordinator& coordinator, LevelSession& session,
    const GeneratedLevel& level, const PlayerInput& input, float stepTime)
{
    GeneratedEncounterStepResult result;
    if (input.restartPressed) {
        result.levelSession = session.update(input, stepTime);
        resetGeneratedEncounter(coordinator);
        return result;
    }

    if (coordinator.fighting) {
        result.combat = updateGeneratedCombat(coordinator.playerAttack,
            coordinator.combatState, session.player(), input,
            stepTime, session.activeWalls());
        if (coordinator.combatState.enemies.allDefeated()
            && coordinator.roomId.has_value()) {
            const int room = *coordinator.roomId;
            if (session.clearEncounter(room)) {
                coordinator.combatState.enemyProjectiles
                    = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
                coordinator.fighting = false;
                result.encounterCleared = true;
            }
        }
        return result;
    }

    result.levelSession = session.update(input, stepTime);
    updateGeneratedPlayerWeapon(
        coordinator.playerAttack, session, input, stepTime);
    if (!result.levelSession.enteredRoom.has_value()) {
        return result;
    }

    const int room = *result.levelSession.enteredRoom;
    const auto* generatedRoom = generated_level::findRoomById(level, room);
    if (generatedRoom == nullptr
        || session.roomStates()[static_cast<std::size_t>(room)]
            != RoomLifecycleState::entered) {
        return result;
    }
    if (generatedRoom->role == stalberg::rooms::RoomRole::Exit) {
        session.markRoomCleared(room);
        coordinator.floorComplete = true;
        result.floorCompleted = true;
        return result;
    }
    if (!roomStartsEncounter(*generatedRoom)) {
        return result;
    }

    const Vector2 playerPosition {
        session.player().position.x,
        session.player().position.z
    };
    const auto spawns = selectGeneratedEnemySpawns(
        level, room, playerPosition);
    if (spawns.empty()) {
        session.markRoomCleared(room);
        return result;
    }

    if (!session.beginEncounter(room)) {
        return result;
    }

    const Vector2 lockedPosition {
        session.player().position.x,
        session.player().position.z
    };
    resolvePlayerWallCollisions(
        session.player(), lockedPosition, session.activeWalls());
    beginGeneratedCombat(coordinator.combatState, spawns);
    coordinator.roomId = room;
    coordinator.fighting = true;
    result.encounterStarted = true;
    return result;
}
