#pragma once

#include "generated_level.hpp"
#include "horde_match.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct MatchMapMetrics {
    float minimumDoorwayWidth = 0.0F;
    float minimumSubstantialRoomArea = 0.0F;
    float anchorRoomArea = 0.0F;
    float minimumObjectiveClearance = 0.0F;
    float anchorRoomSpan = 0.0F;
    float startToExitRouteDistance = 0.0F;
    float maximumUsableIngressSeparation = 0.0F;
    std::size_t usableEnemySpawnCandidateCount = 0;
    std::size_t usableEnemySpawnRoomCount = 0;
    std::size_t hubDoorwayDegree = 0;

    float minimumDoorwayWidthInPlayerDiameters() const;
    float minimumSubstantialRoomAreaInPlayerDiameterSquares() const;
    float anchorRoomAreaInPlayerDiameterSquares() const;
    float minimumObjectiveClearanceInPlayerDiameters() const;
    float anchorRoomSpanInPlayerDiameters() const;
    float startToExitRouteDistanceInPlayerDiameters() const;
    float maximumUsableIngressSeparationInPlayerDiameters() const;
};

enum class GateStage : std::uint8_t {
    Initial,
    ExpansionOpen,
    AnchorOpen,
    ExitOpen,
    RewardOpen
};

enum class GateStageFailureCode : std::uint8_t {
    GateNotApproachable,
    ObjectiveUnreachable,
    GateAddsNoValue,
    SpawnCapacity
};

struct GateStageMetrics {
    GateStage stage = GateStage::Initial;
    GatePurpose openedGate = GatePurpose::Expansion;
    std::size_t reachableCellCount = 0;
    std::size_t reachableRoomCount = 0;
    std::size_t newlyReachableCellCount = 0;
    std::size_t routeSavingsTransitions = 0;
    std::size_t packedEnemySpawnSlots = 0;
    std::size_t enemySpawnRooms = 0;
    bool gateWasApproachable = false;
    bool objectiveIsReachable = false;
};

struct GateStageFailure {
    GateStageFailureCode code = GateStageFailureCode::ObjectiveUnreachable;
    GateStage stage = GateStage::Initial;
    GatePurpose gate = GatePurpose::Expansion;
    std::size_t doorway = 0;
};

struct GateStageValidationReport {
    std::vector<GateStageMetrics> stages;
    std::vector<GateStageFailure> failures;

    bool passed() const { return failures.empty(); }
};

MatchMapMetrics measureMatchMap(const GeneratedLevel& level);
GateStageValidationReport validateMatchStages(const GeneratedLevel& level,
    const SmallMapPlan& plan,
    std::size_t minimumRouteSavingsTransitions = 2,
    std::size_t minimumPackedEnemySpawnSlots = 0,
    std::size_t minimumEnemySpawnRooms = 0);
const char* gateStageName(GateStage stage);
const char* gateStageFailureName(GateStageFailureCode code);
