#include "generated_level.hpp"

#include "integration/room_grid_adapter.hpp"
#include "rooms/room_generator.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

using CellIndex = stalberg::rooms::CellIndex;

constexpr double EDGE_KEY_SCALE = 1000.0;

struct PointKey {
    long long x = 0;
    long long y = 0;

    bool operator==(const PointKey&) const = default;
    bool operator<(const PointKey& other) const
    {
        return x < other.x || (x == other.x && y < other.y);
    }
};

struct EdgeKey {
    PointKey first;
    PointKey second;

    EdgeKey(PointKey a, PointKey b)
        : first(std::min(a, b)), second(std::max(a, b))
    {
    }

    bool operator==(const EdgeKey&) const = default;
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& edge) const
    {
        const auto hashPoint = [](PointKey point) {
            return std::hash<long long> {}(point.x)
                ^ (std::hash<long long> {}(point.y) << 1U);
        };
        return hashPoint(edge.first) ^ (hashPoint(edge.second) << 1U);
    }
};

struct RuntimeEdge {
    Segment2D segment;
    std::vector<CellIndex> owners;
};

PointKey pointKey(stalberg::Point point)
{
    return PointKey {
        std::llround(static_cast<double>(point.x) * EDGE_KEY_SCALE),
        std::llround(static_cast<double>(point.y) * EDGE_KEY_SCALE)
    };
}

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
    const auto assignments = roomLayoutData.getCellAssignments();
    if (roomLayoutData.getRoomCount() == 0
        || assignments.size() != dualData.cells.size()) {
        throw std::runtime_error("generated level did not produce a valid room layout");
    }

    const auto doors = doorwayPairs(roomLayoutData);
    navigation.resize(assignments.size());
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == stalberg::rooms::EMPTY_CELL) {
            continue;
        }

        for (const CellIndex neighbor : roomGridData.neighbors[cell]) {
            if (neighbor >= assignments.size()
                || assignments[neighbor] == stalberg::rooms::EMPTY_CELL) {
                continue;
            }
            if (assignments[cell] == assignments[neighbor]
                || doors.contains(orderedPair(cell, neighbor))) {
                navigation[cell].push_back(neighbor);
            }
        }
    }

    std::unordered_map<EdgeKey, RuntimeEdge, EdgeKeyHash> runtimeEdges;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == stalberg::rooms::EMPTY_CELL) {
            continue;
        }

        const stalberg::DualCell& dualCell = dualData.cells[cell];
        if (dualCell.polygon.size() < 3) {
            throw std::runtime_error("assigned dual cell has no floor polygon");
        }

        const Vector2 center = toWorld(dualCell.center, scale);
        floors.reserve(floors.size() + dualCell.polygon.size());
        for (std::size_t point = 0; point < dualCell.polygon.size(); ++point) {
            const stalberg::Point first = dualCell.polygon[point];
            const stalberg::Point second
                = dualCell.polygon[(point + 1) % dualCell.polygon.size()];
            floors.push_back(FloorTriangle {
                center,
                toWorld(second, scale),
                toWorld(first, scale),
                cell,
                assignments[cell]
            });

            const EdgeKey edge(pointKey(first), pointKey(second));
            const auto [found, inserted] = runtimeEdges.try_emplace(
                edge, RuntimeEdge { Segment2D {
                                        toWorld(first, scale),
                                        toWorld(second, scale) },
                    {} });
            static_cast<void>(inserted);
            found->second.owners.push_back(cell);
        }
    }

    wallSegments.reserve(runtimeEdges.size());
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
            wallSegments.push_back(edge.segment);
        }
    }

    const auto layoutDoorways = roomLayoutData.getDoorways();
    thresholds.reserve(layoutDoorways.size());
    for (std::size_t doorway = 0; doorway < layoutDoorways.size(); ++doorway) {
        const stalberg::rooms::Doorway& source = layoutDoorways[doorway];
        const auto cells = orderedPair(source.firstCell, source.secondCell);
        const auto connection = std::ranges::find_if(
            dualData.connections, [&](const stalberg::DualConnection& candidate) {
                return orderedPair(candidate.cells.a, candidate.cells.b) == cells;
            });
        if (connection == dualData.connections.end()) {
            throw std::runtime_error("generated doorway has no dual threshold");
        }
        thresholds.push_back(DoorwayThreshold {
            doorway,
            source.firstRegion,
            source.firstCell,
            source.secondRegion,
            source.secondCell,
            Segment2D {
                toWorld(connection->first, scale),
                toWorld(connection->second, scale)
            }
        });
    }

    int startRegion = stalberg::rooms::EMPTY_CELL;
    for (const stalberg::rooms::GeneratedRoom& room : roomLayoutData.getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Start) {
            startRegion = room.id;
            break;
        }
    }
    if (startRegion == stalberg::rooms::EMPTY_CELL) {
        throw std::runtime_error("generated level has no start room");
    }

    float bestClearance = -std::numeric_limits<float>::infinity();
    bool foundSpawn = false;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] != startRegion) {
            continue;
        }
        const float clearance = dualData.cells[cell].clearance;
        if (!foundSpawn || clearance > bestClearance) {
            foundSpawn = true;
            bestClearance = clearance;
            spawnCell = cell;
        }
    }
    if (!foundSpawn) {
        throw std::runtime_error("generated start room has no floor cell");
    }
    spawn = toWorld(dualData.cells[spawnCell].center, scale);
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
