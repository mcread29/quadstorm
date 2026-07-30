#pragma once

#include "generated_level.hpp"
#include "match_map_metrics.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

inline constexpr std::size_t DEFAULT_MATCH_GENERATION_ATTEMPTS = 8;

struct MatchMapProfile {
    PhysicalMapProfile id = PhysicalMapProfile::SystemsFixture;
    std::uint32_t constraintVersion = 1;
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

enum class MatchValidationFailureCode : std::uint8_t {
    GridRadius,
    WorldScale,
    RoomLayout,
    QualityScore,
    PlanRooms,
    RelayTargets,
    ExpansionGate,
    AnchorGate,
    RewardGate,
    ExitGate,
    DoorwayWidth,
    SubstantialRoomArea,
    AnchorRoomArea,
    ObjectiveClearance,
    AnchorRoomSpan,
    RouteDistance,
    IngressSeparation,
    EnemySpawnCandidates,
    EnemySpawnRooms,
    HubDoorwayDegree
};

struct MatchValidationFailure {
    MatchValidationFailureCode code = MatchValidationFailureCode::RoomLayout;
    double expectedMinimum = 0.0;
    double actual = 0.0;
    int room = stalberg::rooms::EMPTY_CELL;
    std::size_t doorway = 0;

    bool operator==(const MatchValidationFailure&) const = default;
};

struct MatchValidationReport {
    std::uint32_t constraintVersion = 0;
    MatchMapMetrics metrics;
    std::vector<MatchValidationFailure> failures;

    bool passed() const { return failures.empty(); }
};

const MatchMapProfile& matchMapProfile(PhysicalMapProfile profile);
const char* physicalMapProfileName(PhysicalMapProfile profile);
const char* matchValidationFailureName(MatchValidationFailureCode code);
MatchValidationReport validateMatchMap(const GeneratedLevel& level,
    const MatchMapProfile& profile);
bool matchMapMeetsProfile(const GeneratedLevel& level,
    const MatchMapProfile& profile);
GenerationBrief deriveGenerationBrief(const MatchGenerationRequest& request);
std::uint64_t generationBriefHash(const GenerationBrief& brief);
GeneratedLevelConfig deriveMatchLevelConfig(
    const MatchGenerationRequest& request, std::size_t attempt);
std::unique_ptr<GeneratedLevel> generateMatchLevel(
    const MatchGenerationRequest& request);
