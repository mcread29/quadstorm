#include "game/horde_match.hpp"
#include "game/match_generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <vector>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool sameConfig(const GeneratedLevelConfig& first,
    const GeneratedLevelConfig& second)
{
    return first.gridRadius == second.gridRadius
        && first.gridSeed == second.gridSeed
        && first.roomSeed == second.roomSeed
        && first.worldScale == second.worldScale;
}

bool sameAcceptedLayout(const GeneratedLevel& first,
    const GeneratedLevel& second)
{
    return sameConfig(first.config(), second.config())
        && std::ranges::equal(first.roomLayout().getCellAssignments(),
            second.roomLayout().getCellAssignments())
        && first.roomLayout().getSelectedCandidate()
            == second.roomLayout().getSelectedCandidate()
        && first.roomLayout().getQualityScore()
            == second.roomLayout().getQualityScore();
}

bool sameSeedReproducesAcceptedMatch()
{
    // This seed rejects its first whole-map candidate and accepts a later one.
    constexpr std::uint64_t seed = 202;
    const auto first = generateMatchLevel(MatchGenerationRequest { seed });
    const auto repeated = generateMatchLevel(MatchGenerationRequest { seed });
    const auto& firstInfo = first->matchGeneration();
    const auto& repeatedInfo = repeated->matchGeneration();

    bool valid = check(firstInfo.has_value() && repeatedInfo.has_value(),
        "generated matches publish match generation metadata");
    valid &= check(firstInfo->matchSeed == seed
            && repeatedInfo->matchSeed == seed,
        "the requested public match seed is retained");
    valid &= check(firstInfo->attempts == repeatedInfo->attempts
            && firstInfo->usedFallback == repeatedInfo->usedFallback,
        "the same match seed reproduces retry and fallback metadata");
    valid &= check(!firstInfo->usedFallback && firstInfo->attempts > 1,
        "same-seed replay includes a deterministic retry winner");
    valid &= check(sameAcceptedLayout(*first, *repeated),
        "the same match seed reproduces the accepted layout exactly");
    return valid;
}

bool differentSeedsVaryNormalMatches()
{
    const auto first = generateMatchLevel(MatchGenerationRequest { 101 });
    const auto second = generateMatchLevel(MatchGenerationRequest { 202 });
    bool valid = check(!first->matchGeneration()->usedFallback
            && !second->matchGeneration()->usedFallback,
        "normal distinct match seeds do not use the fixture fallback");
    valid &= check(!sameConfig(first->config(), second->config()),
        "different match seeds derive different generation inputs");
    valid &= check(first->grid().getSeed() != second->grid().getSeed()
            || first->roomLayout().getSeed()
                != second->roomLayout().getSeed(),
        "different match seeds vary accepted geometry inputs");
    return valid;
}

bool restartPreservesAcceptedMap()
{
    const auto level = generateMatchLevel(MatchGenerationRequest { 303 });
    const GeneratedLevelConfig acceptedConfig = level->config();
    const MatchGenerationInfo acceptedInfo = *level->matchGeneration();
    const auto acceptedAssignments
        = level->roomLayout().getCellAssignments();
    const std::vector<int> assignmentSnapshot(
        acceptedAssignments.begin(), acceptedAssignments.end());

    LevelSession session(*level);
    HordeMatch match(*level, session);
    match.grantPoints(1000);
    PlayerInput restart;
    restart.restartPressed = true;
    const HordeMatchStepResult result = updateHordeMatch(
        match, session, restart, 1.0F / 120.0F);

    bool valid = check(result.reset,
        "R still resets mutable horde match state");
    valid &= check(sameConfig(level->config(), acceptedConfig)
            && level->matchGeneration()->matchSeed == acceptedInfo.matchSeed
            && level->matchGeneration()->attempts == acceptedInfo.attempts,
        "R preserves the accepted map configuration and seed");
    valid &= check(std::ranges::equal(
                       level->roomLayout().getCellAssignments(),
                       assignmentSnapshot),
        "R does not regenerate accepted geometry");
    return valid;
}

bool exhaustedBudgetUsesVisibleDeterministicFallback()
{
    // Seed 202 rejects attempt one, so a one-attempt budget exercises rejection
    // and exhaustion rather than bypassing the candidate loop.
    constexpr std::uint64_t seed = 202;
    const auto first = generateMatchLevel(MatchGenerationRequest {
        seed, 1
    });
    const auto repeated = generateMatchLevel(MatchGenerationRequest {
        seed, 1
    });

    bool valid = check(first->matchGeneration().has_value()
            && first->matchGeneration()->usedFallback
            && first->matchGeneration()->attempts == 1,
        "a rejected candidate exhausts the budget and visibly marks fallback");
    valid &= check(sameConfig(
                       first->config(), REPRESENTATIVE_LEVEL_CONFIGS.front()),
        "fallback publishes the known-valid regression fixture");
    valid &= check(sameAcceptedLayout(*first, *repeated),
        "fallback behavior is deterministic for a requested match seed");

    const auto invalidRequest = generateMatchLevel(MatchGenerationRequest {
        seed, DEFAULT_MATCH_GENERATION_ATTEMPTS, 1, 0.16F
    });
    valid &= check(invalidRequest->matchGeneration()->usedFallback
            && invalidRequest->matchGeneration()->attempts == 0,
        "invalid requests do not report candidate attempts that never ran");
    return valid;
}

bool derivationIncludesAttemptAndScaleProfile()
{
    const MatchGenerationRequest request {
        505, DEFAULT_MATCH_GENERATION_ATTEMPTS, 5, 0.18F
    };
    const GeneratedLevelConfig first = deriveMatchLevelConfig(request, 0);
    const GeneratedLevelConfig second = deriveMatchLevelConfig(request, 1);
    bool valid = check(first.gridSeed != second.gridSeed
            || first.roomSeed != second.roomSeed,
        "candidate retries derive distinct deterministic inputs");
    valid &= check(first.gridRadius == request.gridRadius
            && first.worldScale == request.worldScale,
        "the requested extent and physical-scale profile enter derivation");
    return valid;
}

} // namespace

int main()
{
    bool valid = true;
    valid &= sameSeedReproducesAcceptedMatch();
    valid &= differentSeedsVaryNormalMatches();
    valid &= restartPreservesAcceptedMap();
    valid &= exhaustedBudgetUsesVisibleDeterministicFallback();
    valid &= derivationIncludesAttemptAndScaleProfile();
    if (!valid) {
        return 1;
    }
    std::cout << "All match generation tests passed\n";
    return 0;
}
