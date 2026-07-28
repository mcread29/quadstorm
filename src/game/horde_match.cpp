#include "horde_match.hpp"

#include "arena.hpp"
#include "collision_2d.hpp"
#include "vector2_math.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <queue>
#include <ranges>
#include <set>
#include <utility>

namespace {

constexpr float INTERACTION_DISTANCE = 1.8F;
constexpr float DEVICE_INTERACTION_DISTANCE = 2.2F;
constexpr float RELAY_TARGET_RADIUS = 0.42F;
constexpr float CONTACT_DAMAGE_DISTANCE = ENEMY_RADIUS + PLAYER_RADIUS;
constexpr std::size_t MAXIMUM_LIVING_HORDE = 18;
constexpr float BUILDUP_DURATION = 2.0F;
constexpr float BUILDUP_SPAWN_INTERVAL = 0.72F;
constexpr float PEAK_SPAWN_INTERVAL = 0.42F;

using CellIndex = stalberg::rooms::CellIndex;
using vector2::distanceSquared;
using vector2::length;
using vector2::normalized;

const stalberg::rooms::GeneratedRoom* roomWithRole(
    const GeneratedLevel& level, stalberg::rooms::RoomRole role)
{
    const auto rooms = level.roomLayout().getRooms();
    const auto room = std::ranges::find(rooms, role,
        &stalberg::rooms::GeneratedRoom::role);
    return room == rooms.end() ? nullptr : &*room;
}

CellIndex highestClearanceCell(const GeneratedLevel& level, int room)
{
    const auto assignments = level.roomLayout().getCellAssignments();
    CellIndex selected = assignments.size();
    float selectedClearance = -1.0F;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] != room) {
            continue;
        }
        const float clearance = level.dualGrid().cells[cell].clearance;
        if (clearance > selectedClearance
            || (clearance == selectedClearance && cell < selected)) {
            selected = cell;
            selectedClearance = clearance;
        }
    }
    return selected == assignments.size() ? level.playerSpawnCell() : selected;
}

Vector2 worldCellCenter(const GeneratedLevel& level, CellIndex cell)
{
    const stalberg::Point center = level.dualGrid().cells[cell].center;
    return Vector2 {
        center.x * level.worldScale(), center.y * level.worldScale()
    };
}

struct RoomGraphEdge {
    int room = stalberg::rooms::EMPTY_CELL;
    std::size_t doorway = 0;
};

std::vector<std::size_t> doorwayPath(
    const GeneratedLevel& level, int start, int destination)
{
    const std::size_t roomCount = level.roomLayout().getRoomCount();
    if (start < 0 || destination < 0
        || static_cast<std::size_t>(start) >= roomCount
        || static_cast<std::size_t>(destination) >= roomCount) {
        return {};
    }

    std::vector<std::vector<RoomGraphEdge>> graph(roomCount);
    const auto doorways = level.roomLayout().getDoorways();
    for (std::size_t index = 0; index < doorways.size(); ++index) {
        const auto& doorway = doorways[index];
        graph[static_cast<std::size_t>(doorway.firstRegion)].push_back(
            RoomGraphEdge { doorway.secondRegion, index });
        graph[static_cast<std::size_t>(doorway.secondRegion)].push_back(
            RoomGraphEdge { doorway.firstRegion, index });
    }
    for (auto& edges : graph) {
        std::ranges::sort(edges, {}, &RoomGraphEdge::room);
    }

    std::vector<int> parent(roomCount, stalberg::rooms::EMPTY_CELL);
    std::vector<std::size_t> parentDoor(roomCount, doorways.size());
    std::queue<int> frontier;
    parent[static_cast<std::size_t>(start)] = start;
    frontier.push(start);
    while (!frontier.empty() && parent[static_cast<std::size_t>(destination)] < 0) {
        const int room = frontier.front();
        frontier.pop();
        for (const RoomGraphEdge edge : graph[static_cast<std::size_t>(room)]) {
            if (parent[static_cast<std::size_t>(edge.room)] >= 0) {
                continue;
            }
            parent[static_cast<std::size_t>(edge.room)] = room;
            parentDoor[static_cast<std::size_t>(edge.room)] = edge.doorway;
            frontier.push(edge.room);
        }
    }
    if (parent[static_cast<std::size_t>(destination)] < 0) {
        return {};
    }

    std::vector<std::size_t> result;
    for (int room = destination; room != start;
         room = parent[static_cast<std::size_t>(room)]) {
        result.push_back(parentDoor[static_cast<std::size_t>(room)]);
    }
    std::ranges::reverse(result);
    return result;
}

