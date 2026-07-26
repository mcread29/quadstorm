#include "rooms/room_generator.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace stalberg::rooms {
namespace {

constexpr std::size_t MINIMUM_ROOM_SIZE = 2;
constexpr std::size_t PREFERRED_ROOM_SIZE = 7;
constexpr std::size_t MAXIMUM_ROOM_SIZE = 34;
constexpr std::size_t REQUIRED_EDGE_CONNECTIONS = 4;

std::uint64_t mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

float unitNoise(std::uint32_t seed, std::uint64_t salt)
{
    const std::uint64_t value = mix(
        (static_cast<std::uint64_t>(seed) << 32U) ^ mix(salt));
    return static_cast<float>(value & 0xffffU) / 65535.0F;
}

bool topologyIsValid(const RoomGrid& grid)
{
    if (grid.neighbors.size() != grid.cells.size()) {
        return false;
    }
    return std::ranges::all_of(grid.neighbors, [&](const auto& neighbors) {
               return std::ranges::all_of(neighbors, [&](CellIndex neighbor) {
                   return neighbor < grid.cells.size();
               });
           })
        && std::ranges::all_of(grid.entranceCandidates, [&](CellIndex entrance) {
               return entrance < grid.cells.size();
           });
}

std::uint64_t gridFingerprint(const RoomGrid& grid)
{
    std::uint64_t fingerprint = mix(grid.cells.size());
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        const Cell& value = grid.cells[cell];
        const bool blocked = !value.buildable;
        fingerprint = mix(fingerprint
            ^ (static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(
                    value.position.x))
                << 32U)
            ^ std::bit_cast<std::uint32_t>(value.position.y)
            ^ static_cast<std::uint64_t>(blocked));
        for (const CellIndex neighbor : grid.neighbors[cell]) {
            fingerprint = mix(fingerprint ^ mix(cell) ^ (mix(neighbor) << 1U));
        }
    }
    return fingerprint;
}

float distance(CellPoint first, CellPoint second)
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return std::sqrt(x * x + y * y);
}

float distanceFromOrigin(CellPoint point)
{
    return std::sqrt(point.x * point.x + point.y * point.y);
}

std::vector<std::vector<CellIndex>> sortedAdjacency(const RoomGrid& grid)
{
    std::vector<std::vector<CellIndex>> adjacency;
    adjacency.reserve(grid.getNeighbors().size());
    for (const auto& neighbors : grid.getNeighbors()) {
        adjacency.push_back(neighbors);
        std::ranges::sort(adjacency.back());
    }
    return adjacency;
}

float estimateCellScale(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable)
{
    const auto cells = grid.getCells();
    float total = 0.0F;
    std::size_t count = 0;
    for (CellIndex cell = 0; cell < adjacency.size(); ++cell) {
        if (!buildable[cell]) {
            continue;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor > cell && buildable[neighbor]) {
                total += distance(cells[cell].position, cells[neighbor].position);
                ++count;
            }
        }
    }
    return count == 0 ? 24.0F : total / static_cast<float>(count);
}

struct ShapeParameters {
    int type;
    float axisX;
    float axisY;
    float halfLength;
    float halfWidth;
    float secondLength;
};

