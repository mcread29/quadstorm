#include "match_generator.hpp"

#include "horde_match.hpp"
#include "match_map_metrics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <ranges>

namespace {

constexpr std::uint64_t GRID_SEED_DOMAIN = 0x243f6a8885a308d3ULL;
constexpr std::uint64_t ROOM_SEED_DOMAIN = 0x13198a2e03707344ULL;
constexpr std::uint64_t ARCHETYPE_DOMAIN = 0xa4093822299f31d0ULL;
constexpr std::uint64_t QUEST_DOMAIN = 0x082efa98ec4e6c89ULL;
constexpr std::uint64_t BRIEF_HASH_DOMAIN = 0x452821e638d01377ULL;
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
    .minimumHubDoorwayDegree = 3,
    .minimumProgressionGateRouteSavingsTransitions = 2
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

void addMinimumFailure(MatchValidationReport& report,
    MatchValidationFailureCode code, double actual, double expectedMinimum,
    int room = stalberg::rooms::EMPTY_CELL, std::size_t doorway = 0)
{
    if (actual >= expectedMinimum) {
        return;
    }
    report.failures.push_back(MatchValidationFailure {
        code, expectedMinimum, actual, room, doorway
    });
}

void addRequiredFailure(MatchValidationReport& report,
    MatchValidationFailureCode code, bool present)
{
    addMinimumFailure(report, code, present ? 1.0 : 0.0, 1.0);
}

std::vector<MatchRejectionCount> publishedRejectionCounts(
    const std::array<std::size_t,
        static_cast<std::size_t>(MatchValidationFailureCode::Count)>& counts)
{
    std::vector<MatchRejectionCount> result;
    for (std::size_t code = 0; code < counts.size(); ++code) {
        if (counts[code] > 0) {
            result.push_back(MatchRejectionCount {
                static_cast<std::uint16_t>(code), counts[code]
            });
        }
    }
    return result;
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

const char* matchValidationFailureName(MatchValidationFailureCode code)
{
    switch (code) {
    case MatchValidationFailureCode::GridRadius:
        return "grid_radius";
    case MatchValidationFailureCode::WorldScale:
        return "world_scale";
    case MatchValidationFailureCode::RoomLayout:
        return "room_layout";
    case MatchValidationFailureCode::QualityScore:
        return "quality_score";
    case MatchValidationFailureCode::PlanRooms:
        return "plan_rooms";
    case MatchValidationFailureCode::RelayTargets:
        return "relay_targets";
    case MatchValidationFailureCode::ExpansionGate:
        return "expansion_gate";
    case MatchValidationFailureCode::AnchorGate:
        return "anchor_gate";
    case MatchValidationFailureCode::RewardGate:
        return "reward_gate";
    case MatchValidationFailureCode::ExitGate:
        return "exit_gate";
    case MatchValidationFailureCode::DoorwayWidth:
        return "doorway_width";
    case MatchValidationFailureCode::SubstantialRoomArea:
        return "substantial_room_area";
    case MatchValidationFailureCode::AnchorRoomArea:
        return "anchor_room_area";
    case MatchValidationFailureCode::ObjectiveClearance:
        return "objective_clearance";
    case MatchValidationFailureCode::AnchorRoomSpan:
        return "anchor_room_span";
    case MatchValidationFailureCode::RouteDistance:
        return "route_distance";
    case MatchValidationFailureCode::IngressSeparation:
        return "ingress_separation";
    case MatchValidationFailureCode::EnemySpawnCandidates:
        return "enemy_spawn_candidates";
    case MatchValidationFailureCode::EnemySpawnRooms:
        return "enemy_spawn_rooms";
    case MatchValidationFailureCode::HubDoorwayDegree:
        return "hub_doorway_degree";
    case MatchValidationFailureCode::StageGateApproach:
        return "stage_gate_approach";
    case MatchValidationFailureCode::StageObjectiveReachability:
        return "stage_objective_reachability";
    case MatchValidationFailureCode::StageGateValue:
        return "stage_gate_value";
    case MatchValidationFailureCode::Count:
        break;
    }
    return "unknown";
}

MatchValidationReport validateMatchMap(const GeneratedLevel& level,
    const MatchMapProfile& profile)
{
    MatchValidationReport report;
    report.constraintVersion = profile.constraintVersion;
    report.metrics = measureMatchMap(level);

    addMinimumFailure(report, MatchValidationFailureCode::GridRadius,
        level.config().gridRadius, profile.gridRadius);
    addMinimumFailure(report, MatchValidationFailureCode::WorldScale,
        level.worldScale(), profile.worldScale);
    addRequiredFailure(report, MatchValidationFailureCode::RoomLayout,
        level.roomLayout().getRoomCount() > 0);
    const float qualityScore = level.roomLayout().getQualityScore();
    addMinimumFailure(report, MatchValidationFailureCode::QualityScore,
        std::isfinite(qualityScore) ? qualityScore : 0.0, 0.000001);

    const SmallMapPlan plan = buildSmallMapPlan(level);
    const auto validRoom = [&](int room) {
        return room >= 0
            && static_cast<std::size_t>(room)
                < level.roomLayout().getRoomCount();
    };
    const std::array planRooms {
        plan.startRoom, plan.hubRoom, plan.anchorRoom,
        plan.rewardRoom, plan.exitRoom
    };
    const std::size_t validPlanRoomCount = std::ranges::count_if(
        planRooms, validRoom);
    addMinimumFailure(report, MatchValidationFailureCode::PlanRooms,
        validPlanRoomCount, planRooms.size());
    addMinimumFailure(report, MatchValidationFailureCode::RelayTargets,
        plan.relayTargets.size(), 3.0);
    addRequiredFailure(report, MatchValidationFailureCode::ExpansionGate,
        hasGate(plan, GatePurpose::Expansion));
    addRequiredFailure(report, MatchValidationFailureCode::AnchorGate,
        hasGate(plan, GatePurpose::Anchor));
    addRequiredFailure(report, MatchValidationFailureCode::RewardGate,
        hasGate(plan, GatePurpose::Reward));
    addRequiredFailure(report, MatchValidationFailureCode::ExitGate,
        hasGate(plan, GatePurpose::Exit));

    report.stages = validateMatchStages(level, plan,
        profile.minimumProgressionGateRouteSavingsTransitions);
    for (const GateStageFailure& stageFailure : report.stages.failures) {
        MatchValidationFailureCode code
            = MatchValidationFailureCode::StageObjectiveReachability;
        switch (stageFailure.code) {
        case GateStageFailureCode::GateNotApproachable:
            code = MatchValidationFailureCode::StageGateApproach;
            break;
        case GateStageFailureCode::ObjectiveUnreachable:
            break;
        case GateStageFailureCode::GateAddsNoValue:
            code = MatchValidationFailureCode::StageGateValue;
            break;
        }
        report.failures.push_back(MatchValidationFailure {
            .code = code,
            .expectedMinimum = 1.0,
            .actual = 0.0,
            .doorway = stageFailure.doorway,
            .stage = stageFailure.stage
        });
    }

    const MatchMapMetrics& metrics = report.metrics;
    addMinimumFailure(report, MatchValidationFailureCode::DoorwayWidth,
        metrics.minimumDoorwayWidthInPlayerDiameters(),
        profile.minimumDoorwayWidthInPlayerDiameters);
    addMinimumFailure(report,
        MatchValidationFailureCode::SubstantialRoomArea,
        metrics.minimumSubstantialRoomAreaInPlayerDiameterSquares(),
        profile.minimumSubstantialRoomAreaInPlayerDiameterSquares);
    addMinimumFailure(report, MatchValidationFailureCode::AnchorRoomArea,
        metrics.anchorRoomAreaInPlayerDiameterSquares(),
        profile.minimumAnchorRoomAreaInPlayerDiameterSquares,
        plan.anchorRoom);
    addMinimumFailure(report, MatchValidationFailureCode::ObjectiveClearance,
        metrics.minimumObjectiveClearanceInPlayerDiameters(),
        profile.minimumObjectiveClearanceInPlayerDiameters);
    addMinimumFailure(report, MatchValidationFailureCode::AnchorRoomSpan,
        metrics.anchorRoomSpanInPlayerDiameters(),
        profile.minimumAnchorRoomSpanInPlayerDiameters, plan.anchorRoom);
    addMinimumFailure(report, MatchValidationFailureCode::RouteDistance,
        metrics.startToExitRouteDistanceInPlayerDiameters(),
        profile.minimumRouteDistanceInPlayerDiameters);
    addMinimumFailure(report, MatchValidationFailureCode::IngressSeparation,
        metrics.maximumUsableIngressSeparationInPlayerDiameters(),
        profile.minimumUsableIngressSeparationInPlayerDiameters);
    addMinimumFailure(report, MatchValidationFailureCode::EnemySpawnCandidates,
        metrics.usableEnemySpawnCandidateCount,
        profile.minimumUsableEnemySpawnCandidates);
    addMinimumFailure(report, MatchValidationFailureCode::EnemySpawnRooms,
        metrics.usableEnemySpawnRoomCount,
        profile.minimumUsableEnemySpawnRooms);
    addMinimumFailure(report, MatchValidationFailureCode::HubDoorwayDegree,
        metrics.hubDoorwayDegree, profile.minimumHubDoorwayDegree,
        plan.hubRoom);
    return report;
}

bool matchMapMeetsProfile(const GeneratedLevel& level,
    const MatchMapProfile& profile)
{
    return validateMatchMap(level, profile).passed();
}

CandidateScoreBreakdown scoreMatchCandidate(const GeneratedLevel& level,
    const MatchValidationReport& report, const MatchMapProfile& profile)
{
    CandidateScoreBreakdown score;
    const auto& signature = level.roomLayout().getTopologySignature();
    if (signature.substantialRoomCount > 0) {
        const float roomCount
            = static_cast<float>(signature.substantialRoomCount);
        const float multiEntryRatio
            = static_cast<float>(signature.multiDoorSubstantialRoomCount)
            / roomCount;
        const float leafRatio
            = static_cast<float>(signature.degreeHistogram[1]) / roomCount;
        score.circulation = multiEntryRatio * 20.0F
            + std::min(static_cast<float>(signature.cycleRank), 3.0F)
                / 3.0F * 10.0F
            + (1.0F - std::clamp(leafRatio, 0.0F, 1.0F)) * 10.0F;
    }

    float physicalMarginTotal = 0.0F;
    std::size_t physicalMetricCount = 0;
    const auto addPhysicalMargin = [&](double actual, double minimum) {
        if (minimum <= 0.0) {
            return;
        }
        const double relativeMargin = actual / minimum - 1.0;
        physicalMarginTotal += static_cast<float>(
            std::clamp(relativeMargin / 0.5, 0.0, 1.0));
        ++physicalMetricCount;
    };
    const MatchMapMetrics& metrics = report.metrics;
    addPhysicalMargin(metrics.minimumDoorwayWidthInPlayerDiameters(),
        profile.minimumDoorwayWidthInPlayerDiameters);
    addPhysicalMargin(
        metrics.minimumSubstantialRoomAreaInPlayerDiameterSquares(),
        profile.minimumSubstantialRoomAreaInPlayerDiameterSquares);
    addPhysicalMargin(metrics.anchorRoomAreaInPlayerDiameterSquares(),
        profile.minimumAnchorRoomAreaInPlayerDiameterSquares);
    addPhysicalMargin(metrics.minimumObjectiveClearanceInPlayerDiameters(),
        profile.minimumObjectiveClearanceInPlayerDiameters);
    addPhysicalMargin(metrics.anchorRoomSpanInPlayerDiameters(),
        profile.minimumAnchorRoomSpanInPlayerDiameters);
    addPhysicalMargin(metrics.startToExitRouteDistanceInPlayerDiameters(),
        profile.minimumRouteDistanceInPlayerDiameters);
    addPhysicalMargin(
        metrics.maximumUsableIngressSeparationInPlayerDiameters(),
        profile.minimumUsableIngressSeparationInPlayerDiameters);
    addPhysicalMargin(metrics.usableEnemySpawnCandidateCount,
        profile.minimumUsableEnemySpawnCandidates);
    addPhysicalMargin(metrics.usableEnemySpawnRoomCount,
        profile.minimumUsableEnemySpawnRooms);
    if (physicalMetricCount > 0) {
        score.physicalMargin = physicalMarginTotal
            / static_cast<float>(physicalMetricCount) * 25.0F;
    }

    std::size_t newlyReachableCells = 0;
    std::size_t routeSavings = 0;
    for (const GateStageMetrics& stage : report.stages.stages) {
        newlyReachableCells += stage.newlyReachableCellCount;
        routeSavings += stage.routeSavingsTransitions;
    }
    const float cellCount = static_cast<float>(
        std::max<std::size_t>(1, level.roomGrid().getCellCount()));
    score.progression = std::clamp(
            static_cast<float>(newlyReachableCells) / cellCount,
            0.0F, 1.0F)
            * 20.0F
        + std::clamp(static_cast<float>(routeSavings) / 10.0F,
            0.0F, 1.0F)
            * 5.0F;
    score.generatorQuality = std::clamp(
        level.roomLayout().getQualityScore() / 100.0F, 0.0F, 1.0F)
        * 10.0F;
    return score;
}

GenerationBrief deriveGenerationBrief(const MatchGenerationRequest& request)
{
    constexpr std::uint64_t archetypeCount = 5;
    constexpr std::uint64_t questCount = 3;
    const auto archetype = static_cast<stalberg::rooms::LargeMapArchetype>(
        mixSeed(request.matchSeed ^ ARCHETYPE_DOMAIN) % archetypeCount);
    const auto quest = static_cast<stalberg::rooms::SmallMapRecipe>(
        mixSeed(request.matchSeed ^ QUEST_DOMAIN) % questCount);
    return GenerationBrief {
        .generationVersion = 1,
        .constraintProfileVersion
            = matchMapProfile(request.physicalProfile).constraintVersion,
        .largeMapArchetype = archetype,
        .questRecipe = quest
    };
}

std::uint64_t generationBriefHash(const GenerationBrief& brief)
{
    std::uint64_t value = BRIEF_HASH_DOMAIN;
    value = mixSeed(value ^ brief.generationVersion);
    value = mixSeed(value ^ brief.constraintProfileVersion);
    value = mixSeed(value
        ^ static_cast<std::uint64_t>(brief.largeMapArchetype));
    return mixSeed(value ^ static_cast<std::uint64_t>(brief.questRecipe));
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
    std::size_t validCandidateCount = 0;
    std::size_t firstValidAttempt = 0;
    std::size_t selectedAttempt = 0;
    CandidateScoreBreakdown selectedScore;
    std::array<std::size_t,
        static_cast<std::size_t>(MatchValidationFailureCode::Count)>
        rejectionCounts {};
    std::size_t constructionFailureCount = 0;
    std::unique_ptr<GeneratedLevel> selectedLevel;
    const GenerationBrief brief = deriveGenerationBrief(request);
    const std::uint64_t briefHash = generationBriefHash(brief);
    if (requestIsValid) {
        for (std::size_t attempt = 0; attempt < request.attemptBudget;
             ++attempt) {
            ++attemptsPerformed;
            try {
                auto level = std::make_unique<GeneratedLevel>(
                    deriveMatchLevelConfig(request, attempt),
                    MatchGenerationInfo {
                        .matchSeed = request.matchSeed,
                        .attempts = attempt + 1,
                        .selectedAttempt = attempt + 1,
                        .physicalProfile = profile.id,
                        .usedFallback = false,
                        .brief = brief,
                        .briefHash = briefHash,
                        .score = {},
                        .rejectionCounts = {},
                        .constructionFailureCount = 0
                    });
                const MatchValidationReport report
                    = validateMatchMap(*level, profile);
                if (!report.passed()) {
                    for (const MatchValidationFailure& failure
                        : report.failures) {
                        ++rejectionCounts[static_cast<std::size_t>(
                            failure.code)];
                    }
                    if (firstValidAttempt > 0
                        && attempt + 1 >= firstValidAttempt
                                + request.rankingAttemptBudget) {
                        break;
                    }
                    continue;
                }
                ++validCandidateCount;
                if (firstValidAttempt == 0) {
                    firstValidAttempt = attempt + 1;
                }
                const CandidateScoreBreakdown score
                    = scoreMatchCandidate(*level, report, profile);
                if (selectedLevel == nullptr
                    || score.total() > selectedScore.total()) {
                    selectedAttempt = attempt + 1;
                    selectedScore = score;
                    selectedLevel = std::move(level);
                }
                if (selectedScore.total() >= request.qualityMargin
                    || validCandidateCount
                        >= std::max<std::size_t>(
                            1, request.validCandidateBudget)
                    || attempt + 1 >= firstValidAttempt
                            + request.rankingAttemptBudget) {
                    break;
                }
            } catch (const std::exception&) {
                ++constructionFailureCount;
            }
        }
    }

    if (selectedLevel != nullptr) {
        selectedLevel->finalizeMatchGeneration(MatchGenerationInfo {
            .matchSeed = request.matchSeed,
            .attempts = attemptsPerformed,
            .selectedAttempt = selectedAttempt,
            .validCandidateCount = validCandidateCount,
            .physicalProfile = profile.id,
            .usedFallback = false,
            .brief = brief,
            .briefHash = briefHash,
            .score = selectedScore,
            .rejectionCounts = publishedRejectionCounts(rejectionCounts),
            .constructionFailureCount = constructionFailureCount
        });
        return selectedLevel;
    }

    return std::make_unique<GeneratedLevel>(
        REPRESENTATIVE_LEVEL_CONFIGS.front(),
        MatchGenerationInfo {
            .matchSeed = request.matchSeed,
            .attempts = attemptsPerformed,
            .physicalProfile = PhysicalMapProfile::SystemsFixture,
            .usedFallback = true,
            .brief = brief,
            .briefHash = briefHash,
            .score = {},
            .rejectionCounts = publishedRejectionCounts(rejectionCounts),
            .constructionFailureCount = constructionFailureCount
        });
}