bool roomsRemainConnectedWithoutDoorway(const GeneratedLevel& level,
    int start, int destination, std::size_t blockedDoorway)
{
    const std::size_t roomCount = level.roomLayout().getRoomCount();
    if (start < 0 || destination < 0
        || static_cast<std::size_t>(start) >= roomCount
        || static_cast<std::size_t>(destination) >= roomCount) {
        return false;
    }
    std::vector<bool> reached(roomCount, false);
    std::queue<int> frontier;
    reached[static_cast<std::size_t>(start)] = true;
    frontier.push(start);
    const auto doorways = level.roomLayout().getDoorways();
    while (!frontier.empty()) {
        const int room = frontier.front();
        frontier.pop();
        for (std::size_t doorway = 0; doorway < doorways.size(); ++doorway) {
            if (doorway == blockedDoorway) {
                continue;
            }
            const auto& edge = doorways[doorway];
            int neighbor = stalberg::rooms::EMPTY_CELL;
            if (edge.firstRegion == room) {
                neighbor = edge.secondRegion;
            } else if (edge.secondRegion == room) {
                neighbor = edge.firstRegion;
            }
            if (neighbor < 0 || reached[static_cast<std::size_t>(neighbor)]) {
                continue;
            }
            reached[static_cast<std::size_t>(neighbor)] = true;
            frontier.push(neighbor);
        }
    }
    return reached[static_cast<std::size_t>(destination)];
}

void addGate(std::vector<MapGate>& gates, std::set<std::size_t>& used,
    std::span<const std::size_t> path, bool fromFront,
    GatePurpose purpose, int cost)
{
    if (path.empty()) {
        return;
    }
    if (fromFront) {
        for (const std::size_t doorway : path) {
            if (used.insert(doorway).second) {
                gates.push_back(MapGate { doorway, purpose, cost, false });
                return;
            }
        }
    } else {
        for (auto doorway = path.rbegin(); doorway != path.rend(); ++doorway) {
            if (used.insert(*doorway).second) {
                gates.push_back(MapGate { *doorway, purpose, cost, false });
                return;
            }
        }
    }
}

void addSafeRewardGate(const GeneratedLevel& level,
    std::vector<MapGate>& gates, std::set<std::size_t>& used,
    std::span<const std::size_t> rewardPath, int startRoom,
    int rewardRoom, int exitRoom)
{
    for (auto doorway = rewardPath.rbegin(); doorway != rewardPath.rend();
         ++doorway) {
        if (used.contains(*doorway)
            || !roomsRemainConnectedWithoutDoorway(
                level, startRoom, exitRoom, *doorway)
            || roomsRemainConnectedWithoutDoorway(
                level, startRoom, rewardRoom, *doorway)) {
            continue;
        }
        used.insert(*doorway);
        gates.push_back(MapGate {
            *doorway, GatePurpose::Reward, 800, false
        });
        return;
    }
}

bool gateForPurposeIsOpen(const HordeMatch& match, GatePurpose purpose)
{
    const auto gate = std::ranges::find(
        match.plan().gates, purpose, &MapGate::purpose);
    return gate == match.plan().gates.end() || gate->open;
}

int enemyMaximumHealth(HordeEnemyRole role)
{
    switch (role) {
    case HordeEnemyRole::Drifter:
        return 3;
    case HordeEnemyRole::Runner:
        return 2;
    case HordeEnemyRole::Caster:
        return 6;
    case HordeEnemyRole::Elite:
        return 12;
    }
    return 3;
}

float enemySpeed(HordeEnemyRole role)
{
    switch (role) {
    case HordeEnemyRole::Drifter:
        return 1.9F;
    case HordeEnemyRole::Runner:
        return 3.5F;
    case HordeEnemyRole::Caster:
        return 1.55F;
    case HordeEnemyRole::Elite:
        return 1.35F;
    }
    return 1.9F;
}

