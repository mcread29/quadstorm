#include "match_generator.hpp"

#include "horde_match.hpp"
#include "match_map_metrics.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <ranges>

namespace {

constexpr std::uint64_t GRID_SEED_DOMAIN = 0x243f6a8885a308d3ULL;
constexpr std::uint64_t ROOM_SEED_DOMAIN = 0x13198a2e03707344ULL;
constexpr std::uint64_t ATTEMPT_STEP = 0x9e3779b97f4a7c15ULL;

constexpr MatchMapProfile SYSTEMS_FIXTURE_PROFILE {
    .id = PhysicalMapProfile::SystemsFixture,
    .gridRadius = 5,
    .worldScale = 0.16F
};

constexpr MatchMapProfile FORTRESS_V1_PROFILE {
    .id = PhysicalMapProfile::FortressV1,
    .gridRadius = 8,
    .worldScale = 0.22F,
    .minimumDoorwayWidthInPlayerDiameters = 3.5F,
    .minimumSubstantialRoomAreaInPlayerDiameterSquares = 270.0F,
    .minimumAnchorRoomAreaInPlayerDiameterSquares = 300.0F,
    .minimumObjectiveClearanceInPlayerDiameters = 2.25F,
    .minimumAnchorRoomSpanInPlayerDiameters = 18.0F,
    .minimumRouteDistanceInPlayerDiameters = 135.0F,
    .minimumUsableIngressSeparationInPlayerDiameters = 130.0F,
    .minimumUsableEnemySpawnCandidates = 220,
    .minimumUsableEnemySpawnRooms = 12,
    .minimumHubDoorwayDegree = 3
};

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

bool isMatchValid(const GeneratedLevel& level,
    const MatchMapProfile& profile)
{
    if (level.config().gridRadius < profile.gridRadius
        || level.worldScale() < profile.worldScale
        || level.roomLayout().getRoomCount() == 0
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
    if (!validRoom(plan.startRoom) || !validRoom(plan.hubRoom)
        || !validRoom(plan.anchorRoom) || !validRoom(plan.rewardRoom)
        || !validRoom(plan.exitRoom) || plan.relayTargets.size() != 3
        || !hasGate(plan, GatePurpose::Expansion)
        || !hasGate(plan, GatePurpose::Anchor)
        || !hasGate(plan, GatePurpose::Reward)
        || !hasGate(plan, GatePurpose::Exit)) {
        return false;
    }

    const MatchMapMetrics metrics = measureMatchMap(level);
    return metrics.minimumDoorwayWidthInPlayerDiameters()
            >= profile.minimumDoorwayWidthInPlayerDiameters
        && metrics.minimumSubstantialRoomAreaInPlayerDiameterSquares()
            >= profile.minimumSubstantialRoomAreaInPlayerDiameterSquares
        && metrics.anchorRoomAreaInPlayerDiameterSquares()
            >= profile.minimumAnchorRoomAreaInPlayerDiameterSquares
        && metrics.minimumObjectiveClearanceInPlayerDiameters()
            >= profile.minimumObjectiveClearanceInPlayerDiameters
        && metrics.anchorRoomSpanInPlayerDiameters()
            >= profile.minimumAnchorRoomSpanInPlayerDiameters
        && metrics.startToExitRouteDistanceInPlayerDiameters()
            >= profile.minimumRouteDistanceInPlayerDiameters
        && metrics.maximumUsableIngressSeparationInPlayerDiameters()
            >= profile.minimumUsableIngressSeparationInPlayerDiameters
        && metrics.usableEnemySpawnCandidateCount
            >= profile.minimumUsableEnemySpawnCandidates
        && metrics.usableEnemySpawnRoomCount
            >= profile.minimumUsableEnemySpawnRooms
        && metrics.hubDoorwayDegree >= profile.minimumHubDoorwayDegree;
}

} // namespace

const MatchMapProfile& matchMapProfile(PhysicalMapProfile profile)
{
    switch (profile) {
    case PhysicalMapProfile::SystemsFixture:
        return SYSTEMS_FIXTURE_PROFILE;
    case PhysicalMapProfile::FortressV1:
        return FORTRESS_V1_PROFILE;
    }
    return SYSTEMS_FIXTURE_PROFILE;
}

const char* physicalMapProfileName(PhysicalMapProfile profile)
{
    switch (profile) {
    case PhysicalMapProfile::SystemsFixture:
        return "SYSTEMS";
    case PhysicalMapProfile::FortressV1:
        return "FORTRESS V1";
    }
    return "UNKNOWN";
}

bool matchMapMeetsProfile(const GeneratedLevel& level,
    const MatchMapProfile& profile)
{
    return isMatchValid(level, profile);
}

GeneratedLevelConfig deriveMatchLevelConfig(
    const MatchGenerationRequest& request, std::size_t attempt)
{
    const std::uint64_t attemptSeed = request.matchSeed
        + static_cast<std::uint64_t>(attempt) * ATTEMPT_STEP;
    const MatchMapProfile& profile = matchMapProfile(request.physicalProfile);
    return GeneratedLevelConfig {
        profile.gridRadius,
        nonzeroSeed(mixSeed(attemptSeed ^ GRID_SEED_DOMAIN)),
        nonzeroSeed(mixSeed(attemptSeed ^ ROOM_SEED_DOMAIN)),
        profile.worldScale
    };
}

std::unique_ptr<GeneratedLevel> generateMatchLevel(
    const MatchGenerationRequest& request)
{
    const MatchMapProfile& profile = matchMapProfile(
        request.physicalProfile);
    const bool requestIsValid = profile.gridRadius >= 2
        && std::isfinite(profile.worldScale) && profile.worldScale > 0.0F;
    std::size_t attemptsPerformed = 0;
    if (requestIsValid) {
        for (std::size_t attempt = 0; attempt < request.attemptBudget;
             ++attempt) {
            ++attemptsPerformed;
            try {
                auto level = std::make_unique<GeneratedLevel>(
                    deriveMatchLevelConfig(request, attempt),
                    MatchGenerationInfo {
                        request.matchSeed, attempt + 1,
                        profile.id, false
                    });
                if (matchMapMeetsProfile(*level, profile)) {
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
            request.matchSeed, attemptsPerformed,
            PhysicalMapProfile::SystemsFixture, true
        });
}
