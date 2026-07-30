#include "game_renderer.hpp"

#include "generated_level_queries.hpp"
#include "match_generator.hpp"
#include "render_style.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <string>

using namespace game_render;

namespace {

constexpr int UI_WIDTH = static_cast<int>(UI_CANVAS_WIDTH);
constexpr int UI_HEIGHT = static_cast<int>(UI_CANVAS_HEIGHT);

std::string upgradeHudEntry(const HordeMatch& match,
    MatchUpgrade upgrade, const char* label)
{
    const std::uint8_t level = match.upgradeLevel(upgrade);
    const int cost = match.nextUpgradeCost(upgrade);
    return std::string(label) + " L" + std::to_string(level)
        + (cost == 0 ? " MAX" : " " + std::to_string(cost));
}

} // namespace

void GameRenderer::drawGeneratedHud(const Player& player,
    const GeneratedLevel& level, const LevelSession& session,
    const HordeMatch& match, bool showDebug) const
{
    beginUiCanvas();
    const auto currentCell = level.cellAtWorldPoint(
        Vector2 { player.position.x, player.position.z });
    const int currentRegion = currentCell.has_value()
        ? level.roomLayout().getCellAssignment(*currentCell)
        : stalberg::rooms::EMPTY_CELL;
    const auto* currentRoom
        = generated_level::findRoomById(level, currentRegion);

    drawPlayerHud(player);
    drawHudPanel(Rectangle { 292.0F, 16.0F, 350.0F, 64.0F },
        MACHINE_GOLD);
    drawText("CREDITS", 309, 25, 13, Color { 132, 165, 164, 255 });
    drawText(TextFormat("%05i", match.points()), 382, 22, 23,
        MACHINE_GOLD);
    const std::string roundLabel
        = "ROUND " + std::to_string(match.round());
    drawText(roundLabel.c_str(), 309, 52, 14,
        Color { 204, 220, 215, 255 });
    if (match.phase() == RoundPhase::Intermission) {
        drawText(TextFormat("NEXT %.1f", match.timeUntilNextRound()),
            520, 25, 14, Color { 132, 165, 164, 255 });
    } else {
        drawText(roundPhaseName(match.phase()), 520, 25, 14,
            Color { 132, 165, 164, 255 });
    }
    drawText(TextFormat("HOSTILES %02i",
                 static_cast<int>(std::ranges::count_if(match.enemies(),
                     [](const HordeEnemy& enemy) {
                         return isEnemyAlive(enemy.enemy);
                     }))),
        529, 52, 14, Color { 226, 123, 78, 255 });

    if (currentRoom != nullptr) {
        const char* label = TextFormat("%s  %02i",
            roomRoleName(currentRoom->role), currentRoom->id + 1);
        const int width = measureText(label, 18) + 30;
        const Rectangle roomPanel {
            static_cast<float>(UI_WIDTH - width - 18),
            18.0F, static_cast<float>(width), 38.0F
        };
        drawHudPanel(roomPanel, roomRoleColor(currentRoom->role));
        drawText(label, UI_WIDTH - width - 1, 28, 18,
            roomRoleColor(currentRoom->role));
    }
    if (level.matchGeneration().has_value()) {
        const MatchGenerationInfo& generation = *level.matchGeneration();
        const std::string seedLabel
            = std::string(physicalMapProfileName(generation.physicalProfile))
            + "  SEED " + std::to_string(generation.matchSeed)
            + (generation.usedFallback ? "  FALLBACK" : "");
        const int width = measureText(seedLabel.c_str(), 13) + 22;
        const Rectangle seedPanel {
            static_cast<float>(UI_WIDTH - width - 18),
            62.0F, static_cast<float>(width), 30.0F
        };
        drawHudPanel(seedPanel, generation.usedFallback
                ? Color { 234, 105, 80, 255 }
                : Color { 151, 193, 190, 255 });
        drawText(seedLabel.c_str(), UI_WIDTH - width - 7, 70, 13,
            generation.usedFallback ? Color { 255, 174, 92, 255 }
                                    : Color { 151, 193, 190, 255 });
    }

    const Vector2 playerMapPosition {
        player.position.x, player.position.z
    };
    const auto anchorGate = std::ranges::find(
        match.plan().gates, GatePurpose::Anchor, &MapGate::purpose);
    const bool anchorRouteOpen = anchorGate == match.plan().gates.end()
        || anchorGate->open;
    const char* interactionPrompt = nullptr;
    std::string interactionPromptStorage;
    float nearestGate = HORDE_GATE_INTERACTION_DISTANCE;
    const auto thresholds = level.doorwayThresholds();
    for (const MapGate& gate : match.plan().gates) {
        if (gate.open || gate.doorway >= thresholds.size()) {
            continue;
        }
        const Vector2 closest = closestPointOnSegment(
            playerMapPosition, thresholds[gate.doorway].segment).position;
        const float x = closest.x - playerMapPosition.x;
        const float y = closest.y - playerMapPosition.y;
        const float distance = std::sqrt(x * x + y * y);
        if (distance > nearestGate) {
            continue;
        }
        nearestGate = distance;
        if (gate.purpose == GatePurpose::Exit) {
            interactionPrompt = "EXIT SEALED  -  POWER THE HUB";
        } else if (gate.purpose == GatePurpose::Reward
            && !anchorRouteOpen) {
            interactionPrompt = "ANCHOR ROUTE REQUIRED BEFORE OPTIONAL SPENDING";
        } else if (match.points() >= gate.cost) {
            interactionPrompt = TextFormat("E  OPEN GATE  %i POINTS", gate.cost);
        } else {
            interactionPrompt = TextFormat("GATE %i  -  NEED %i MORE",
                gate.cost, gate.cost - match.points());
        }
    }
    const auto nearDevice = [&](Vector2 position) {
        const float x = position.x - playerMapPosition.x;
        const float y = position.y - playerMapPosition.y;
        return x * x + y * y
            <= HORDE_DEVICE_INTERACTION_DISTANCE
                * HORDE_DEVICE_INTERACTION_DISTANCE;
    };
    if (interactionPrompt == nullptr
        && nearDevice(match.plan().anchorPosition)
        && !match.anchorIsComplete()) {
        if (match.round() < 2) {
            interactionPrompt = "ANCHOR DORMANT  -  SURVIVE TWO ROUNDS";
        } else if (!anchorRouteOpen && anchorGate != match.plan().gates.end()) {
            interactionPrompt = match.points() >= anchorGate->cost
                ? TextFormat("E  FUND + START HOLDOUT  %i  -  STAY IN RING",
                    anchorGate->cost)
                : TextFormat("ANCHOR ROUTE NEEDS %i MORE POINTS",
                    anchorGate->cost - match.points());
        } else {
            interactionPrompt = "E  START HOLDOUT  -  STAY INSIDE THE GOLD RING";
        }
    }
    if (interactionPrompt == nullptr
        && nearDevice(match.plan().hubPosition)) {
        if (match.anchorIsComplete() && !match.hubIsPowered()) {
            interactionPrompt = "E  POWER THE HUB";
        } else if (!anchorRouteOpen) {
            interactionPrompt = "OPEN THE ANCHOR ROUTE TO ENABLE UPGRADES";
        } else {
            interactionPromptStorage = match.hubIsPowered()
                    && player.health < PLAYER_MAX_HEALTH
                ? "E REPAIR 1 HEALTH "
                    + std::to_string(match.hubRepairCost()) + " | "
                : "";
            interactionPromptStorage += "UPGRADES  1 "
                + upgradeHudEntry(match, MatchUpgrade::Damage, "DMG")
                + " | 2 "
                + upgradeHudEntry(match, MatchUpgrade::FireRate, "FIRE")
                + " | 3 "
                + upgradeHudEntry(match, MatchUpgrade::Dash, "DASH");
            interactionPrompt = interactionPromptStorage.c_str();
        }
    }
    if (interactionPrompt == nullptr
        && nearDevice(match.plan().exitPosition)) {
        interactionPrompt = match.hubIsPowered()
                && match.round() >= HORDE_EXTRACTION_MINIMUM_ROUND
            ? "E  EXTRACT FROM THE ENDLESS MATCH"
            : "EXIT DORMANT";
    }
    const auto rewardGate = std::ranges::find(
        match.plan().gates, GatePurpose::Reward, &MapGate::purpose);
    const bool rewardRouteOpen = rewardGate == match.plan().gates.end()
        || rewardGate->open;
    const bool nearRelay = std::ranges::any_of(
        match.plan().relayTargets, [&](const RelayTarget& relay) {
            return nearDevice(relay.position);
        });
    if (interactionPrompt == nullptr && nearRelay && match.hubIsPowered()
        && rewardRouteOpen && !match.relayIsComplete()) {
        interactionPrompt
            = "SHOOT THE GOLD RELAY  -  A WRONG TARGET RESETS THE SEQUENCE";
    }
    if (interactionPrompt != nullptr) {
        const int promptWidth = measureText(interactionPrompt, 18) + 30;
        const Rectangle promptPanel {
            static_cast<float>(UI_WIDTH / 2 - promptWidth / 2),
            static_cast<float>(UI_HEIGHT - 112),
            static_cast<float>(promptWidth), 42.0F
        };
        drawHudPanel(promptPanel, MACHINE_GOLD);
        drawText(interactionPrompt,
            UI_WIDTH / 2 - promptWidth / 2 + 16,
            UI_HEIGHT - 100, 18, MACHINE_GOLD);
    }

    const char* objective = match.round() == 0
        ? "Round 1 deploys automatically when the countdown ends"
        : "Survive the automatically advancing rounds";
    if (match.anchorIsActive()) {
        objective = TextFormat(
            "STAY IN THE GOLD RING  %.1f / %.1f  -  LEAVING PAUSES",
            match.anchorProgress(), ANCHOR_HOLDOUT_DURATION);
    } else if (!match.anchorIsComplete() && match.round() >= 2) {
        objective
            = "At the Anchor: press E, then stay in its gold ring during the wave";
    } else if (match.anchorIsComplete() && !match.hubIsPowered()) {
        objective = "Return to the Hub and press E to power the Exit";
    } else if (match.hubIsPowered()
        && match.round() < HORDE_EXTRACTION_MINIMUM_ROUND) {
        objective = "Keep surviving; extraction unlocks after Round 5";
    } else if (match.hubIsPowered() && !match.matchIsComplete()) {
        objective = "Extract at the Exit or continue the endless rounds";
    }
    const Rectangle objectivePanel {
        16.0F, static_cast<float>(UI_HEIGHT - 60),
        700.0F, 42.0F
    };
    drawHudPanel(objectivePanel, Color { 187, 145, 57, 255 });
    drawText("DIRECTIVE", 31, UI_HEIGHT - 49, 12,
        Color { 132, 165, 164, 255 });
    drawText(objective, 115, UI_HEIGHT - 50, 16,
        Color { 224, 211, 158, 255 });

    if (showDebug) {
        DrawRectangleRounded(Rectangle { 16.0F, 90.0F, 720.0F, 142.0F },
            0.08F, 6, Color { 7, 17, 24, 225 });
        drawText("F3 HIDE DEBUG", 28, 101, 17,
            Color { 151, 193, 190, 255 });
        drawText("WASD | SPACE dash | LMB fire | E interact | 1/2/3 upgrades",
            28, 127, 15, Color { 180, 203, 200, 255 });
        drawText("R restart | N new match | F1 arena | F2 overview / fixtures",
            28, 150, 15, Color { 180, 203, 200, 255 });
        const char* topology = level.roomLayout().hasSmallMapRecipe()
            ? smallMapRecipeName(level.roomLayout().getSmallMapRecipe())
            : largeMapArchetypeName(
                level.roomLayout().getLargeMapArchetype());
        drawText(TextFormat("%s  rooms %i  doors %i  walls %i  r%i  s%.2f",
                     topology,
                     static_cast<int>(level.roomLayout().getRoomCount()),
                     static_cast<int>(level.roomLayout().getDoorways().size()),
                     static_cast<int>(session.activeWalls().size()),
                     level.config().gridRadius, level.worldScale()),
            28, 177, 15, Color { 224, 211, 158, 255 });
        drawText(TextFormat("tier %u budget %i  anchor %.1f  relay %i/%i  damage %i",
                     match.difficulty().pressureTier,
                     static_cast<int>(match.difficulty().spawnBudget),
                     match.anchorProgress(),
                     static_cast<int>(match.relayProgress()),
                     static_cast<int>(match.plan().relayTargets.size()),
                     match.weaponDamage()),
            28, 201, 15, Color { 180, 203, 200, 255 });
        drawText(TextFormat("%i FPS", GetFPS()), UI_WIDTH - 91, 70, 14,
            Color { 151, 193, 190, 255 });
    }

    const char* statusMessage = nullptr;
    Color statusColor { 255, 174, 92, 255 };
    if (!isPlayerAlive(player)) {
        statusMessage = "DEFEATED  -  press R to restart";
    } else if (match.matchIsComplete()) {
        statusMessage = "MAP COMPLETE  -  press R to restart";
        statusColor = Color { 151, 231, 190, 255 };
    }
    if (statusMessage != nullptr) {
        constexpr int fontSize = 30;
        const int messageWidth = static_cast<int>(
            measureText(statusMessage, fontSize));
        DrawRectangleRounded(Rectangle {
                                 static_cast<float>(UI_WIDTH / 2
                                     - messageWidth / 2 - 24),
                                 static_cast<float>(UI_HEIGHT / 2 - 34),
                                 static_cast<float>(messageWidth + 48), 68.0F },
            0.2F, 8, Color { 7, 17, 24, 235 });
        drawText(statusMessage, UI_WIDTH / 2 - messageWidth / 2,
            UI_HEIGHT / 2 - fontSize / 2, fontSize, statusColor);
    }

    endUiCanvas();
}