ShapeParameters randomShape(std::mt19937& random, float scale)
{
    std::uniform_int_distribution<int> type(0, 3);
    std::uniform_real_distribution<float> angle(0.0F, 2.0F * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> length(2.0F, 3.8F);
    std::uniform_real_distribution<float> width(1.25F, 2.25F);
    const float direction = angle(random);
    return ShapeParameters {
        type(random),
        std::cos(direction),
        std::sin(direction),
        length(random) * scale,
        width(random) * scale,
        length(random) * scale
    };
}

ShapeParameters randomRadialShape(
    std::mt19937& random, float scale, float directionX, float directionY)
{
    std::uniform_real_distribution<float> turn(-0.3F, 0.3F);
    std::uniform_real_distribution<float> length(3.0F, 5.4F);
    std::uniform_real_distribution<float> width(1.2F, 1.75F);
    const float angle = turn(random);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return ShapeParameters {
        1,
        directionX * cosine - directionY * sine,
        directionX * sine + directionY * cosine,
        length(random) * scale,
        width(random) * scale,
        length(random) * scale
    };
}

bool shapeContains(const ShapeParameters& shape, CellPoint center, CellPoint point)
{
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    const float along = x * shape.axisX + y * shape.axisY;
    const float across = -x * shape.axisY + y * shape.axisX;
    const float absoluteAlong = std::abs(along);
    const float absoluteAcross = std::abs(across);

    switch (shape.type) {
    case 0: // Soft rectangle.
        return absoluteAlong <= shape.halfLength
            && absoluteAcross <= shape.halfWidth
            && absoluteAlong / shape.halfLength
                    + absoluteAcross / shape.halfWidth
                <= 1.65F;
    case 1: // Long gallery.
        return absoluteAlong <= shape.halfLength * 1.25F
            && absoluteAcross <= shape.halfWidth * 0.72F;
    case 2: { // Capsule.
        const float endDistance = std::max(absoluteAlong - shape.halfLength, 0.0F);
        return endDistance * endDistance + across * across
            <= shape.halfWidth * shape.halfWidth;
    }
    default: // L-shaped room made from two perpendicular wings.
        return (along >= -shape.halfWidth && along <= shape.halfLength
                   && absoluteAcross <= shape.halfWidth)
            || (absoluteAlong <= shape.halfWidth
                && across >= -shape.halfWidth && across <= shape.secondLength);
    }
}

std::vector<CellIndex> makeRoomShape(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    const std::vector<bool>& temporarilyBlocked,
    CellIndex seedCell,
    const ShapeParameters& shape,
    std::size_t maximumSize)
{
    if (seedCell >= assignments.size() || !buildable[seedCell]
        || assignments[seedCell] != EMPTY_CELL || temporarilyBlocked[seedCell]) {
        return {};
    }

    const auto cells = grid.getCells();
    const CellPoint center = cells[seedCell].position;
    std::vector<bool> inside(cells.size(), false);
    for (CellIndex cell = 0; cell < cells.size(); ++cell) {
        inside[cell] = buildable[cell]
            && assignments[cell] == EMPTY_CELL
            && !temporarilyBlocked[cell]
            && shapeContains(shape, center, cells[cell].position);
    }
    inside[seedCell] = true;

    std::vector<CellIndex> result;
    std::vector<bool> visited(cells.size(), false);
    std::queue<CellIndex> queue;
    queue.push(seedCell);
    visited[seedCell] = true;
    while (!queue.empty() && result.size() < maximumSize) {
        const CellIndex cell = queue.front();
        queue.pop();
        result.push_back(cell);
        for (const CellIndex neighbor : adjacency[cell]) {
            if (inside[neighbor] && !visited[neighbor]) {
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return result;
}

std::vector<CellIndex> fallbackConnectedRoom(
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    CellIndex center,
    std::size_t maximumSize)
{
    if (center >= buildable.size() || !buildable[center]
        || assignments[center] != EMPTY_CELL) {
        return {};
    }

    std::vector<CellIndex> result;
    std::vector<bool> visited(buildable.size(), false);
    std::queue<CellIndex> queue;
    queue.push(center);
    visited[center] = true;
    while (!queue.empty() && result.size() < maximumSize) {
        const CellIndex cell = queue.front();
        queue.pop();
        result.push_back(cell);
        for (const CellIndex neighbor : adjacency[cell]) {
            if (buildable[neighbor] && assignments[neighbor] == EMPTY_CELL
                && !visited[neighbor]) {
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return result;
}

std::vector<CellIndex> shortestConnector(
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    const std::vector<CellIndex>& roomCells)
{
    const CellIndex noCell = assignments.size();
    std::vector<bool> roomMask(assignments.size(), false);
    std::vector<CellIndex> parent(assignments.size(), noCell);
    std::queue<CellIndex> queue;
    for (const CellIndex cell : roomCells) {
        roomMask[cell] = true;
        parent[cell] = cell;
        queue.push(cell);
    }

    while (!queue.empty()) {
        const CellIndex cell = queue.front();
        queue.pop();
        for (const CellIndex neighbor : adjacency[cell]) {
            if (assignments[neighbor] != EMPTY_CELL) {
                std::vector<CellIndex> connector;
                CellIndex pathCell = cell;
                while (!roomMask[pathCell]) {
                    connector.push_back(pathCell);
                    pathCell = parent[pathCell];
                }
                std::ranges::reverse(connector);
                return connector;
            }
            if (parent[neighbor] != noCell
                || (!buildable[neighbor] && !roomMask[neighbor])) {
                continue;
            }
            parent[neighbor] = cell;
            queue.push(neighbor);
        }
    }
    return {};
}

std::vector<CellIndex> builtFrontier(
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<int>& assignments)
{
    std::vector<CellIndex> frontier;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == EMPTY_CELL) {
            continue;
        }
        if (std::ranges::any_of(adjacency[cell], [&](CellIndex neighbor) {
                return assignments[neighbor] == EMPTY_CELL;
            })) {
            frontier.push_back(cell);
        }
    }
    return frontier;
}

std::vector<CellIndex> growConnector(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    CellIndex attachment,
    int connectorLength,
    float directionX,
    float directionY,
    std::uint32_t seed,
    std::uint64_t salt)
{
    const auto cells = grid.getCells();
    std::vector<CellIndex> path;
    CellIndex current = attachment;

    // One extra step is the future room seed; preceding steps join that room.
    for (int step = 0; step <= connectorLength; ++step) {
        CellIndex selected = cells.size();
        float selectedScore = -std::numeric_limits<float>::infinity();
        for (const CellIndex neighbor : adjacency[current]) {
            if (!buildable[neighbor] || assignments[neighbor] != EMPTY_CELL
                || std::ranges::find(path, neighbor) != path.end()) {
                continue;
            }

            const CellPoint from = cells[current].position;
            const CellPoint to = cells[neighbor].position;
            const float x = to.x - from.x;
            const float y = to.y - from.y;
            const float edgeLength = std::max(std::sqrt(x * x + y * y), 0.0001F);
            const float alignment = x / edgeLength * directionX
                + y / edgeLength * directionY;
            const float outward = distanceFromOrigin(to) - distanceFromOrigin(from);
            const float noise = unitNoise(seed,
                salt + static_cast<std::uint64_t>(step) * 4099U + neighbor);
            const float score = alignment * 2.0F
                + outward / edgeLength * 0.65F + noise * 0.8F;
            if (score > selectedScore
                || (score == selectedScore && neighbor < selected)) {
                selected = neighbor;
                selectedScore = score;
            }
        }
        if (selected == cells.size()) {
            return {};
        }
        path.push_back(selected);
        current = selected;
    }
    return path;
}

bool growBranchRoom(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    float cellScale,
    std::uint32_t generationSeed,
    std::mt19937& random,
    std::vector<int>& assignments,
    std::vector<GeneratedRoom>& rooms,
    std::size_t& occupiedCount,
    bool radial)
{
    std::vector<CellIndex> frontier = builtFrontier(adjacency, assignments);
    if (frontier.empty()) {
        return false;
    }

    if (radial && frontier.size() > 3) {
        std::ranges::sort(frontier, [&](CellIndex lhs, CellIndex rhs) {
            return distanceFromOrigin(grid.getCells()[lhs].position)
                < distanceFromOrigin(grid.getCells()[rhs].position);
        });
        frontier.resize(std::max<std::size_t>(3, frontier.size() / 2));
    }

    const auto cells = grid.getCells();
    std::uniform_real_distribution<float> turnDistribution(
        radial ? -0.35F : -0.8F,
        radial ? 0.35F : 0.8F);
    std::uniform_int_distribution<int> connectorLengthDistribution(
        radial ? 2 : 1,
        radial ? 5 : 3);
    constexpr std::size_t attemptsPerRoom = 90;

    for (std::size_t attempt = 0; attempt < attemptsPerRoom; ++attempt) {
        std::uniform_int_distribution<std::size_t> attachmentDistribution(
            0, frontier.size() - 1);
        const CellIndex attachment = frontier[attachmentDistribution(random)];

        CellPoint direction = cells[attachment].position;
        const float directionLength = distanceFromOrigin(direction);
        if (directionLength <= 0.0001F) {
            const float angle = turnDistribution(random) * std::numbers::pi_v<float>;
            direction = CellPoint { std::cos(angle), std::sin(angle) };
        } else {
            direction.x /= directionLength;
            direction.y /= directionLength;
        }
        const float turn = turnDistribution(random);
        const float cosine = std::cos(turn);
        const float sine = std::sin(turn);
        const float directionX = direction.x * cosine - direction.y * sine;
        const float directionY = direction.x * sine + direction.y * cosine;
        const int connectorLength = connectorLengthDistribution(random);
        const std::vector<CellIndex> connector = growConnector(
            grid,
            adjacency,
            buildable,
            assignments,
            attachment,
            connectorLength,
            directionX,
            directionY,
            generationSeed,
            rooms.size() * 100000U + attempt * 1000U);
        if (connector.size() != static_cast<std::size_t>(connectorLength + 1)) {
            continue;
        }

        std::vector<bool> blocked(cells.size(), false);
        for (std::size_t i = 0; i + 1 < connector.size(); ++i) {
            blocked[connector[i]] = true;
        }
        const CellIndex roomSeed = connector.back();
        const ShapeParameters shape = radial
            ? randomRadialShape(random, cellScale, directionX, directionY)
            : randomShape(random, cellScale);
        const std::vector<CellIndex> roomCells = makeRoomShape(
            grid,
            adjacency,
            buildable,
            assignments,
            blocked,
            roomSeed,
            shape,
            MAXIMUM_ROOM_SIZE);
        if (roomCells.size() < PREFERRED_ROOM_SIZE) {
            continue;
        }

        const int roomId = static_cast<int>(rooms.size());
        for (std::size_t i = 0; i + 1 < connector.size(); ++i) {
            assignments[connector[i]] = roomId;
        }
        for (const CellIndex cell : roomCells) {
            assignments[cell] = roomId;
        }
        const std::size_t roomSize = roomCells.size() + connector.size() - 1;
        rooms.push_back(GeneratedRoom { roomId, roomSize });
        occupiedCount += roomSize;
        return true;
    }
    return false;
}

} // namespace

RoomLayout RoomGenerator::generate(
    const RoomGrid& grid, std::uint32_t newSeed) const
{
    RoomLayout result;
    result.seed = newSeed;
    auto& rooms = result.rooms;
    auto& doorways = result.doorways;
    auto& connectedEntrances = result.connectedEntrances;
    auto& cellAssignments = result.cellAssignments;

    cellAssignments.assign(grid.getCellCount(), EMPTY_CELL);
    if (grid.getCellCount() == 0 || !topologyIsValid(grid)) {
        return result;
    }

    const auto cells = grid.getCells();
    const auto adjacency = sortedAdjacency(grid);
    std::vector<bool> buildable(cells.size(), false);
    std::vector<CellIndex> buildableCells;
    for (CellIndex cell = 0; cell < cells.size(); ++cell) {
        if (cells[cell].buildable) {
            buildable[cell] = true;
            buildableCells.push_back(cell);
        }
    }
    if (buildableCells.empty()) {
        return result;
    }

    const std::uint32_t generationSeed = static_cast<std::uint32_t>(mix(
        gridFingerprint(grid) ^ static_cast<std::uint64_t>(newSeed)));
    std::mt19937 random(generationSeed);
    const CellIndex center = *std::ranges::min_element(buildableCells,
        [&](CellIndex lhs, CellIndex rhs) {
            return distanceFromOrigin(cells[lhs].position)
                < distanceFromOrigin(cells[rhs].position);
        });
    const float cellScale = estimateCellScale(grid, adjacency, buildable);
    const std::size_t desiredRooms = std::clamp<std::size_t>(
        buildableCells.size() / 34, 7, MAX_GENERATED_ROOMS);
    std::uniform_real_distribution<float> coverageDistribution(0.42F, 0.55F);
    const std::size_t coverageTarget = static_cast<std::size_t>(
        static_cast<float>(buildableCells.size()) * coverageDistribution(random));

    std::vector<bool> noTemporaryBlocks(cells.size(), false);
    const std::size_t centralRoomLimit = std::clamp<std::size_t>(
        buildableCells.size() / 5,
        PREFERRED_ROOM_SIZE,
        MAXIMUM_ROOM_SIZE);
    std::vector<CellIndex> centralCells = makeRoomShape(
        grid,
        adjacency,
        buildable,
        cellAssignments,
        noTemporaryBlocks,
        center,
        randomShape(random, cellScale),
        centralRoomLimit);
    if (centralCells.size() < PREFERRED_ROOM_SIZE) {
        centralCells = fallbackConnectedRoom(
            adjacency, buildable, cellAssignments, center, centralRoomLimit);
    }
    if (centralCells.size() < MINIMUM_ROOM_SIZE) {
        return result;
    }
    for (const CellIndex cell : centralCells) {
        cellAssignments[cell] = 0;
    }
    rooms.push_back(GeneratedRoom { 0, centralCells.size() });
    std::size_t occupiedCount = centralCells.size();

    // Sometimes establish a few radial branches before connecting the outside.
    // This keeps an edge room from always being the room that reaches the center.
    const std::size_t maximumEarlyBranches = desiredRooms
            > REQUIRED_EDGE_CONNECTIONS + 1
        ? std::min<std::size_t>(
              4, desiredRooms - REQUIRED_EDGE_CONNECTIONS - 1)
        : 0;
    std::uniform_int_distribution<std::size_t> earlyBranchDistribution(
        0, maximumEarlyBranches);
    const std::size_t earlyBranchCount = earlyBranchDistribution(random);
    for (std::size_t branch = 0; branch < earlyBranchCount; ++branch) {
        if (!growBranchRoom(grid,
                adjacency,
                buildable,
                cellScale,
                generationSeed,
                random,
                cellAssignments,
                rooms,
                occupiedCount,
                true)) {
            break;
        }
    }

    // Randomize the supplied entrances and make only the selected candidate
    // buildable, preserving negative space around the rest of the footprint.
    std::vector<std::size_t> entranceOrder;
    entranceOrder.reserve(grid.entranceCandidates.size());
    for (std::size_t entrance = 0;
         entrance < grid.entranceCandidates.size(); ++entrance) {
        entranceOrder.push_back(entrance);
    }
    std::ranges::shuffle(entranceOrder, random);

    for (const std::size_t entrance : entranceOrder) {
        if (connectedEntrances.size() >= REQUIRED_EDGE_CONNECTIONS) {
            break;
        }
        const CellIndex edgeCenter = grid.entranceCandidates[entrance];
        if (cellAssignments[edgeCenter] != EMPTY_CELL) {
            continue;
        }

        std::vector<bool> edgeRoomBuildable = buildable;
        edgeRoomBuildable[edgeCenter] = true;
        std::vector<CellIndex> roomCells = makeRoomShape(
            grid,
            adjacency,
            edgeRoomBuildable,
            cellAssignments,
            noTemporaryBlocks,
            edgeCenter,
            randomShape(random, cellScale),
            MAXIMUM_ROOM_SIZE);
        if (roomCells.size() < MINIMUM_ROOM_SIZE) {
            roomCells = fallbackConnectedRoom(adjacency,
                edgeRoomBuildable,
                cellAssignments,
                edgeCenter,
                MINIMUM_ROOM_SIZE);
        }
        const auto touchesExistingFloor = [&](const std::vector<CellIndex>& cells) {
            return std::ranges::any_of(cells, [&](CellIndex cell) {
                return std::ranges::any_of(adjacency[cell], [&](CellIndex neighbor) {
                    return cellAssignments[neighbor] != EMPTY_CELL;
                });
            });
        };

        if (roomCells.size() < MINIMUM_ROOM_SIZE) {
            roomCells = { edgeCenter };
            const std::vector<CellIndex> connector = shortestConnector(
                adjacency, buildable, cellAssignments, roomCells);
            if (connector.empty() && !touchesExistingFloor(roomCells)) {
                continue;
            }

            const CellIndex connectionCell = connector.empty()
                ? edgeCenter
                : connector.back();
            const auto touchingRoom = std::ranges::find_if(
                adjacency[connectionCell], [&](CellIndex neighbor) {
                    return cellAssignments[neighbor] >= 0;
                });
            if (touchingRoom == adjacency[connectionCell].end()) {
                continue;
            }

            const int roomId = cellAssignments[*touchingRoom];
            for (const CellIndex cell : connector) {
                cellAssignments[cell] = roomId;
            }
            cellAssignments[edgeCenter] = roomId;
            rooms[static_cast<std::size_t>(roomId)].cellCount
                += connector.size() + 1;
            connectedEntrances.push_back(edgeCenter);
            occupiedCount += connector.size() + 1;
            continue;
        }

        const std::vector<CellIndex> connector = shortestConnector(
            adjacency, buildable, cellAssignments, roomCells);
        if (connector.empty() && !touchesExistingFloor(roomCells)) {
            continue;
        }

        const int roomId = static_cast<int>(rooms.size());
        for (const CellIndex cell : connector) {
            cellAssignments[cell] = roomId;
        }
        for (const CellIndex cell : roomCells) {
            cellAssignments[cell] = roomId;
        }
        rooms.push_back(GeneratedRoom {
            roomId,
            roomCells.size() + connector.size()
        });
        connectedEntrances.push_back(edgeCenter);
        occupiedCount += connector.size() + roomCells.size();
    }

    std::bernoulli_distribution radialGrowthDistribution(0.45);
    while (rooms.size() < desiredRooms && occupiedCount < coverageTarget) {
        const bool radial = radialGrowthDistribution(random);
        if (!growBranchRoom(grid,
                adjacency,
                buildable,
                cellScale,
                generationSeed,
                random,
                cellAssignments,
                rooms,
                occupiedCount,
                radial)) {
            break;
        }
    }

    using RegionPair = std::pair<int, int>;
    using CellPair = std::pair<CellIndex, CellIndex>;
    std::map<RegionPair, std::vector<CellPair>> sharedBoundaries;
    for (CellIndex cell = 0; cell < cellAssignments.size(); ++cell) {
        const int firstRegion = cellAssignments[cell];
        if (firstRegion == EMPTY_CELL) {
            continue;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor <= cell) {
                continue;
            }
            const int secondRegion = cellAssignments[neighbor];
            if (secondRegion == EMPTY_CELL || secondRegion == firstRegion) {
                continue;
            }
            if (firstRegion < secondRegion) {
                sharedBoundaries[{ firstRegion, secondRegion }].emplace_back(cell, neighbor);
            } else {
                sharedBoundaries[{ secondRegion, firstRegion }].emplace_back(neighbor, cell);
            }
        }
    }

    for (auto& [regions, candidates] : sharedBoundaries) {
        std::ranges::sort(candidates, [&](const CellPair& lhs, const CellPair& rhs) {
            const std::uint64_t left = mix(static_cast<std::uint64_t>(generationSeed)
                ^ mix(lhs.first) ^ (mix(lhs.second) << 1U));
            const std::uint64_t right = mix(static_cast<std::uint64_t>(generationSeed)
                ^ mix(rhs.first) ^ (mix(rhs.second) << 1U));
            if (left != right) {
                return left < right;
            }
            return lhs < rhs;
        });
        const CellPair selected = candidates.front();
        doorways.push_back(Doorway {
            regions.first,
            selected.first,
            regions.second,
            selected.second
        });
    }
    return result;
}

} // namespace stalberg::rooms
