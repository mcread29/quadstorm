#pragma once

#include "generated_level.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

inline constexpr std::size_t DEFAULT_MATCH_GENERATION_ATTEMPTS = 8;

struct MatchGenerationRequest {
    std::uint64_t matchSeed = 0;
    std::size_t attemptBudget = DEFAULT_MATCH_GENERATION_ATTEMPTS;
    int gridRadius = 5;
    float worldScale = 0.16F;
};

GeneratedLevelConfig deriveMatchLevelConfig(
    const MatchGenerationRequest& request, std::size_t attempt);
std::unique_ptr<GeneratedLevel> generateMatchLevel(
    const MatchGenerationRequest& request);
