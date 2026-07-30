#pragma once

#include "game/collision_2d.hpp"
#include "grid/dual_grid.hpp"
#include "grid/stalberg_grid.hpp"
#include "rooms/room_grid.hpp"
#include "rooms/room_layout.hpp"

#include "raylib.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct GeneratedLevelConfig {
    int gridRadius = 5;
    std::uint32_t gridSeed = 1;
    std::uint32_t roomSeed = 1;
    float worldScale = 0.16F;
};

enum class PhysicalMapProfile : std::uint8_t {
    SystemsFixture,
    FortressV1
};

struct GenerationBrief {
    std::uint32_t generationVersion = 1;
    std::uint32_t constraintProfileVersion = 1;
    stalberg::rooms::LargeMapArchetype largeMapArchetype
        = stalberg::rooms::LargeMapArchetype::HubAndSpokes;
    stalberg::rooms::SmallMapRecipe questRecipe
        = stalberg::rooms::SmallMapRecipe::HubCircuit;

    bool operator==(const GenerationBrief&) const = default;
};

struct CandidateScoreBreakdown {
    float circulation = 0.0F;
    float physicalMargin = 0.0F;
    float progression = 0.0F;
    float generatorQuality = 0.0F;

    float total() const
    {
        return circulation + physicalMargin + progression + generatorQuality;
    }

    bool operator==(const CandidateScoreBreakdown&) const = default;
};

struct MatchGenerationInfo {
    std::uint64_t matchSeed = 0;
    std::size_t attempts = 0;
    std::size_t selectedAttempt = 0;
    std::size_t validCandidateCount = 0;
    PhysicalMapProfile physicalProfile = PhysicalMapProfile::SystemsFixture;
    bool usedFallback = false;
    std::optional<GenerationBrief> brief;
    std::uint64_t briefHash = 0;
    CandidateScoreBreakdown score;
};

// Fixed read-only browser and deterministic fallback matrix.
inline constexpr std::array<GeneratedLevelConfig, 6>
    REPRESENTATIVE_LEVEL_CONFIGS {{
        { 5, 1, 7, 0.16F },
        { 5, 1, 2, 0.16F },
        { 5, 1, 3, 0.16F },
        { 5, 1, 4, 0.16F },
        { 5, 1, 8, 0.16F },
        { 5, 1, 6, 0.16F },
    }};

struct FloorTriangle {
    Vector2 first {};
    Vector2 second {};
    Vector2 third {};
    stalberg::rooms::CellIndex cell = 0;
    int region = stalberg::rooms::EMPTY_CELL;
};

struct DoorwayThreshold {
    std::size_t doorway = 0;
    int firstRegion = stalberg::rooms::EMPTY_CELL;
    stalberg::rooms::CellIndex firstCell = 0;
    int secondRegion = stalberg::rooms::EMPTY_CELL;
    stalberg::rooms::CellIndex secondCell = 0;
    Segment2D segment {};
};

class GeneratedLevel {
public:
    explicit GeneratedLevel(GeneratedLevelConfig config = {},
        std::optional<MatchGenerationInfo> generation = std::nullopt);

    const stalberg::StalbergGrid& grid() const { return gridData; }
    const stalberg::DualGrid& dualGrid() const { return dualData; }
    const stalberg::rooms::RoomGrid& roomGrid() const { return roomGridData; }
    const stalberg::rooms::RoomLayout& roomLayout() const { return roomLayoutData; }
    std::span<const FloorTriangle> floorTriangles() const { return floors; }
    std::span<const Segment2D> walls() const { return wallSegments; }
    std::span<const DoorwayThreshold> doorwayThresholds() const
    {
        return thresholds;
    }
    std::span<const stalberg::rooms::CellIndex> traversableNeighbors(
        stalberg::rooms::CellIndex cell) const;
    bool canTraverse(stalberg::rooms::CellIndex first,
        stalberg::rooms::CellIndex second) const;
    std::optional<stalberg::rooms::CellIndex> cellAtWorldPoint(
        Vector2 point) const;

    Vector2 playerSpawn() const { return spawn; }
    stalberg::rooms::CellIndex playerSpawnCell() const { return spawnCell; }
    float worldScale() const { return scale; }
    const GeneratedLevelConfig& config() const { return configData; }
    const std::optional<MatchGenerationInfo>& matchGeneration() const
    {
        return generationInfo;
    }
    void finalizeMatchGeneration(MatchGenerationInfo generation);

private:
    GeneratedLevelConfig configData;
    std::optional<MatchGenerationInfo> generationInfo;
    stalberg::StalbergGrid gridData;
    stalberg::DualGrid dualData;
    stalberg::rooms::RoomGrid roomGridData;
    stalberg::rooms::RoomLayout roomLayoutData;
    std::vector<FloorTriangle> floors;
    std::vector<Segment2D> wallSegments;
    std::vector<DoorwayThreshold> thresholds;
    std::vector<std::vector<stalberg::rooms::CellIndex>> navigation;
    Vector2 spawn {};
    stalberg::rooms::CellIndex spawnCell = 0;
    float scale = 1.0F;
};
