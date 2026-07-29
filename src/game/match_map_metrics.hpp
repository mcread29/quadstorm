#pragma once

#include "generated_level.hpp"

#include <cstddef>

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

MatchMapMetrics measureMatchMap(const GeneratedLevel& level);