int enemyKillReward(HordeEnemyRole role)
{
    switch (role) {
    case HordeEnemyRole::Drifter:
        return 60;
    case HordeEnemyRole::Runner:
        return 70;
    case HordeEnemyRole::Caster:
        return 100;
    case HordeEnemyRole::Elite:
        return 250;
    }
    return 60;
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
        const Vector2 position = worldCellCenter(level, cell);
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
    entry.enemy.position = worldCellCenter(level, *selected);
    entry.enemy.previousPosition = entry.enemy.position;
    entry.enemy.health = enemyMaximumHealth(role);
    entry.enemy.shotCooldownRemaining = role == HordeEnemyRole::Caster
        || role == HordeEnemyRole::Elite
        ? ENEMY_FIRST_SHOT_DELAY
        : std::numeric_limits<float>::infinity();
    match.enemyEntries.push_back(entry);
    return true;
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
                destination = worldCellCenter(level, *nextCell);
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
        const float speed = enemySpeed(entry.role);
        enemy.velocity = Vector2 { direction.x * speed, direction.y * speed };
        enemy.position.x += enemy.velocity.x * stepTime;
        enemy.position.y += enemy.velocity.y * stepTime;
        resolveCircleWallCollisions(enemy.position, enemy.velocity,
            ENEMY_RADIUS, enemy.previousPosition, session.activeWalls());
    }

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
        match.grantPoints(10);
        if (selected->enemy.health <= 0) {
            selected->enemy.health = 0;
            selected->enemy.velocity = Vector2 {};
            match.grantPoints(enemyKillReward(selected->role));
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
    --player.health;
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

} // namespace

SmallMapPlan buildSmallMapPlan(const GeneratedLevel& level)
{
    SmallMapPlan plan;
    plan.recipe = level.roomLayout().getSmallMapRecipe();
    const auto* start = roomWithRole(level, stalberg::rooms::RoomRole::Start);
    const auto* hub = roomWithRole(level, stalberg::rooms::RoomRole::Hub);
    const auto* reward = roomWithRole(level, stalberg::rooms::RoomRole::Reward);
    const auto* exit = roomWithRole(level, stalberg::rooms::RoomRole::Exit);
    plan.startRoom = start == nullptr ? stalberg::rooms::EMPTY_CELL : start->id;
    plan.hubRoom = hub == nullptr ? plan.startRoom : hub->id;
    plan.rewardRoom = reward == nullptr ? stalberg::rooms::EMPTY_CELL : reward->id;
    plan.exitRoom = exit == nullptr ? stalberg::rooms::EMPTY_CELL : exit->id;

    for (const auto& room : level.roomLayout().getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Combat
            && room.id != plan.rewardRoom) {
            plan.anchorRoom = room.id;
            break;
        }
    }
    if (plan.anchorRoom == stalberg::rooms::EMPTY_CELL) {
        plan.anchorRoom = plan.hubRoom;
    }

    plan.hubCell = highestClearanceCell(level, plan.hubRoom);
    plan.anchorCell = highestClearanceCell(level, plan.anchorRoom);
    plan.exitCell = highestClearanceCell(level, plan.exitRoom);
    plan.hubPosition = worldCellCenter(level, plan.hubCell);
    plan.anchorPosition = worldCellCenter(level, plan.anchorCell);
    plan.exitPosition = worldCellCenter(level, plan.exitCell);

    std::set<std::size_t> usedDoorways;
    const std::vector<std::size_t> startToHub
        = doorwayPath(level, plan.startRoom, plan.hubRoom);
    const std::vector<std::size_t> hubToAnchor
        = doorwayPath(level, plan.hubRoom, plan.anchorRoom);
    const std::vector<std::size_t> hubToReward
        = doorwayPath(level, plan.hubRoom, plan.rewardRoom);
    const std::vector<std::size_t> hubToExit
        = doorwayPath(level, plan.hubRoom, plan.exitRoom);
    addGate(plan.gates, usedDoorways, startToHub, true,
        GatePurpose::Expansion, 500);
    addGate(plan.gates, usedDoorways, hubToAnchor, false,
        GatePurpose::Anchor, 750);
    addGate(plan.gates, usedDoorways, hubToExit, false,
        GatePurpose::Exit, 0);
    addSafeRewardGate(level, plan.gates, usedDoorways, hubToReward,
        plan.startRoom, plan.rewardRoom, plan.exitRoom);

    if (reward != nullptr) {
        std::vector<CellIndex> relayCells(
            reward->coverCandidates.begin(), reward->coverCandidates.end());
        if (relayCells.size() < 3) {
            const auto assignments = level.roomLayout().getCellAssignments();
            for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
                if (assignments[cell] == reward->id) {
                    relayCells.push_back(cell);
                }
            }
        }
        std::ranges::sort(relayCells);
        relayCells.erase(std::unique(relayCells.begin(), relayCells.end()),
            relayCells.end());
        const std::size_t targetCount = std::min<std::size_t>(3, relayCells.size());
        for (std::size_t target = 0; target < targetCount; ++target) {
            const std::size_t index = targetCount == 1
                ? 0
                : target * (relayCells.size() - 1) / (targetCount - 1);
            const CellIndex cell = relayCells[index];
            plan.relayTargets.push_back(
                RelayTarget { cell, worldCellCenter(level, cell) });
        }
    }
    return plan;
}

