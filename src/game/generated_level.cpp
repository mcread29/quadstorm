#include "generated_level.hpp"

#include "geometry/quantized_geometry.hpp"
#include "integration/room_grid_adapter.hpp"
#include "rooms/room_generator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

using CellIndex = stalberg::rooms::CellIndex;
using EdgeKey = stalberg::geometry::QuantizedEdge;

struct RuntimeEdge {
    Segment2D segment;
    std::vector<CellIndex> owners;
};

struct RuntimeGeometry {
    std::vector<FloorTriangle> floors;
    std::vector<Segment2D> walls;
};

struct SpawnSelection {
    Vector2 position;
    CellIndex cell;
};

Vector2 toWorld(stalberg::Point point, float scale)
{
    return Vector2 { point.x * scale, point.y * scale };
}

std::pair<CellIndex, CellIndex> orderedPair(CellIndex first, CellIndex second)
{
    return std::minmax(first, second);
}

std::set<std::pair<CellIndex, CellIndex>> doorwayPairs(
    const stalberg::rooms::RoomLayout& layout)
{
    std::set<std::pair<CellIndex, CellIndex>> result;
    for (const stalberg::rooms::Doorway& doorway : layout.getDoorways()) {
        result.insert(orderedPair(doorway.firstCell, doorway.secondCell));
    }
    return result;
}

stalberg::StalbergGrid makeGrid(const GeneratedLevelConfig& config)
{
    if (config.gridRadius < 2 || !std::isfinite(config.worldScale)
        || config.worldScale <= 0.0F) {
        throw std::invalid_argument("invalid generated-level configuration");
    }

    stalberg::StalbergGrid grid;
    grid.generate(config.gridRadius, config.gridSeed);
    grid.relaxToCompletion();
    return grid;
}

void validateGeneratedArtifacts(const stalberg::DualGrid& dual,
    const stalberg::rooms::RoomLayout& layout)
{
    const auto assignments = layout.getCellAssignments();
    if (layout.getRoomCount() == 0 || assignments.size() != dual.cells.size()) {
        throw std::runtime_error("generated level did not produce a valid room layout");
    }
    std::vector<bool> roomIds(layout.getRoomCount(), false);
    for (const stalberg::rooms::GeneratedRoom& room : layout.getRooms()) {
        if (room.id < 0 || static_cast<std::size_t>(room.id) >= roomIds.size()
            || roomIds[static_cast<std::size_t>(room.id)]) {
            throw std::runtime_error("generated level has invalid room identifiers");
        }
        roomIds[static_cast<std::size_t>(room.id)] = true;
    }
    if (std::ranges::find(roomIds, false) != roomIds.end()) {
        throw std::runtime_error("generated level room identifiers are not contiguous");
    }
}

std::vector<std::vector<CellIndex>> buildNavigation(
    const stalberg::rooms::RoomGrid& grid,
    const stalberg::rooms::RoomLayout& layout)
{
    const auto assignments = layout.getCellAssignments();
    const auto doors = doorwayPairs(layout);
    std::vector<std::vector<CellIndex>> result(assignments.size());
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == stalberg::rooms::EMPTY_CELL) {
            continue;
        }
        for (const CellIndex neighbor : grid.neighbors(cell)) {
            if (neighbor >= assignments.size()
                || assignments[neighbor] == stalberg::rooms::EMPTY_CELL) {
                continue;
            }
            if (assignments[cell] == assignments[neighbor]
                || doors.contains(orderedPair(cell, neighbor))) {
                result[cell].push_back(neighbor);
            }
        }
        std::ranges::sort(result[cell]);
    }
    return result;
}

RuntimeGeometry buildRuntimeGeometry(const stalberg::DualGrid& dual,
    const stalberg::rooms::RoomLayout& layout, float scale)
{
    RuntimeGeometry result;
    const auto assignments = layout.getCellAssignments();
    const auto doors = doorwayPairs(layout);
    std::size_t triangleCount = 0;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] != stalberg::rooms::EMPTY_CELL) {
            triangleCount += dual.cells[cell].polygon.size();
        }
    }
    result.floors.reserve(triangleCount);

    std::map<EdgeKey, RuntimeEdge> runtimeEdges;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == stalberg::rooms::EMPTY_CELL) {
            continue;
        }
        const stalberg::DualCell& dualCell = dual.cells[cell];
        if (dualCell.polygon.size() < 3) {
            throw std::runtime_error("assigned dual cell has no floor polygon");
        }
        const Vector2 center = toWorld(dualCell.center, scale);
        for (std::size_t point = 0; point < dualCell.polygon.size(); ++point) {
            const stalberg::Point first = dualCell.polygon[point];
            const stalberg::Point second
                = dualCell.polygon[(point + 1) % dualCell.polygon.size()];
            result.floors.push_back(FloorTriangle {
                center, toWorld(second, scale), toWorld(first, scale),
                cell, assignments[cell]
            });
            const EdgeKey key(
                stalberg::geometry::quantizePoint(first.x, first.y),
                stalberg::geometry::quantizePoint(second.x, second.y));
            const auto [found, inserted] = runtimeEdges.try_emplace(
                key, RuntimeEdge { Segment2D {
                                       toWorld(first, scale),
                                       toWorld(second, scale) },
                    {} });
            static_cast<void>(inserted);
            found->second.owners.push_back(cell);
        }
    }

    result.walls.reserve(runtimeEdges.size());
    for (const auto& [key, edge] : runtimeEdges) {
        static_cast<void>(key);
        bool open = false;
        if (edge.owners.size() == 2) {
            const CellIndex first = edge.owners[0];
            const CellIndex second = edge.owners[1];
            open = assignments[first] == assignments[second]
                || doors.contains(orderedPair(first, second));
        }
        if (!open) {
            result.walls.push_back(edge.segment);
        }
    }
    return result;
}