void GameRenderer::drawCombatHud(const Player& player,
    const ProjectilePool& playerProjectiles, const Target& target,
    const Enemy& enemy, const ProjectilePool& enemyProjectiles,
    bool showDebug) const
{
    beginUiCanvas();
    drawPlayerHud(player);
    const Rectangle trainingPanel {
        static_cast<float>(UI_WIDTH - 238),
        18.0F, 220.0F, 38.0F
    };
    drawHudPanel(trainingPanel, Color { 180, 112, 220, 255 });
    drawText("COMBAT SIMULATION", UI_WIDTH - 218, 28, 18,
        Color { 201, 160, 229, 255 });

    if (showDebug) {
        DrawRectangleRounded(Rectangle { 16.0F, 82.0F, 570.0F, 118.0F },
            0.08F, 6, Color { 7, 17, 24, 225 });
        drawText("F3  HIDE DEBUG", 28, 94, 18,
            Color { 151, 193, 190, 255 });
        drawText("WASD move | SPACE dash | LMB fire | F1 level | F2 overview",
            28, 123, 16, Color { 180, 203, 200, 255 });
        drawText(TextFormat("target %i/%i  player shots %i  hostile shots %i",
                     target.health, TARGET_MAX_HEALTH,
                     static_cast<int>(playerProjectiles.activeCount()),
                     static_cast<int>(enemyProjectiles.activeCount())),
            28, 150, 16, Color { 224, 211, 158, 255 });
        drawText(isEnemyAlive(enemy) ? "enemy active" : "enemy defeated",
            28, 177, 16, Color { 180, 162, 220, 255 });
        drawText(TextFormat("%i FPS", GetFPS()), UI_WIDTH - 91, 70, 14,
            Color { 151, 193, 190, 255 });
    }

    const char* encounterMessage = nullptr;
    Color encounterMessageColor { 255, 174, 92, 255 };
    if (!isPlayerAlive(player)) {
        encounterMessage = "DEFEATED  -  press R to restart";
    } else if (!isEnemyAlive(enemy)) {
        encounterMessage = "ENEMY DEFEATED  -  press R to restart";
        encounterMessageColor = Color { 151, 231, 190, 255 };
    }
    if (encounterMessage != nullptr) {
        const int fontSize = 30;
        const int messageWidth = measureText(encounterMessage, fontSize);
        DrawRectangleRounded(Rectangle {
                                 static_cast<float>(UI_WIDTH / 2
                                     - messageWidth / 2 - 24),
                                 static_cast<float>(UI_HEIGHT / 2 - 34),
                                 static_cast<float>(messageWidth + 48), 68.0F },
            0.2F, 8, Color { 7, 17, 24, 235 });
        drawText(encounterMessage, UI_WIDTH / 2 - messageWidth / 2,
            UI_HEIGHT / 2 - fontSize / 2, fontSize,
            encounterMessageColor);
    }

    endUiCanvas();
}

