#pragma once

#include "generated_level.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

inline constexpr std::size_t DEFAULT_MATCH_GENERATION_ATTEMPTS = 8;

struct MatchMapProfile {
    PhysicalMapProfile id = PhysicalMapProfile::SystemsFixture;
    int gridRadius = 5;
    float worldScale = 0.16F;
    float minimumDoorwayWidthInPlayerDiameters = 0.0F;
    float minimumSubstantialRoomAreaInPlayerDiameterSquares = 0.0F;
    float minimumAnchorRoomAreaInPlayerDiameterSquares = 0.0F;
    float minimumObjectiveClearanceInPlayerDiameters = 0.0F;
    float minimumAnchorRoomSpanInPlayerDiameters = 0.0F;
    float minimumRouteDistanceInPlayerDiameters = 0.0F;
    float minimumUsableIngressSeparationInPlayerDiameters = 0.0F;
    std::size_t minimumUsableEnemySpawnCandidates = 0;
    std::size_t minimumUsableEnemySpawnRooms = 0;
    std::size_t minimumHubDoorwayDegree = 0;
};

struct MatchGenerationRequest {
    std::uint64_t matchSeed = 0;
    std::size_t attemptBudget = DEFAULT_MATCH_GENERATION_ATTEMPTS;
    PhysicalMapProfile physicalProfile = PhysicalMapProfile::FortressV1;
};

const MatchMapProfile& matchMapProfile(PhysicalMapProfile profile);
const char* physicalMapProfileName(PhysicalMapProfile profile);
bool matchMapMeetsProfile(const GeneratedLevel& level,
    const MatchMapProfile& profile);
GeneratedLevelConfig deriveMatchLevelConfig(
    const MatchGenerationRequest& request, std::size_t attempt);
std::unique_ptr<GeneratedLevel> generateMatchLevel(
    const MatchGenerationRequest& request);
