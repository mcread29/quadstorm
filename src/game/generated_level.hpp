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
    int gridRadius = 6;
    std::uint32_t gridSeed = 1;
    std::uint32_t roomSeed = 1;
    float worldScale = 0.16F;
};

// Fixed read-only browser matrix; the first entry also configures the active level.
inline constexpr std::array<GeneratedLevelConfig, 6>
    REPRESENTATIVE_LEVEL_CONFIGS {{
        { 6, 1, 1, 0.16F },
        { 6, 7, 19, 0.16F },
        { 6, 23, 41, 0.16F },
        { 8, 3, 17, 0.16F },
        { 8, 29, 73, 0.16F },
        { 10, 11, 5, 0.16F },
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
    explicit GeneratedLevel(GeneratedLevelConfig config = {});

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

private:
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
