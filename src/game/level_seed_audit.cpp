#include "match_generator.hpp"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

const char* archetypeName(stalberg::rooms::LargeMapArchetype archetype)
{
    switch (archetype) {
    case stalberg::rooms::LargeMapArchetype::HubAndSpokes:
        return "hub_and_spokes";
    case stalberg::rooms::LargeMapArchetype::RingAndBranches:
        return "ring_and_branches";
    case stalberg::rooms::LargeMapArchetype::MainSpine:
        return "main_spine";
    case stalberg::rooms::LargeMapArchetype::TwinDistricts:
        return "twin_districts";
    case stalberg::rooms::LargeMapArchetype::DenseCoreWithSparseBranch:
        return "dense_core_sparse_branch";
    }
    return "unknown";
}

const char* questName(stalberg::rooms::SmallMapRecipe recipe)
{
    switch (recipe) {
    case stalberg::rooms::SmallMapRecipe::HubCircuit:
        return "hub_circuit";
    case stalberg::rooms::SmallMapRecipe::BrokenRing:
        return "broken_ring";
    case stalberg::rooms::SmallMapRecipe::TwinWings:
        return "twin_wings";
    }
    return "unknown";
}

std::uint64_t parseArgument(char* text, std::uint64_t fallback)
{
    std::uint64_t result = 0;
    const std::string_view value(text);
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc {} && parsed.ptr == value.data() + value.size()
        ? result
        : fallback;
}

void printRejections(const MatchGenerationInfo& info)
{
    bool first = true;
    for (const MatchRejectionCount& rejection : info.rejectionCounts) {
        const auto code = static_cast<MatchValidationFailureCode>(
            rejection.code);
        std::cout << (first ? "" : ";")
                  << matchValidationFailureName(code) << ':' << rejection.count;
        first = false;
    }
    if (info.constructionFailureCount > 0) {
        std::cout << (first ? "" : ";")
                  << "construction:" << info.constructionFailureCount;
    }
}

} // namespace

int main(int argc, char** argv)
{
    const std::uint64_t firstSeed = argc > 1
        ? parseArgument(argv[1], 1)
        : 1;
    const std::uint64_t seedCount = argc > 2
        ? parseArgument(argv[2], 10)
        : 10;
    std::cout
        << "seed,fallback,attempts,selected_attempt,valid_candidates,archetype,"
           "quest,score,circulation,physical_margin,progression,generator_quality,"
           "cycles,useful_cycles,shortcut_savings,combat_leaves,"
           "multi_entry_ratio,rejections\n";
    std::cout << std::fixed << std::setprecision(4);
    for (std::uint64_t offset = 0; offset < seedCount; ++offset) {
        const std::uint64_t seed = firstSeed + offset;
        const auto level = generateMatchLevel(MatchGenerationRequest { seed });
        const MatchGenerationInfo& info = *level->matchGeneration();
        const GenerationBrief& brief = *info.brief;
        const auto& topology = level->roomLayout().getTopologySignature();
        const float multiEntryRatio = topology.substantialRoomCount == 0
            ? 0.0F
            : static_cast<float>(topology.multiDoorSubstantialRoomCount)
                / static_cast<float>(topology.substantialRoomCount);
        std::cout << seed << ',' << (info.usedFallback ? 1 : 0) << ','
                  << info.attempts << ',' << info.selectedAttempt << ','
                  << info.validCandidateCount << ','
                  << archetypeName(brief.largeMapArchetype) << ','
                  << questName(brief.questRecipe) << ',' << info.score.total()
                  << ',' << info.score.circulation << ','
                  << info.score.physicalMargin << ',' << info.score.progression
                  << ',' << info.score.generatorQuality << ','
                  << topology.cycleRank << ',' << topology.usefulCycleCount
                  << ',' << topology.minimumShortcutSavingsTransitions << ','
                  << topology.ordinaryCombatLeafCount << ','
                  << multiEntryRatio << ',';
        printRejections(info);
        std::cout << '\n';
    }
    return 0;
}
