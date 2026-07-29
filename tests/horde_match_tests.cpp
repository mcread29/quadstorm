#include "game/generated_level_queries.hpp"
#include "game/horde_match.hpp"
#include "game/match_generator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <ranges>
#include <string_view>

namespace {

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool canReachCell(const GeneratedLevel& level,
    const LevelSession& session, stalberg::rooms::CellIndex start,
    stalberg::rooms::CellIndex destination)
{
    std::vector<bool> reached(level.roomGrid().getCellCount(), false);
    std::queue<stalberg::rooms::CellIndex> frontier;
    reached[start] = true;
    frontier.push(start);
    while (!frontier.empty()) {
        const auto cell = frontier.front();
        frontier.pop();
        for (const auto neighbor : level.traversableNeighbors(cell)) {
            if (!reached[neighbor] && session.canTraverse(cell, neighbor)) {
                reached[neighbor] = true;
                frontier.push(neighbor);
            }
        }
    }
    return reached[destination];
}

std::size_t gateIndex(const HordeMatch& match, GatePurpose purpose)
{
    const auto gate = std::ranges::find(
        match.plan().gates, purpose, &MapGate::purpose);
    return gate == match.plan().gates.end()
        ? match.plan().gates.size()
        : static_cast<std::size_t>(gate - match.plan().gates.begin());
}

bool advanceUntilRoundStarts(HordeMatch& match, LevelSession& session,
    float maximumSeconds)
{
    constexpr float fixedStep = 1.0F / 120.0F;
    const int ticks = static_cast<int>(std::ceil(maximumSeconds / fixedStep));
    for (int tick = 0; tick < ticks; ++tick) {
        if (updateHordeMatch(
                match, session, PlayerInput {}, fixedStep).roundStarted) {
            return true;
        }
    }
    return false;
}

bool everyRecipeCompilesPlayableMatch()
{
    bool valid = true;
    for (std::size_t configuration = 0;
         configuration < REPRESENTATIVE_LEVEL_CONFIGS.size(); ++configuration) {
        const GeneratedLevel level(REPRESENTATIVE_LEVEL_CONFIGS[configuration]);
        LevelSession session(level);
        HordeMatch match(level, session);
        valid &= check(match.plan().recipe
                == static_cast<stalberg::rooms::SmallMapRecipe>(
                    configuration % 3),
            "representative maps cycle through all three recipes");
        valid &= check(match.plan().startRoom >= 0
                && match.plan().hubRoom >= 0
                && match.plan().anchorRoom >= 0
                && match.plan().rewardRoom >= 0
                && match.plan().exitRoom >= 0,
            "every recipe binds all required semantic rooms");
        valid &= check(match.plan().relayTargets.size() == 3,
            "every recipe binds a complete optional relay puzzle");
        valid &= check(gateIndex(match, GatePurpose::Expansion)
                    < match.plan().gates.size()
                && gateIndex(match, GatePurpose::Anchor)
                    < match.plan().gates.size()
                && gateIndex(match, GatePurpose::Reward)
                    < match.plan().gates.size()
                && gateIndex(match, GatePurpose::Exit)
                    < match.plan().gates.size(),
            "every recipe binds expansion, Anchor, Reward, and Exit gates");
        for (const GatePurpose purpose
            : { GatePurpose::Expansion, GatePurpose::Anchor }) {
            const std::size_t gate = gateIndex(match, purpose);
            if (gate < match.plan().gates.size()) {
                match.grantPoints(match.plan().gates[gate].cost);
                valid &= check(match.purchaseGate(session, gate),
                    "required representative gate can be purchased");
            }
        }
        match.anchorComplete = true;
        valid &= check(match.activateHub(session),
            "representative Hub can open its objective Exit route");
        const std::size_t rewardGate = gateIndex(
            match, GatePurpose::Reward);
        valid &= check(canReachCell(level, session,
                           level.playerSpawnCell(), match.plan().exitCell)
                && rewardGate < match.plan().gates.size()
                && session.doorwayIsLocked(
                    match.plan().gates[rewardGate].doorway),
            "Reward gate remains optional on every representative map");
        valid &= check(!canReachCell(level, session,
                           level.playerSpawnCell(),
                           match.plan().relayTargets.front().cell),
            "locked Reward gate actually protects the optional relay chamber");
    }
    return valid;
}

bool recipeCompilesPuzzleSitesAndGates(
    const GeneratedLevel& level, const HordeMatch& match,
    const LevelSession& session)
{
    const SmallMapPlan& plan = match.plan();
    bool valid = check(plan.recipe
            == stalberg::rooms::SmallMapRecipe::HubCircuit,
        "default small map publishes the Hub Circuit recipe");
    valid &= check(plan.startRoom >= 0 && plan.hubRoom >= 0
            && plan.anchorRoom >= 0 && plan.rewardRoom >= 0
            && plan.exitRoom >= 0,
        "recipe binds Start, Hub, Anchor, Reward, and Exit rooms");
    valid &= check(plan.hubCell < level.roomGrid().getCellCount()
            && plan.anchorCell < level.roomGrid().getCellCount()
            && plan.exitCell < level.roomGrid().getCellCount(),
        "recipe sites use valid semantic cells");
    valid &= check(plan.relayTargets.size() == 3,
        "small puzzle map publishes a three-target relay sequence");
    valid &= check(plan.gates.size() >= 3,
        "small puzzle map publishes economy and objective gates");
    const auto expansion = std::ranges::find(
        plan.gates, GatePurpose::Expansion, &MapGate::purpose);
    const auto anchor = std::ranges::find(
        plan.gates, GatePurpose::Anchor, &MapGate::purpose);
    valid &= check(expansion != plan.gates.end() && expansion->cost <= 540,
        "Round 1 guarantees enough points for the first required gate");
    valid &= check(anchor != plan.gates.end() && anchor->cost <= 900,
        "Round 2 guarantees enough points for the Anchor route");
    for (const MapGate& gate : plan.gates) {
        valid &= check(gate.doorway < level.doorwayThresholds().size(),
            "every recipe gate binds an exact doorway threshold");
        valid &= check(session.doorwayIsLocked(gate.doorway),
            "recipe gates begin collision- and navigation-locked");
    }
    return valid;
}

bool hordeDamageAwardsPointsOnce(const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    HordeEnemy enemy;
    enemy.id = 1;
    enemy.role = HordeEnemyRole::Drifter;
    enemy.enemy.position = Vector2 {
        session.player().position.x + 1.0F,
        session.player().position.z
    };
    enemy.enemy.previousPosition = enemy.enemy.position;
    enemy.enemy.health = 1;
    enemy.killReward = 60;
    match.enemyEntries.push_back(enemy);
    Projectile& projectile = match.playerAttack.projectiles.projectiles()[0];
    projectile.active = true;
    projectile.position = enemy.enemy.position;
    projectile.previousPosition = enemy.enemy.position;
    projectile.velocity = Vector2 {};
    projectile.remainingLifetime = 1.0F;

    const HordeMatchStepResult first = updateHordeMatch(
        match, session, PlayerInput {}, 1.0F / 120.0F);
    bool valid = check(first.enemyDamage == EnemyDamageResult::died
            && match.points() == 70,
        "a lethal Drifter hit awards ten hit points and one kill bonus");
    updateHordeMatch(match, session, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(match.points() == 70,
        "dead enemies cannot award points more than once");
    return valid;
}

bool optionalSpendingCannotConsumeRequiredProgression(
    const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    const std::size_t expansion = gateIndex(match, GatePurpose::Expansion);
    const std::size_t reward = gateIndex(match, GatePurpose::Reward);
    if (expansion >= match.plan().gates.size()
        || reward >= match.plan().gates.size()) {
        return check(false, "economy guard has expansion and Reward gates");
    }
    match.grantPoints(match.plan().gates[expansion].cost);
    bool valid = check(match.purchaseGate(session, expansion),
        "economy guard opens the required expansion gate first");
    match.grantPoints(1200);
    const int reservedPoints = match.points();
    valid &= check(!match.purchaseGate(session, reward)
            && !match.purchaseUpgrade(session, MatchUpgrade::Dash)
            && match.points() == reservedPoints,
        "optional gates and upgrades stay unavailable until Anchor access is funded");
    return valid;
}

bool anchorInteractionFundsAndStartsHoldout(const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    const std::size_t anchorGate = gateIndex(match, GatePurpose::Anchor);
    if (anchorGate >= match.plan().gates.size()) {
        return check(false, "Anchor interaction scenario has its route gate");
    }
    match.currentRound = 2;
    match.grantPoints(match.plan().gates[anchorGate].cost);
    session.player().position = Vector3 {
        match.plan().anchorPosition.x, PLAYER_RADIUS,
        match.plan().anchorPosition.y
    };
    PlayerInput input;
    input.interactPressed = true;
    const HordeMatchStepResult result = updateHordeMatch(
        match, session, input, 1.0F / 120.0F);
    return check(result.gatePurchased && result.objectiveAdvanced
            && !result.roundStarted
            && match.plan().gates[anchorGate].open
            && match.anchorIsActive() && match.round() == 2,
        "one E press funds and starts the Anchor without authorizing a round");
}

bool pointsPurchaseGatesAtomically(
    const GeneratedLevel& level, LevelSession& session, HordeMatch& match)
{
    const std::size_t expansion = gateIndex(match, GatePurpose::Expansion);
    if (expansion >= match.plan().gates.size()) {
        return check(false, "recipe provides an expansion gate");
    }
    const MapGate gate = match.plan().gates[expansion];
    match.grantPoints(gate.cost - 1);
    bool valid = check(!match.purchaseGate(session, expansion)
            && match.points() == gate.cost - 1
            && session.doorwayIsLocked(gate.doorway),
        "unaffordable purchase changes neither points nor doorway state");
    match.grantPoints(1);
    valid &= check(match.purchaseGate(session, expansion)
            && match.points() == 0
            && !session.doorwayIsLocked(gate.doorway),
        "affordable purchase atomically deducts points and opens traversal");

    const DoorwayThreshold& threshold
        = level.doorwayThresholds()[gate.doorway];
    valid &= check(session.canTraverse(
                       threshold.firstCell, threshold.secondCell)
            && session.canTraverse(
                threshold.secondCell, threshold.firstCell),
        "purchased gate opens player and enemy navigation in both directions");
    return valid;
}

bool fortressGateInteractionMatchesTheHudRange()
{
    const auto level = generateMatchLevel(MatchGenerationRequest { 101 });
    LevelSession session(*level);
    HordeMatch match(*level, session);
    const std::size_t expansion = gateIndex(match, GatePurpose::Expansion);
    if (expansion >= match.plan().gates.size()) {
        return check(false, "Fortress V1 publishes an expansion gate");
    }

    const MapGate& gate = match.plan().gates[expansion];
    const DoorwayThreshold& threshold
        = level->doorwayThresholds()[gate.doorway];
    const Vector2 midpoint {
        (threshold.segment.start.x + threshold.segment.end.x) * 0.5F,
        (threshold.segment.start.y + threshold.segment.end.y) * 0.5F
    };
    const Vector2 firstCenter = generated_level::worldCellCenter(
        *level, threshold.firstCell);
    const float x = firstCenter.x - midpoint.x;
    const float y = firstCenter.y - midpoint.y;
    const float length = std::sqrt(x * x + y * y);
    if (length <= 0.001F) {
        return check(false, "gate threshold has an interior approach direction");
    }
    constexpr float promptRangePosition
        = HORDE_GATE_INTERACTION_DISTANCE - 0.1F;
    session.player().position = Vector3 {
        midpoint.x + x / length * promptRangePosition,
        PLAYER_RADIUS,
        midpoint.y + y / length * promptRangePosition
    };
    match.grantPoints(gate.cost);
    PlayerInput input;
    input.interactPressed = true;
    const HordeMatchStepResult result = updateHordeMatch(
        match, session, input, 1.0F / 120.0F);
    return check(result.gatePurchased && gate.open
            && !session.doorwayIsLocked(gate.doorway),
        "the full HUD gate-prompt range opens a Fortress V1 doorway");
}

bool automaticDirectorStartsWithoutInputOrPuzzleState(
    const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    constexpr float fixedStep = 1.0F / 120.0F;
    bool valid = check(match.round() == 0
            && match.phase() == RoundPhase::Intermission
            && std::abs(match.timeUntilNextRound()
                - HORDE_INITIAL_COUNTDOWN_DURATION) < 0.001F,
        "reset begins with the automatic Round 1 countdown");
    for (int tick = 0; tick < 359; ++tick) {
        valid &= check(!updateHordeMatch(
                            match, session, PlayerInput {}, fixedStep)
                            .roundStarted,
            "Round 1 does not start before its countdown expires");
    }
    const bool started = advanceUntilRoundStarts(match, session, 0.1F);
    valid &= check(started && match.round() == 1
            && match.phase() == RoundPhase::Buildup
            && match.pendingSpawns == hordeCompositionForRound(
                1, match.plan().recipe),
        "Round 1 starts automatically without an input edge");

    match.enemyEntries.clear();
    match.pendingSpawns.clear();
    match.nextPendingSpawn = 0;
    match.roundPhase = RoundPhase::Cleanup;
    match.anchorActive = true;
    const Vector2 projectileOrigin {
        session.player().position.x, session.player().position.z
    };
    match.playerAttack.projectiles.spawn(
        projectileOrigin, session.player().facing);
    const Vector2 hostileOrigin {
        projectileOrigin.x + session.player().facing.x * 1.5F,
        projectileOrigin.y + session.player().facing.y * 1.5F
    };
    match.hostileProjectiles.spawn(hostileOrigin, session.player().facing);
    valid &= check(match.enemyProjectiles().activeCount() == 1,
        "cleanup scenario begins with a live hostile projectile");
    const HordeMatchStepResult completed = updateHordeMatch(
        match, session, PlayerInput {}, fixedStep);
    valid &= check(completed.roundCompleted
            && match.phase() == RoundPhase::Intermission
            && match.anchorIsActive()
            && match.playerProjectiles().activeCount() == 1
            && match.enemyProjectiles().activeCount() == 0,
        "cleanup ignores puzzle state, clears hostile shots, and preserves player attacks");
    valid &= check(advanceUntilRoundStarts(
                       match, session, HORDE_INTERMISSION_DURATION + 0.1F)
            && match.round() == 2,
        "the next round starts after a fixed intermission while the puzzle remains active");

    match.anchorActive = false;
    match.anchorComplete = true;
    match.hubPowered = false;
    match.enemyEntries.clear();
    match.pendingSpawns.clear();
    match.nextPendingSpawn = 0;
    match.roundPhase = RoundPhase::Cleanup;
    updateHordeMatch(match, session, PlayerInput {}, fixedStep);
    valid &= check(advanceUntilRoundStarts(
                       match, session, HORDE_INTERMISSION_DURATION + 0.1F)
            && match.round() == 3,
        "a completed Anchor and dormant Hub cannot pause the director");

    match.currentRound = HORDE_EXTRACTION_MINIMUM_ROUND;
    match.hubPowered = true;
    match.relayComplete = true;
    match.enemyEntries.clear();
    match.pendingSpawns.clear();
    match.nextPendingSpawn = 0;
    match.roundPhase = RoundPhase::Cleanup;
    updateHordeMatch(match, session, PlayerInput {}, fixedStep);
    valid &= check(advanceUntilRoundStarts(
                       match, session, HORDE_INTERMISSION_DURATION + 0.1F)
            && match.round() == HORDE_EXTRACTION_MINIMUM_ROUND + 1
            && !match.matchIsComplete(),
        "completed quests and declined extraction preserve endless continuation");
    return valid;
}

bool deterministicDifficultyScalesAndStaysBounded()
{
    const auto recipe = stalberg::rooms::SmallMapRecipe::HubCircuit;
    const auto round1 = hordeDifficultyForRound(1, recipe);
    const auto round5 = hordeDifficultyForRound(5, recipe);
    const auto round10 = hordeDifficultyForRound(10, recipe);
    const auto round25 = hordeDifficultyForRound(25, recipe);
    const auto round100 = hordeDifficultyForRound(100, recipe);
    const auto huge = hordeDifficultyForRound(
        std::numeric_limits<std::uint64_t>::max(), recipe);
    bool valid = check(round1.spawnBudget == 6
            && round1.maximumLiving == 6
            && round1.pressureTier == 0,
        "Round 1 difficulty snapshot preserves the six-Drifter opening");
    valid &= check(round5.spawnBudget == 21 && round5.eliteEvent,
        "Round 5 snapshot schedules its first readable elite event");
    valid &= check(round10.spawnBudget == 32
            && round10.maximumLiving == 11
            && round10.pressureTier == 1,
        "Round 10 snapshot increases budget and simultaneous pressure");
    valid &= check(round25.spawnBudget == 48
            && round25.maximumLiving == 18
            && round25.pressureTier == 4,
        "Round 25 snapshot reaches bounded population pressure");
    valid &= check(round100.spawnBudget == 48
            && round100.maximumLiving == 18
            && std::abs(round100.healthScale - 1.8F) < 0.001F
            && std::abs(round100.projectileSpeedScale - 1.4F) < 0.001F
            && std::abs(round100.firingIntervalScale - 0.65F) < 0.001F
            && round100.enemyDamage == 2
            && round100.rewardPercent == 150,
        "Round 100 snapshot applies bounded attribute and economy scaling");
    valid &= check(huge.spawnBudget == round100.spawnBudget
            && huge.maximumLiving == round100.maximumLiving
            && huge.pressureTier == round100.pressureTier
            && hordeCompositionForRound(
                   std::numeric_limits<std::uint64_t>::max(), recipe)
                    .size() == 48,
        "maximum round arithmetic is deterministic and overflow-safe");

    const auto hubSchedule = hordeCompositionForRound(25, recipe);
    const auto hubScheduleAgain = hordeCompositionForRound(25, recipe);
    const auto wingsSchedule = hordeCompositionForRound(25,
        stalberg::rooms::SmallMapRecipe::TwinWings);
    valid &= check(hubSchedule == hubScheduleAgain
            && std::ranges::count(hubSchedule, HordeEnemyRole::Elite) == 2
            && std::ranges::count(hubSchedule, HordeEnemyRole::Caster) > 1,
        "representative schedules are reproducible and shift composition toward pressure roles");
    valid &= check(wingsSchedule.size() == hubSchedule.size()
            && wingsSchedule != hubSchedule,
        "map recipe deterministically affects schedule ordering and composition");

    HordeDifficultyProfile previous = round1;
    for (std::uint64_t round = 2; round <= 100; ++round) {
        const HordeDifficultyProfile current
            = hordeDifficultyForRound(round, recipe);
        valid &= check(current.spawnBudget >= previous.spawnBudget
                && current.maximumLiving >= previous.maximumLiving
                && current.healthScale >= previous.healthScale
                && current.movementSpeedScale >= previous.movementSpeedScale
                && current.projectileSpeedScale
                    >= previous.projectileSpeedScale
                && current.firingIntervalScale
                    <= previous.firingIntervalScale,
            "difficulty pressure is monotonic before and through every cap");
        previous = current;
    }
    return valid;
}

bool difficultyProfileAppliesToSpawnedEnemies(const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    session.player().invulnerabilityRemaining = 100.0F;
    match.currentRound = 99;
    match.roundPhase = RoundPhase::Intermission;
    match.phaseElapsed = HORDE_INTERMISSION_DURATION;
    const HordeMatchStepResult started = updateHordeMatch(
        match, session, PlayerInput {}, 1.0F / 120.0F);
    bool valid = check(started.roundStarted && match.round() == 100
            && match.difficulty().pressureTier == 10
            && std::abs(match.enemyProjectiles().profile().speed - 9.1F)
                < 0.001F
            && match.phaseElapsed == 0.0F && match.enemies().empty(),
        "starting Round 100 installs scaling without double-consuming the boundary tick");
    updateHordeMatch(match, session, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(!match.enemies().empty(),
        "the first scaled spawn enters on the first full buildup tick");
    if (!match.enemies().empty()) {
        HordeEnemy& enemy = match.enemies().front();
        valid &= check(enemy.role == HordeEnemyRole::Drifter
                && enemy.maximumHealth == 6 && enemy.killReward == 90
                && std::abs(enemy.movementSpeed - 2.375F) < 0.001F
                && std::abs(enemy.shotInterval - 0.7475F) < 0.001F,
            "spawned enemies snapshot bounded health, reward, movement, and cadence scaling");
        enemy.enemy.health = 1;
        Projectile& projectile
            = match.playerAttack.projectiles.projectiles()[0];
        projectile.active = true;
        projectile.position = enemy.enemy.position;
        projectile.previousPosition = enemy.enemy.position;
        projectile.velocity = Vector2 {};
        projectile.remainingLifetime = 1.0F;
        match.pointTotal = 0;
        const HordeMatchStepResult damage = updateHordeMatch(
            match, session, PlayerInput {}, 1.0F / 120.0F);
        valid &= check(damage.enemyDamage == EnemyDamageResult::died
                && match.points() == 105,
            "late-round hit and kill rewards apply the bounded 1.5x economy scale");
    }

    Enemy caster;
    caster.position = Vector2 {
        session.player().position.x + 4.0F,
        session.player().position.z
    };
    caster.previousPosition = caster.position;
    caster.shotCooldownRemaining = 0.0F;
    ProjectilePool scaledHostilePool(ProjectileProfile {
        .speed = ENEMY_PROJECTILE_SPEED
            * match.difficulty().projectileSpeedScale,
        .lifetime = ENEMY_PROJECTILE_LIFETIME,
        .radius = ENEMY_PROJECTILE_RADIUS
    });
    valid &= check(updateEnemyPattern(caster, scaledHostilePool,
                       Vector2 { session.player().position.x,
                           session.player().position.z },
                       1.0F / 120.0F, {}, 0.7475F)
            && scaledHostilePool.activeCount() == 3
            && std::abs(caster.shotCooldownRemaining - 0.7475F)
                < 0.001F,
        "late-round ranged cadence and projectile profile affect authoritative shots");

    match.enemyEntries.clear();
    match.pendingSpawns.clear();
    match.nextPendingSpawn = 0;
    match.roundPhase = RoundPhase::Cleanup;
    match.hostileProjectiles = ProjectilePool {
        scaledHostilePool.profile()
    };
    match.hostileProjectiles.spawn(
        Vector2 { session.player().position.x, session.player().position.z },
        Vector2 { 1.0F, 0.0F });
    session.player().health = PLAYER_MAX_HEALTH;
    session.player().invulnerabilityRemaining = 0.0F;
    updateHordeMatch(match, session, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(session.player().health == PLAYER_MAX_HEALTH - 2,
        "late-round hostile projectiles apply bounded two-point damage");

    session.player().health = PLAYER_MAX_HEALTH;
    session.player().invulnerabilityRemaining = 0.0F;
    match.roundPhase = RoundPhase::Cleanup;
    HordeEnemy contactEnemy;
    contactEnemy.enemy.position = Vector2 {
        session.player().position.x, session.player().position.z
    };
    contactEnemy.enemy.previousPosition = contactEnemy.enemy.position;
    contactEnemy.enemy.health = 1;
    match.enemyEntries.push_back(contactEnemy);
    updateHordeMatch(match, session, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(session.player().health == PLAYER_MAX_HEALTH - 2,
        "late-round contact applies the same bounded two-point damage");

    match.enemyEntries.clear();
    match.pendingSpawns.clear();
    match.nextPendingSpawn = 0;
    match.currentRound = std::numeric_limits<std::uint64_t>::max();
    match.roundPhase = RoundPhase::Intermission;
    match.phaseElapsed = HORDE_INTERMISSION_DURATION;
    const HordeMatchStepResult maximumStarted = updateHordeMatch(
        match, session, PlayerInput {}, 1.0F / 120.0F);
    valid &= check(maximumStarted.roundStarted
            && match.round() == std::numeric_limits<std::uint64_t>::max(),
        "the director repeats the maximum representable round without wrapping");
    return valid;
}

bool spawnedHordeRespectsGeometryAndPopulation(
    const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    session.player().invulnerabilityRemaining = 100.0F;
    bool valid = check(advanceUntilRoundStarts(
                           match, session,
                           HORDE_INITIAL_COUNTDOWN_DURATION + 0.1F),
        "geometry scenario automatically starts Round 1");
    constexpr float fixedStep = 1.0F / 120.0F;
    for (int tick = 0; tick < 600; ++tick) {
        updateHordeMatch(match, session, PlayerInput {}, fixedStep);
    }
    valid &= check(match.enemies().size() == 6
            && std::ranges::all_of(match.enemies(), [](const HordeEnemy& enemy) {
                return enemy.role == HordeEnemyRole::Drifter;
            }),
        "Round 1 spawns exactly six role-identifiable Drifters");
    for (const HordeEnemy& enemy : match.enemies()) {
        valid &= check(level.cellAtWorldPoint(enemy.enemy.position).has_value(),
            "spawned and separated enemies remain on assigned floor");
        for (const Segment2D& wall : session.activeWalls()) {
            const Vector2 closest = closestPointOnSegment(
                enemy.enemy.position, wall).position;
            const float x = closest.x - enemy.enemy.position.x;
            const float y = closest.y - enemy.enemy.position.y;
            valid &= check(x * x + y * y
                    >= (ENEMY_RADIUS - 0.03F) * (ENEMY_RADIUS - 0.03F),
                "horde movement and separation preserve wall clearance");
        }
    }
    for (std::size_t first = 0; first < match.enemies().size(); ++first) {
        for (std::size_t second = first + 1;
             second < match.enemies().size(); ++second) {
            const Vector2 difference {
                match.enemies()[second].enemy.position.x
                    - match.enemies()[first].enemy.position.x,
                match.enemies()[second].enemy.position.y
                    - match.enemies()[first].enemy.position.y
            };
            valid &= check(difference.x * difference.x
                    + difference.y * difference.y > 0.01F,
                "stable local separation resolves exact enemy stacks");
        }
    }
    for (HordeEnemy& enemy : match.enemies()) {
        enemy.enemy.health = 0;
    }
    const HordeMatchStepResult completed = updateHordeMatch(
        match, session, PlayerInput {}, fixedStep);
    valid &= check(completed.roundCompleted
            && match.phase() == RoundPhase::Intermission,
        "cleanup reaches intermission only after all scheduled enemies die");
    return valid;
}

bool enemyNavigationUsesDoorState(
    const GeneratedLevel& level, LevelSession& session, HordeMatch& match)
{
    const std::size_t anchorGate = gateIndex(match, GatePurpose::Anchor);
    if (anchorGate >= match.plan().gates.size()) {
        return check(false, "recipe provides an Anchor gate");
    }
    const MapGate& gate = match.plan().gates[anchorGate];
    const DoorwayThreshold& threshold
        = level.doorwayThresholds()[gate.doorway];
    bool valid = check(!session.canTraverse(
                           threshold.firstCell, threshold.secondCell),
        "closed gate blocks the shared navigation contract");
    match.grantPoints(gate.cost);
    valid &= check(match.purchaseGate(session, anchorGate),
        "Anchor route can be purchased with earned points");
    valid &= check(session.canTraverse(
                       threshold.firstCell, threshold.secondCell),
        "opening the Anchor gate updates navigation immediately");
    const auto next = nextHordeNavigationCell(level, session,
        threshold.firstCell, threshold.secondCell);
    valid &= check(next.has_value() && *next == threshold.secondCell,
        "horde pathfinding crosses an opened exact threshold");
    return valid;
}

bool anchorHoldoutRunsAlongsideAutomaticDirector(
    const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    session.player().position = Vector3 {
        match.plan().anchorPosition.x, PLAYER_RADIUS,
        match.plan().anchorPosition.y
    };
    session.player().invulnerabilityRemaining = 100.0F;
    match.currentRound = 3;
    match.roundPhase = RoundPhase::Cleanup;
    match.anchorActive = true;
    constexpr float fixedStep = 1.0F / 120.0F;
    const HordeMatchStepResult completed = updateHordeMatch(
        match, session, PlayerInput {}, fixedStep);
    bool valid = check(completed.roundCompleted
            && match.phase() == RoundPhase::Intermission
            && !match.anchorIsComplete(),
        "an incomplete holdout cannot keep cleanup open");
    valid &= check(advanceUntilRoundStarts(
                       match, session, HORDE_INTERMISSION_DURATION + 0.1F)
            && match.round() == 4,
        "the director advances while the holdout remains incomplete");
    for (int tick = 0; tick < 970; ++tick) {
        updateHordeMatch(match, session, PlayerInput {}, fixedStep);
    }
    valid &= check(match.anchorIsComplete(),
        "fixed-step holdout progress completes during concurrent rounds");
    return valid;
}

bool anchorHubRelayProgressesWithoutOwningRounds(
    LevelSession& session, HordeMatch& match)
{
    match.currentRound = 2;
    match.roundPhase = RoundPhase::Buildup;
    bool valid = check(match.activateAnchor() && match.anchorIsActive(),
        "eligible Anchor interaction begins during an active round");
    valid &= check(match.advanceAnchor(ANCHOR_HOLDOUT_DURATION * 0.5F)
            && !match.anchorIsComplete(),
        "partial holdout time persists without completing early");
    const int pointsBeforeCompletion = match.points();
    valid &= check(match.advanceAnchor(ANCHOR_HOLDOUT_DURATION)
            && match.anchorIsComplete()
            && match.points() == pointsBeforeCompletion + 250,
        "completed holdout powers the Anchor and awards points once");
    match.roundPhase = RoundPhase::Cleanup;
    valid &= check(match.activateHub(session) && match.hubIsPowered(),
        "powered Anchor makes the Hub activatable during combat cleanup");

    const std::size_t exitGate = gateIndex(match, GatePurpose::Exit);
    if (exitGate < match.plan().gates.size()) {
        valid &= check(match.plan().gates[exitGate].open
                && !session.doorwayIsLocked(
                    match.plan().gates[exitGate].doorway),
            "Hub activation opens the objective-locked Exit route");
    }

    const std::size_t rewardGate = gateIndex(match, GatePurpose::Reward);
    if (rewardGate >= match.plan().gates.size()) {
        return check(false, "recipe provides a Reward gate");
    }
    match.grantPoints(match.plan().gates[rewardGate].cost);
    valid &= check(match.purchaseGate(session, rewardGate),
        "optional Reward branch uses the same point economy");
    if (match.plan().relayTargets.size() != 3) {
        return check(false, "relay puzzle contains three targets");
    }
    valid &= check(!match.hitRelay(1) && match.relayProgress() == 0,
        "wrong relay target resets sequence progress");
    valid &= check(match.hitRelay(0) && match.hitRelay(1)
            && match.hitRelay(2) && match.relayIsComplete()
            && match.hasUpgrade(MatchUpgrade::FireRate),
        "correct relay sequence grants its permanent fire-rate reward");
    return valid;
}

bool upgradesChangeAuthoritativeSimulation(
    LevelSession& session, HordeMatch& match)
{
    session.reset();
    match.reset(session);
    for (const GatePurpose purpose
        : { GatePurpose::Expansion, GatePurpose::Anchor }) {
        const std::size_t gate = gateIndex(match, purpose);
        if (gate < match.plan().gates.size()) {
            match.grantPoints(match.plan().gates[gate].cost);
            match.purchaseGate(session, gate);
        }
    }
    match.grantPoints(match.nextUpgradeCost(MatchUpgrade::Damage) - 1);
    bool valid = check(!match.purchaseUpgrade(
                           session, MatchUpgrade::Damage)
            && match.points() == 1199
            && match.upgradeLevel(MatchUpgrade::Damage) == 0,
        "an unaffordable upgrade tier changes neither points nor level");
    match.grantPoints(30000 - match.points());
    valid &= check(match.purchaseUpgrade(session, MatchUpgrade::Damage)
            && match.purchaseUpgrade(session, MatchUpgrade::Damage)
            && match.purchaseUpgrade(session, MatchUpgrade::Damage)
            && match.weaponDamage() == 4
            && match.points() == 22200
            && match.upgradeLevel(MatchUpgrade::Damage)
                == HORDE_MAX_UPGRADE_LEVEL,
        "damage tiers deduct 1,200/2,400/4,200 and reach four damage");
    valid &= check(match.purchaseUpgrade(session, MatchUpgrade::FireRate)
            && match.purchaseUpgrade(session, MatchUpgrade::FireRate)
            && match.purchaseUpgrade(session, MatchUpgrade::FireRate)
            && std::abs(match.playerAttack.weapon.fireInterval - 0.05F)
                < 0.001F
            && match.points() == 15200,
        "fire-rate tiers deduct escalating costs and reach 0.05 seconds");
    valid &= check(match.purchaseUpgrade(session, MatchUpgrade::Dash)
            && match.purchaseUpgrade(session, MatchUpgrade::Dash)
            && match.purchaseUpgrade(session, MatchUpgrade::Dash)
            && std::abs(session.player().dashCooldownScale - 0.45F)
                < 0.001F
            && match.points() == 9500,
        "dash tiers deduct escalating costs and reach 0.45x cooldown");
    valid &= check(!match.purchaseUpgrade(session, MatchUpgrade::Damage)
            && !match.purchaseUpgrade(session, MatchUpgrade::FireRate)
            && !match.purchaseUpgrade(session, MatchUpgrade::Dash)
            && match.nextUpgradeCost(MatchUpgrade::Damage) == 0,
        "all upgrade lines stop cleanly at the pressure-profile cap");

    match.anchorComplete = true;
    valid &= check(match.activateHub(session),
        "the powered Hub enables its repeatable repair sink");
    session.player().health = 3;
    valid &= check(match.purchaseHubRepair(session)
            && match.purchaseHubRepair(session)
            && !match.purchaseHubRepair(session)
            && session.player().health == PLAYER_MAX_HEALTH
            && match.points() == 8500,
        "Hub repair repeatedly spends scaled currency until health is full");
    return valid;
}

bool exitRequiresPoweredCompletedMatch(const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    session.player().position = Vector3 {
        match.plan().exitPosition.x, PLAYER_RADIUS,
        match.plan().exitPosition.y
    };
    match.currentRound = HORDE_EXTRACTION_MINIMUM_ROUND;
    PlayerInput interact;
    interact.interactPressed = true;
    HordeMatchStepResult result = updateHordeMatch(
        match, session, interact, 1.0F / 120.0F);
    bool valid = check(!result.victory && !match.matchIsComplete(),
        "Exit interaction cannot bypass the required objective");

    match.anchorComplete = true;
    match.roundPhase = RoundPhase::Peak;
    valid &= check(match.activateHub(session),
        "test progression can power Hub during an active endless round");
    result = updateHordeMatch(
        match, session, interact, 1.0F / 120.0F);
    valid &= check(result.victory && match.matchIsComplete(),
        "powered Exit interaction after Round 5 explicitly extracts");
    return valid;
}

bool resetRestoresWholeMatch(LevelSession& session, HordeMatch& match)
{
    match.grantPoints(std::numeric_limits<int>::max());
    match.grantPoints(1);
    bool valid = check(match.points() == std::numeric_limits<int>::max(),
        "endless point income saturates without signed overflow");
    match.enemyEntries.push_back(HordeEnemy {});
    match.pendingSpawns.push_back(HordeEnemyRole::Elite);
    match.playerAttack.projectiles.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F });
    match.hostileProjectiles.spawn(Vector2 {}, Vector2 { 1.0F, 0.0F });
    match.anchorActive = true;
    match.anchorProgressSeconds = 4.0F;
    match.relaySequenceProgress = 2;
    match.victory = true;
    session.reset();
    match.reset(session);
    valid &= check(match.points() == 0 && match.round() == 0
            && match.phase() == RoundPhase::Intermission
            && std::abs(match.timeUntilNextRound()
                - HORDE_INITIAL_COUNTDOWN_DURATION) < 0.001F
            && match.difficulty().spawnBudget == 6
            && !match.anchorIsComplete() && !match.hubIsPowered()
            && !match.relayIsComplete() && match.weaponDamage() == 1
            && match.enemies().empty() && match.pendingSpawns.empty()
            && match.playerProjectiles().activeCount() == 0
            && match.enemyProjectiles().activeCount() == 0
            && session.player().dashCooldownScale == 1.0F,
        "reset restores economy, rounds, puzzle, actors, projectiles, and upgrades");
    for (const MapGate& gate : match.plan().gates) {
        valid &= check(!gate.open && session.doorwayIsLocked(gate.doorway),
            "reset restores every recipe gate");
    }
    return valid;
}

} // namespace

int main()
{
    const GeneratedLevel level;
    LevelSession session(level);
    HordeMatch match(level, session);
    bool valid = true;
    valid &= everyRecipeCompilesPlayableMatch();
    valid &= recipeCompilesPuzzleSitesAndGates(level, match, session);
    valid &= hordeDamageAwardsPointsOnce(level);
    valid &= optionalSpendingCannotConsumeRequiredProgression(level);
    valid &= anchorInteractionFundsAndStartsHoldout(level);
    valid &= pointsPurchaseGatesAtomically(level, session, match);
    valid &= fortressGateInteractionMatchesTheHudRange();
    valid &= automaticDirectorStartsWithoutInputOrPuzzleState(level);
    valid &= deterministicDifficultyScalesAndStaysBounded();
    valid &= difficultyProfileAppliesToSpawnedEnemies(level);
    valid &= spawnedHordeRespectsGeometryAndPopulation(level);
    valid &= enemyNavigationUsesDoorState(level, session, match);
    valid &= anchorHoldoutRunsAlongsideAutomaticDirector(level);
    valid &= anchorHubRelayProgressesWithoutOwningRounds(session, match);
    valid &= upgradesChangeAuthoritativeSimulation(session, match);
    valid &= exitRequiresPoweredCompletedMatch(level);
    valid &= resetRestoresWholeMatch(session, match);
    return valid ? 0 : 1;
}
