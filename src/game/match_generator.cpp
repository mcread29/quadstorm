#include "match_generator.hpp"

#include "horde_match.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <ranges>

namespace {

constexpr std::uint64_t GRID_SEED_DOMAIN = 0x243f6a8885a308d3ULL;
constexpr std::uint64_t ROOM_SEED_DOMAIN = 0x13198a2e03707344ULL;
constexpr std::uint64_t ATTEMPT_STEP = 0x9e3779b97f4a7c15ULL;

std::uint64_t mixSeed(std::uint64_t value)
{
    value += ATTEMPT_STEP;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint32_t nonzeroSeed(std::uint64_t value)
{
    const std::uint32_t seed = static_cast<std::uint32_t>(value);
    return seed == 0 ? 1U : seed;
}

bool hasGate(const SmallMapPlan& plan, GatePurpose purpose)
{
    return std::ranges::any_of(plan.gates, [purpose](const MapGate& gate) {
        return gate.purpose == purpose;
    });
}

bool isCurrentSystemsMatchValid(const GeneratedLevel& level)
{
    if (level.roomLayout().getRoomCount() == 0
        || !std::isfinite(level.roomLayout().getQualityScore())
        || level.roomLayout().getQualityScore() <= 0.0F) {
        return false;
    }

    const SmallMapPlan plan = buildSmallMapPlan(level);
    const auto validRoom = [&](int room) {
        return room >= 0
            && static_cast<std::size_t>(room)
                < level.roomLayout().getRoomCount();
    };
    return validRoom(plan.startRoom) && validRoom(plan.hubRoom)
        && validRoom(plan.anchorRoom) && validRoom(plan.rewardRoom)
        && validRoom(plan.exitRoom) && plan.relayTargets.size() == 3
        && hasGate(plan, GatePurpose::Expansion)
        && hasGate(plan, GatePurpose::Anchor)
        && hasGate(plan, GatePurpose::Reward)
        && hasGate(plan, GatePurpose::Exit);
}

} // namespace

GeneratedLevelConfig deriveMatchLevelConfig(
    const MatchGenerationRequest& request, std::size_t attempt)
{
    const std::uint64_t attemptSeed = request.matchSeed
        + static_cast<std::uint64_t>(attempt) * ATTEMPT_STEP;
    return GeneratedLevelConfig {
        request.gridRadius,
        nonzeroSeed(mixSeed(attemptSeed ^ GRID_SEED_DOMAIN)),
        nonzeroSeed(mixSeed(attemptSeed ^ ROOM_SEED_DOMAIN)),
        request.worldScale
    };
}

std::unique_ptr<GeneratedLevel> generateMatchLevel(
    const MatchGenerationRequest& request)
{
    const bool requestIsValid = request.gridRadius >= 2
        && std::isfinite(request.worldScale) && request.worldScale > 0.0F;
    std::size_t attemptsPerformed = 0;
    if (requestIsValid) {
        for (std::size_t attempt = 0; attempt < request.attemptBudget;
             ++attempt) {
            ++attemptsPerformed;
            try {
                auto level = std::make_unique<GeneratedLevel>(
                    deriveMatchLevelConfig(request, attempt),
                    MatchGenerationInfo {
                        request.matchSeed, attempt + 1, false
                    });
                if (isCurrentSystemsMatchValid(*level)) {
                    return level;
                }
            } catch (const std::exception&) {
                // A rejected candidate advances through the deterministic stream.
            }
        }
    }

    return std::make_unique<GeneratedLevel>(
        REPRESENTATIVE_LEVEL_CONFIGS.front(),
        MatchGenerationInfo {
            request.matchSeed, attemptsPerformed, true
        });
}