void GameRenderer::drawPlayerHud(const Player& player) const
{
    constexpr float panelWidth = 260.0F;
    constexpr float meterX = 101.0F;
    constexpr float meterWidth = 156.0F;
    constexpr float segmentGap = 4.0F;
    const float dashAmount = 1.0F - std::clamp(
        player.dashCooldownRemaining / PLAYER_DASH_COOLDOWN, 0.0F, 1.0F);
    const Color healthColor = player.health <= 1
        ? Color { 241, 78, 55, 255 }
        : MACHINE_GOLD;

    drawHudPanel(Rectangle { 16.0F, 16.0F, panelWidth, 64.0F },
        healthColor);
    drawText("VITAL", 30, 25, 14, Color { 132, 165, 164, 255 });
    const float segmentWidth = (meterWidth
        - segmentGap * static_cast<float>(PLAYER_MAX_HEALTH - 1))
        / static_cast<float>(PLAYER_MAX_HEALTH);
    for (int segment = 0; segment < PLAYER_MAX_HEALTH; ++segment) {
        const float x = meterX
            + static_cast<float>(segment) * (segmentWidth + segmentGap);
        DrawRectangleRounded(Rectangle { x, 27.0F, segmentWidth, 14.0F },
            0.22F, 5,
            segment < player.health ? healthColor
                                    : Color { 25, 38, 43, 255 });
    }

    drawText("BOOST", 30, 52, 13, Color { 132, 165, 164, 255 });
    DrawRectangleRounded(Rectangle { meterX, 54.0F, meterWidth, 10.0F },
        0.45F, 7, Color { 25, 38, 43, 255 });
    if (dashAmount > 0.0F) {
        DrawRectangleRounded(Rectangle {
                                 meterX, 54.0F,
                                 meterWidth * dashAmount, 10.0F },
            0.45F, 7, ENERGY_CYAN);
    }
}
