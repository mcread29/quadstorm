#include "arena.hpp"
#include "combat_audio.hpp"
#include "encounter.hpp"
#include "game_camera.hpp"
#include "game_input.hpp"
#include "generated_level.hpp"
#include "horde_match.hpp"
#include "level_session.hpp"
#include "match_generator.hpp"
#include "game_renderer.hpp"

#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float FIXED_STEP_TIME = 1.0F / 120.0F;
constexpr float MAX_FRAME_TIME = 0.05F;

std::uint64_t freshMatchSeed()
{
    static std::uint64_t state = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    state += 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::optional<std::uint64_t> parseMatchSeed(std::string_view argument)
{
    constexpr std::string_view prefix = "--seed=";
    if (!argument.starts_with(prefix)) {
        return std::nullopt;
    }
    std::uint64_t seed = 0;
    const std::string_view digits = argument.substr(prefix.size());
    const auto result = std::from_chars(
        digits.data(), digits.data() + digits.size(), seed);
    if (result.ec != std::errc {} || result.ptr != digits.data() + digits.size()) {
        throw std::invalid_argument("--seed expects an unsigned decimal integer");
    }
    return seed;
}

class GameApplication {
public:
    explicit GameApplication(std::unique_ptr<GeneratedLevel> initialLevel)
        : previousCombatPlayer(encounter.player)
    {
        installGeneratedLevel(std::move(initialLevel));
    }

    void frame()
    {
        if (IsKeyPressed(KEY_N)) {
            installGeneratedLevel(generateMatchLevel(
                MatchGenerationRequest { freshMatchSeed() }));
            combatArenaActive = false;
            overviewActive = false;
            overviewConfiguration = 0;
            overviewPreview.reset();
            clearQueuedInputs();
            accumulatedTime = 0.0F;
        }
        if (IsKeyPressed(KEY_F2)) {
            overviewActive = !overviewActive;
            clearQueuedInputs();
            accumulatedTime = 0.0F;
        }
        if (overviewActive) {
            if (IsKeyPressed(KEY_HOME)) {
                selectOverviewConfiguration(0);
            } else if (IsKeyPressed(KEY_LEFT)) {
                const std::size_t previous = overviewConfiguration == 0
                    ? REPRESENTATIVE_LEVEL_CONFIGS.size()
                    : overviewConfiguration - 1;
                selectOverviewConfiguration(previous);
            } else if (IsKeyPressed(KEY_RIGHT)) {
                selectOverviewConfiguration((overviewConfiguration + 1)
                    % (REPRESENTATIVE_LEVEL_CONFIGS.size() + 1));
            }
            const GeneratedLevel& overviewLevel = selectedOverviewLevel();
            renderer->drawGeneratedOverview(overviewLevel,
                overviewConfiguration == 0 ? levelSession.get() : nullptr,
                overviewConfiguration == 0 ? hordeMatch.get() : nullptr,
                overviewConfiguration,
                REPRESENTATIVE_LEVEL_CONFIGS.size() + 1);
            return;
        }

        const float frameTime = std::min(GetFrameTime(), MAX_FRAME_TIME);
        accumulatedTime += frameTime;

        PlayerInput input = readPlayerInput(renderCamera);
        if (IsKeyPressed(KEY_F3)) {
            showDebug = !showDebug;
        }
        if (input.toggleViewPressed) {
            combatArenaActive = !combatArenaActive;
            clearQueuedInputs();
            accumulatedTime = 0.0F;
            input.hasAimPoint = false;
            if (combatArenaActive) {
                previousCombatPlayer = encounter.player;
                camera = makeGameCamera(encounter.player);
            } else {
                previousGeneratedPlayer = levelSession->player();
                camera = makeGameCamera(levelSession->player());
            }
            previousCamera = camera;
            renderCamera = camera;
        }

        restartQueued |= input.restartPressed;
        dashQueued |= input.dashPressed;
        interactQueued |= input.interactPressed;
        buyDamageQueued |= input.buyDamagePressed;
        buyFireRateQueued |= input.buyFireRatePressed;
        buyDashQueued |= input.buyDashPressed;
        input.restartPressed = restartQueued;
        input.dashPressed = dashQueued;
        input.interactPressed = interactQueued;
        input.buyDamagePressed = buyDamageQueued;
        input.buyFireRatePressed = buyFireRateQueued;
        input.buyDashPressed = buyDashQueued;
        if (input.hasAimPoint) {
            aimPoint = input.aimPoint;
        }

        while (accumulatedTime >= FIXED_STEP_TIME) {
            previousCamera = camera;
            if (combatArenaActive) {
                updateCombatArena(input);
            } else {
                updateGeneratedLevel(input);
            }
            clearQueuedInputs();
            input.restartPressed = false;
            input.dashPressed = false;
            input.interactPressed = false;
            input.buyDamagePressed = false;
            input.buyFireRatePressed = false;
            input.buyDashPressed = false;
            accumulatedTime -= FIXED_STEP_TIME;
        }

        const float interpolationAmount = accumulatedTime / FIXED_STEP_TIME;
        renderCamera = interpolateGameCamera(
            previousCamera, camera, interpolationAmount);
        if (combatArenaActive) {
            const Player renderPlayer = interpolatePlayer(
                previousCombatPlayer, encounter.player, interpolationAmount);
            renderer->drawCombat(renderCamera, renderPlayer, aimPoint,
                encounter.combat.playerAttack.projectiles, encounter.target,
                encounter.combat.enemy, encounter.combat.enemyProjectiles,
                interpolationAmount, showDebug);
        } else {
            const Player renderPlayer = interpolatePlayer(
                previousGeneratedPlayer, levelSession->player(),
                interpolationAmount);
            renderer->drawGenerated(renderCamera, renderPlayer, aimPoint,
                *level, *levelSession, *hordeMatch, interpolationAmount,
                showDebug);
        }
    }

private:
    std::unique_ptr<GeneratedLevel> level;
    std::unique_ptr<GameRenderer> renderer;
    CombatAudio combatAudio;
    Encounter encounter;
    std::unique_ptr<LevelSession> levelSession;
    std::unique_ptr<HordeMatch> hordeMatch;
    Player previousGeneratedPlayer;
    Player previousCombatPlayer;
    bool combatArenaActive = false;
    bool showDebug = false;
    bool overviewActive = false;
    std::size_t overviewConfiguration = 0;
    std::unique_ptr<GeneratedLevel> overviewPreview;
    Camera3D camera {};
    Camera3D previousCamera {};
    Camera3D renderCamera {};
    Vector3 aimPoint {};
    float accumulatedTime = 0.0F;
    bool restartQueued = false;
    bool dashQueued = false;
    bool interactQueued = false;
    bool buyDamageQueued = false;
    bool buyFireRateQueued = false;
    bool buyDashQueued = false;

    void installGeneratedLevel(std::unique_ptr<GeneratedLevel> newLevel)
    {
        auto newRenderer = std::make_unique<GameRenderer>(*newLevel);
        auto newSession = std::make_unique<LevelSession>(*newLevel);
        auto newMatch = std::make_unique<HordeMatch>(*newLevel, *newSession);

        hordeMatch = std::move(newMatch);
        levelSession = std::move(newSession);
        renderer = std::move(newRenderer);
        level = std::move(newLevel);
        previousGeneratedPlayer = levelSession->player();
        camera = makeGameCamera(levelSession->player());
        previousCamera = camera;
        renderCamera = camera;
        aimPoint = Vector3 {
            levelSession->player().position.x - 3.0F,
            0.0F,
            levelSession->player().position.z - 3.0F
        };
    }

    void clearQueuedInputs()
    {
        restartQueued = false;
        dashQueued = false;
        interactQueued = false;
        buyDamageQueued = false;
        buyFireRateQueued = false;
        buyDashQueued = false;
    }

    void selectOverviewConfiguration(std::size_t configuration)
    {
        overviewConfiguration = configuration;
        if (overviewConfiguration == 0) {
            overviewPreview.reset();
            return;
        }
        overviewPreview = std::make_unique<GeneratedLevel>(
            REPRESENTATIVE_LEVEL_CONFIGS[overviewConfiguration - 1]);
    }

    const GeneratedLevel& selectedOverviewLevel() const
    {
        return overviewPreview != nullptr ? *overviewPreview : *level;
    }

    void updateCombatArena(const PlayerInput& input)
    {
        previousCombatPlayer = encounter.player;
        const EncounterStepResult result = updateEncounter(
            encounter, input, FIXED_STEP_TIME, ARENA_WALLS);
        if (result.restarted) {
            previousCombatPlayer = encounter.player;
            camera = makeGameCamera(encounter.player);
            previousCamera = camera;
            combatAudio.playRestart();
        } else {
            updateGameCamera(camera, encounter.player, FIXED_STEP_TIME);
        }
        if (result.enemyFired) {
            combatAudio.playEnemyShot();
        }
        combatAudio.playPlayerDamage(result.playerDamage);
        combatAudio.playEnemyDamage(result.enemyDamage);
    }

    void updateGeneratedLevel(const PlayerInput& input)
    {
        previousGeneratedPlayer = levelSession->player();
        const HordeMatchStepResult result = updateHordeMatch(
            *hordeMatch, *levelSession, input, FIXED_STEP_TIME);
        if (result.reset) {
            previousGeneratedPlayer = levelSession->player();
            camera = makeGameCamera(levelSession->player());
            previousCamera = camera;
            combatAudio.playRestart();
        } else {
            updateGameCamera(camera, levelSession->player(), FIXED_STEP_TIME);
        }
        if (result.enemyFired) {
            combatAudio.playEnemyShot();
        }
        combatAudio.playPlayerDamage(result.playerDamage);
        combatAudio.playEnemyDamage(result.enemyDamage);
    }
};

#if defined(PLATFORM_WEB)
void runWebFrame(void* context)
{
    auto* application = static_cast<GameApplication*>(context);
    if (WindowShouldClose()) {
        emscripten_cancel_main_loop();
        delete application;
        CloseWindow();
        return;
    }
    application->frame();
}
#endif

} // namespace