std::vector<HordeEnemyRole> hordeCompositionForRound(int round)
{
    int drifters = 0;
    int runners = 0;
    int casters = 0;
    int elites = 0;
    switch (round) {
    case 1:
        drifters = 6;
        break;
    case 2:
        drifters = 8;
        runners = 2;
        break;
    case 3:
        drifters = 10;
        runners = 2;
        casters = 1;
        break;
    case 4:
        drifters = 12;
        runners = 3;
        casters = 1;
        break;
    default:
        drifters = 14;
        runners = 4;
        casters = 2;
        elites = 1;
        break;
    }
    std::vector<HordeEnemyRole> result;
    result.insert(result.end(), drifters, HordeEnemyRole::Drifter);
    result.insert(result.end(), runners, HordeEnemyRole::Runner);
    result.insert(result.end(), casters, HordeEnemyRole::Caster);
    result.insert(result.end(), elites, HordeEnemyRole::Elite);
    return result;
}

std::optional<CellIndex> nextHordeNavigationCell(
    const GeneratedLevel& level, const LevelSession& session,
    CellIndex start, CellIndex destination)
{
    const std::size_t cellCount = level.roomGrid().getCellCount();
    if (start >= cellCount || destination >= cellCount) {
        return std::nullopt;
    }
    if (start == destination) {
        return start;
    }
    std::vector<CellIndex> parent(cellCount, cellCount);
    std::queue<CellIndex> frontier;
    parent[start] = start;
    frontier.push(start);
    while (!frontier.empty() && parent[destination] == cellCount) {
        const CellIndex cell = frontier.front();
        frontier.pop();
        for (const CellIndex neighbor : level.traversableNeighbors(cell)) {
            if (parent[neighbor] != cellCount
                || !session.canTraverse(cell, neighbor)) {
                continue;
            }
            parent[neighbor] = cell;
            frontier.push(neighbor);
        }
    }
    if (parent[destination] == cellCount) {
        return std::nullopt;
    }
    CellIndex step = destination;
    while (parent[step] != start) {
        step = parent[step];
    }
    return step;
}

HordeMatch::HordeMatch(const GeneratedLevel& sourceLevel, LevelSession& session)
    : level(&sourceLevel)
    , mapPlan(buildSmallMapPlan(sourceLevel))
{
    reset(session);
}

bool HordeMatch::hasUpgrade(MatchUpgrade upgrade) const
{
    switch (upgrade) {
    case MatchUpgrade::Damage:
        return damageUpgrade;
    case MatchUpgrade::FireRate:
        return fireRateUpgrade;
    case MatchUpgrade::Dash:
        return dashUpgrade;
    }
    return false;
}