std::vector<DoorwayThreshold> buildDoorwayThresholds(
    const stalberg::DualGrid& dual,
    const stalberg::rooms::RoomLayout& layout, float scale)
{
    std::vector<DoorwayThreshold> result;
    const auto layoutDoorways = layout.getDoorways();
    result.reserve(layoutDoorways.size());
    for (std::size_t doorway = 0; doorway < layoutDoorways.size(); ++doorway) {
        const stalberg::rooms::Doorway& source = layoutDoorways[doorway];
        const auto cells = orderedPair(source.firstCell, source.secondCell);
        const auto connection = std::ranges::find_if(
            dual.connections, [&](const stalberg::DualConnection& candidate) {
                return orderedPair(candidate.cells.a, candidate.cells.b) == cells;
            });
        if (connection == dual.connections.end()) {
            throw std::runtime_error("generated doorway has no dual threshold");
        }
        result.push_back(DoorwayThreshold {
            doorway, source.firstRegion, source.firstCell,
            source.secondRegion, source.secondCell,
            Segment2D {
                toWorld(connection->first, scale),
                toWorld(connection->second, scale)
            }
        });
    }
    return result;
}

SpawnSelection selectPlayerSpawn(const stalberg::DualGrid& dual,
    const stalberg::rooms::RoomLayout& layout, float scale)
{
    int startRegion = stalberg::rooms::EMPTY_CELL;
    for (const stalberg::rooms::GeneratedRoom& room : layout.getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Start) {
            if (startRegion != stalberg::rooms::EMPTY_CELL) {
                throw std::runtime_error("generated level has multiple start rooms");
            }
            startRegion = room.id;
        }
    }
    if (startRegion == stalberg::rooms::EMPTY_CELL) {
        throw std::runtime_error("generated level has no start room");
    }

    const auto assignments = layout.getCellAssignments();
    float bestClearance = -std::numeric_limits<float>::infinity();
    std::optional<CellIndex> bestCell;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == startRegion
            && (!bestCell.has_value()
                || dual.cells[cell].clearance > bestClearance)) {
            bestClearance = dual.cells[cell].clearance;
            bestCell = cell;
        }
    }
    if (!bestCell.has_value()) {
        throw std::runtime_error("generated start room has no floor cell");
    }
    return SpawnSelection {
        toWorld(dual.cells[*bestCell].center, scale), *bestCell
    };
}

} // namespace

GeneratedLevel::GeneratedLevel(GeneratedLevelConfig config)
    : gridData(makeGrid(config))
    , dualData(stalberg::buildDualGrid(gridData))
    , roomGridData(stalberg::makeRoomGrid(gridData, dualData))
    , roomLayoutData(stalberg::rooms::RoomGenerator {}.generate(
          roomGridData, config.roomSeed,
          stalberg::rooms::RoomGenerationMethod::ShooterLayout))
    , scale(config.worldScale)
{
    validateGeneratedArtifacts(dualData, roomLayoutData);
    navigation = buildNavigation(roomGridData, roomLayoutData);
    RuntimeGeometry geometry
        = buildRuntimeGeometry(dualData, roomLayoutData, scale);
    floors = std::move(geometry.floors);
    wallSegments = std::move(geometry.walls);
    thresholds = buildDoorwayThresholds(dualData, roomLayoutData, scale);
    const SpawnSelection selected
        = selectPlayerSpawn(dualData, roomLayoutData, scale);
    spawn = selected.position;
    spawnCell = selected.cell;
}

std::span<const CellIndex> GeneratedLevel::traversableNeighbors(
    CellIndex cell) const
{
    if (cell >= navigation.size()) {
        return {};
    }
    return navigation[cell];
}

bool GeneratedLevel::canTraverse(CellIndex first, CellIndex second) const
{
    const auto neighbors = traversableNeighbors(first);
    return std::ranges::find(neighbors, second) != neighbors.end();
}

std::optional<CellIndex> GeneratedLevel::cellAtWorldPoint(Vector2 point) const
{
    const auto cell = stalberg::findDualCellAtPoint(
        dualData, stalberg::Point { point.x / scale, point.y / scale });
    if (!cell.has_value()
        || roomLayoutData.getCellAssignment(*cell) == stalberg::rooms::EMPTY_CELL) {
        return std::nullopt;
    }
    return cell;
}