int main(int argumentCount, char** arguments)
{
    std::optional<GeneratedLevelConfig> fixtureConfig;
    std::optional<std::uint64_t> requestedSeed;
    try {
        for (int argument = 1; argument < argumentCount; ++argument) {
            const std::string_view value(arguments[argument]);
            if (value == "--recipe=hub") {
                fixtureConfig = REPRESENTATIVE_LEVEL_CONFIGS[0];
            } else if (value == "--recipe=ring") {
                fixtureConfig = REPRESENTATIVE_LEVEL_CONFIGS[1];
            } else if (value == "--recipe=wings") {
                fixtureConfig = REPRESENTATIVE_LEVEL_CONFIGS[2];
            } else if (const auto seed = parseMatchSeed(value);
                       seed.has_value()) {
                requestedSeed = *seed;
            }
        }
        if (fixtureConfig.has_value() && requestedSeed.has_value()) {
            throw std::invalid_argument(
                "--seed cannot be combined with a fixed --recipe fixture");
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Invalid game arguments: %s\n", error.what());
        return 1;
    }

    auto windowFlags = FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
#if !defined(PLATFORM_WEB)
    windowFlags |= FLAG_WINDOW_RESIZABLE;
#endif
    // Web keeps a fixed 16:10 framebuffer and lets CSS scale it uniformly.
    // Raylib's resizable web path instead adopts the browser aspect ratio,
    // which diverges from the letterboxed canvas and offsets mouse input.
    SetConfigFlags(windowFlags);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Stalberg");
#if !defined(PLATFORM_WEB)
    SetWindowMinSize(900, 600);
#endif

    std::unique_ptr<GameApplication> application;
    try {
        std::unique_ptr<GeneratedLevel> initialLevel
            = fixtureConfig.has_value()
            ? std::make_unique<GeneratedLevel>(*fixtureConfig)
            : generateMatchLevel(MatchGenerationRequest {
                requestedSeed.value_or(freshMatchSeed())
            });
        application = std::make_unique<GameApplication>(
            std::move(initialLevel));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unable to initialize game: %s\n", error.what());
        CloseWindow();
        return 1;
    }

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(
        runWebFrame, application.release(), 0, true);
#else
    while (!WindowShouldClose()) {
        application->frame();
    }
    application.reset();
    CloseWindow();
#endif
    return 0;
}