void HordeMatch::grantPoints(int amount)
{
    pointTotal = std::max(0, pointTotal + std::max(amount, 0));
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
    int cost = 0;
    bool* purchased = nullptr;
    switch (upgrade) {
    case MatchUpgrade::Damage:
        cost = 1200;
        purchased = &damageUpgrade;
        break;
    case MatchUpgrade::FireRate:
        cost = 1000;
        purchased = &fireRateUpgrade;
        break;
    case MatchUpgrade::Dash:
        cost = 900;
        purchased = &dashUpgrade;
        break;
    }
    if (purchased == nullptr || *purchased || pointTotal < cost
        || !gateForPurposeIsOpen(*this, GatePurpose::Anchor)) {
        return false;
    }
    pointTotal -= cost;
    *purchased = true;
    if (upgrade == MatchUpgrade::Damage) {
        damagePerShot = 2;
    } else if (upgrade == MatchUpgrade::FireRate) {
        playerAttack.weapon.fireInterval = 0.075F;
    } else {
        session.player().dashCooldownScale = 0.7F;
    }
    return true;
}

bool HordeMatch::startNextRound()
{
    if (victory || roundPhase != RoundPhase::Intermission
        || currentRound >= HORDE_FINAL_ROUND
        || (currentRound >= 2 && !anchorComplete && !anchorActive)
        || (currentRound >= 3 && anchorComplete && !hubPowered)) {
        return false;
    }
    ++currentRound;
    pendingSpawns = hordeCompositionForRound(currentRound);
    nextPendingSpawn = 0;
    enemyEntries.clear();
    hostileProjectiles = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
    roundPhase = RoundPhase::Buildup;
    phaseElapsed = 0.0F;
    spawnCooldown = 0.0F;
    return true;
}

