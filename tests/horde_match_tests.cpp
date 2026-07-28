#include "game/horde_match.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
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
            && result.roundStarted
            && match.plan().gates[anchorGate].open
            && match.anchorIsActive() && match.round() == 3,
        "one E press at the Anchor funds its route and starts the holdout");
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

bool roundDirectorPublishesDeterministicRoles(HordeMatch& match)
{
    const auto first = hordeCompositionForRound(1);
    const auto third = hordeCompositionForRound(3);
    const auto finale = hordeCompositionForRound(HORDE_FINAL_ROUND);
    bool valid = check(first.size() == 6
            && std::ranges::all_of(first, [](HordeEnemyRole role) {
                return role == HordeEnemyRole::Drifter;
            }),
        "Round 1 contains six point-funding Drifters");
    valid &= check(std::ranges::count(third, HordeEnemyRole::Runner) == 2
            && std::ranges::count(third, HordeEnemyRole::Caster) == 1,
        "Round 3 introduces Runner and Caster pressure");
    valid &= check(std::ranges::count(finale, HordeEnemyRole::Elite) == 1,
        "the final round includes one elite threat");
    valid &= check(match.startNextRound()
            && match.round() == 1
            && match.phase() == RoundPhase::Buildup
            && match.pendingSpawns == first,
        "starting a round installs its deterministic spawn schedule");
    return valid;
}

bool spawnedHordeRespectsGeometryAndPopulation(
    const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    session.player().invulnerabilityRemaining = 100.0F;
    bool valid = check(match.startNextRound(),
        "geometry scenario starts Round 1");
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

bool cleanupWaitsForActualAnchorHoldout(const GeneratedLevel& level)
{
    LevelSession session(level);
    HordeMatch match(level, session);
    session.player().position = Vector3 {
        match.plan().anchorPosition.x, PLAYER_RADIUS,
        match.plan().anchorPosition.y
    };
    match.currentRound = 3;
    match.roundPhase = RoundPhase::Cleanup;
    match.anchorActive = true;
    constexpr float fixedStep = 1.0F / 120.0F;
    for (int tick = 0; tick < 360; ++tick) {
        updateHordeMatch(match, session, PlayerInput {}, fixedStep);
    }
    bool valid = check(match.phase() == RoundPhase::Cleanup
            && !match.anchorIsComplete(),
        "objective-wave cleanup cannot end before the holdout completes");
    for (int tick = 0; tick < 620; ++tick) {
        updateHordeMatch(match, session, PlayerInput {}, fixedStep);
    }
    valid &= check(match.anchorIsComplete()
            && match.phase() == RoundPhase::Intermission,
        "fixed-step in-zone holdout completion releases intermission");
    return valid;
}

bool anchorHubRelayAndExitFormPuzzleProgression(
    LevelSession& session, HordeMatch& match)
{
    match.currentRound = 2;
    match.roundPhase = RoundPhase::Intermission;
    bool valid = check(!match.startNextRound(),
        "regular progression pauses after Round 2 until the Anchor starts");
    valid &= check(match.activateAnchor() && match.anchorIsActive(),
        "eligible Anchor interaction begins the holdout");
    valid &= check(match.advanceAnchor(ANCHOR_HOLDOUT_DURATION * 0.5F)
            && !match.anchorIsComplete(),
        "partial holdout time persists without completing early");
    const int pointsBeforeCompletion = match.points();
    valid &= check(match.advanceAnchor(ANCHOR_HOLDOUT_DURATION)
            && match.anchorIsComplete()
            && match.points() == pointsBeforeCompletion + 250,
        "completed holdout powers the Anchor and awards points once");
    match.currentRound = 3;
    valid &= check(!match.startNextRound(),
        "progression pauses after the holdout until Hub activation");
    valid &= check(match.activateHub(session) && match.hubIsPowered(),
        "powered Anchor makes the Hub activatable");
    valid &= check(match.startNextRound() && match.round() == 4,
        "powering Hub releases the remaining round progression");
    match.roundPhase = RoundPhase::Intermission;

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
    match.grantPoints(3100);
    bool valid = check(match.purchaseUpgrade(session, MatchUpgrade::Damage)
            && match.weaponDamage() == 2,
        "damage purchase changes projectile damage resolution");
    valid &= check(match.purchaseUpgrade(session, MatchUpgrade::FireRate)
            && match.playerAttack.weapon.fireInterval < WEAPON_FIRE_INTERVAL,
        "fire-rate purchase changes authoritative weapon cadence");
    valid &= check(match.purchaseUpgrade(session, MatchUpgrade::Dash)
            && session.player().dashCooldownScale < 1.0F,
        "dash purchase changes authoritative cooldown behavior");
    valid &= check(!match.purchaseUpgrade(session, MatchUpgrade::Damage),
        "an upgrade cannot be purchased twice");
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
    match.currentRound = HORDE_FINAL_ROUND;
    PlayerInput interact;
    interact.interactPressed = true;
    HordeMatchStepResult result = updateHordeMatch(
        match, session, interact, 1.0F / 120.0F);
    bool valid = check(!result.victory && !match.matchIsComplete(),
        "Exit interaction cannot bypass the required objective");

    match.anchorComplete = true;
    match.roundPhase = RoundPhase::Intermission;
    valid &= check(match.activateHub(session),
        "test progression can power Hub after Anchor completion");
    result = updateHordeMatch(
        match, session, interact, 1.0F / 120.0F);
    valid &= check(result.victory && match.matchIsComplete(),
        "powered Exit interaction after Round 5 completes the map");
    return valid;
}

bool resetRestoresWholeMatch(LevelSession& session, HordeMatch& match)
{
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
    bool valid = check(match.points() == 0 && match.round() == 0
            && match.phase() == RoundPhase::Intermission
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
    valid &= roundDirectorPublishesDeterministicRoles(match);
    valid &= spawnedHordeRespectsGeometryAndPopulation(level);
    valid &= enemyNavigationUsesDoorState(level, session, match);
    valid &= cleanupWaitsForActualAnchorHoldout(level);
    valid &= anchorHubRelayAndExitFormPuzzleProgression(session, match);
    valid &= upgradesChangeAuthoritativeSimulation(session, match);
    valid &= exitRequiresPoweredCompletedMatch(level);
    valid &= resetRestoresWholeMatch(session, match);
    return valid ? 0 : 1;
}