bool HordeMatch::activateAnchor()
{
    if (anchorActive || anchorComplete || currentRound < 2
        || roundPhase != RoundPhase::Intermission
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
    if (!anchorComplete || hubPowered
        || roundPhase != RoundPhase::Intermission) {
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
        if (!fireRateUpgrade) {
            fireRateUpgrade = true;
            playerAttack.weapon.fireInterval = 0.075F;
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
    phaseElapsed = 0.0F;
    spawnCooldown = 0.0F;
    damageUpgrade = false;
    fireRateUpgrade = false;
    dashUpgrade = false;
    anchorActive = false;
    anchorComplete = false;
    anchorProgressSeconds = 0.0F;
    hubPowered = false;
    relaySequenceProgress = 0;
    relayComplete = false;
    victory = false;
    for (MapGate& gate : mapPlan.gates) {
        gate.open = false;
        session.setDoorwayLocked(gate.doorway, true);
    }
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
        std::optional<std::size_t> nearbyGate;
        float nearestGateDistance = INTERACTION_DISTANCE;
        const auto thresholds = match.level->doorwayThresholds();
        for (std::size_t gate = 0; gate < match.mapPlan.gates.size(); ++gate) {
            const MapGate& candidate = match.mapPlan.gates[gate];
            if (candidate.open || candidate.doorway >= thresholds.size()) {
                continue;
            }
            const float distance = distanceToThreshold(
                playerPosition, thresholds[candidate.doorway]);
            if (distance <= nearestGateDistance) {
                nearbyGate = gate;
                nearestGateDistance = distance;
            }
        }
        if (nearbyGate.has_value()) {
            result.gatePurchased = match.purchaseGate(session, *nearbyGate);
        } else if (closeTo(playerPosition, match.mapPlan.anchorPosition,
                       DEVICE_INTERACTION_DISTANCE)) {
            const auto anchorGate = std::ranges::find(match.mapPlan.gates,
                GatePurpose::Anchor, &MapGate::purpose);
            if (anchorGate != match.mapPlan.gates.end() && !anchorGate->open) {
                const std::size_t anchorGateIndex
                    = static_cast<std::size_t>(std::distance(
                        match.mapPlan.gates.begin(), anchorGate));
                result.gatePurchased = match.purchaseGate(
                    session, anchorGateIndex);
            }
            result.objectiveAdvanced = match.activateAnchor();
            if (result.objectiveAdvanced
                && match.roundPhase == RoundPhase::Intermission) {
                result.roundStarted = match.startNextRound();
            }
        } else if (closeTo(playerPosition, match.mapPlan.hubPosition,
                       DEVICE_INTERACTION_DISTANCE)) {
            result.objectiveAdvanced = match.activateHub(session);
        } else if (closeTo(playerPosition, match.mapPlan.exitPosition,
                       DEVICE_INTERACTION_DISTANCE)
            && match.hubPowered
            && match.currentRound >= HORDE_FINAL_ROUND
            && match.roundPhase == RoundPhase::Intermission) {
            match.victory = true;
            result.victory = true;
        }
    }

    const bool nearHub = closeTo(playerPosition, match.mapPlan.hubPosition,
        DEVICE_INTERACTION_DISTANCE);
    if (nearHub && input.buyDamagePressed) {
        result.objectiveAdvanced |= match.purchaseUpgrade(
            session, MatchUpgrade::Damage);
    }
    if (nearHub && input.buyFireRatePressed) {
        result.objectiveAdvanced |= match.purchaseUpgrade(
            session, MatchUpgrade::FireRate);
    }
    if (nearHub && input.buyDashPressed) {
        result.objectiveAdvanced |= match.purchaseUpgrade(
            session, MatchUpgrade::Dash);
    }
    if (input.startRoundPressed) {
        result.roundStarted = match.startNextRound();
    }

    if (match.roundPhase != RoundPhase::Intermission) {
        match.phaseElapsed += stepTime;
        if (match.roundPhase == RoundPhase::Buildup
            && match.phaseElapsed >= BUILDUP_DURATION) {
            match.roundPhase = RoundPhase::Peak;
            match.phaseElapsed = 0.0F;
        }
        match.spawnCooldown = std::max(0.0F, match.spawnCooldown - stepTime);
        if (match.nextPendingSpawn < match.pendingSpawns.size()
            && livingEnemyCount(match.enemyEntries) < MAXIMUM_LIVING_HORDE
            && match.spawnCooldown <= 0.0F
            && spawnEnemy(match, session,
                match.pendingSpawns[match.nextPendingSpawn])) {
            ++match.nextPendingSpawn;
            match.spawnCooldown = match.roundPhase == RoundPhase::Buildup
                ? BUILDUP_SPAWN_INTERVAL
                : PEAK_SPAWN_INTERVAL;
        }
        if (match.nextPendingSpawn == match.pendingSpawns.size()) {
            match.roundPhase = RoundPhase::Cleanup;
        }
    }

    updateHordeMovement(match, session, stepTime);
    result.enemyDamage = updateHordeDamage(match, stepTime);
    match.playerAttack.projectiles.retireExpired();

    for (HordeEnemy& entry : match.enemyEntries) {
        if ((entry.role == HordeEnemyRole::Caster
                || entry.role == HordeEnemyRole::Elite)
            && isEnemyAlive(entry.enemy)) {
            result.enemyFired |= updateEnemyPattern(entry.enemy,
                match.hostileProjectiles, playerPosition, stepTime,
                session.activeWalls());
        }
    }
    match.hostileProjectiles.update(stepTime);
    resolveProjectileWallCollisions(
        match.hostileProjectiles, session.activeWalls());
    result.playerDamage = updatePlayerDamage(session.player(),
        match.hostileProjectiles, previousPlayerPosition, stepTime);
    const PlayerDamageResult contactDamage
        = updateContactDamage(match, session);
    if (contactDamage == PlayerDamageResult::died
        || (contactDamage == PlayerDamageResult::hit
            && result.playerDamage == PlayerDamageResult::none)) {
        result.playerDamage = contactDamage;
    }
    match.hostileProjectiles.retireExpired();

    if (isPlayerAlive(session.player()) && match.anchorActive
        && match.roundPhase != RoundPhase::Intermission
        && closeTo(playerPosition, match.mapPlan.anchorPosition,
            ANCHOR_HOLDOUT_RADIUS)) {
        result.objectiveAdvanced |= match.advanceAnchor(stepTime);
    }

    if (match.roundPhase == RoundPhase::Cleanup
        && match.nextPendingSpawn == match.pendingSpawns.size()
        && livingEnemyCount(match.enemyEntries) == 0
        && (!match.anchorActive || match.anchorComplete)) {
        match.enemyEntries.clear();
        match.hostileProjectiles = ProjectilePool { ENEMY_PROJECTILE_PROFILE };
        match.roundPhase = RoundPhase::Intermission;
        match.phaseElapsed = 0.0F;
        result.roundCompleted = true;
    }
    return result;
}
