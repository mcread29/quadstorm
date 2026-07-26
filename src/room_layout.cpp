#include "room_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace stalberg {
namespace {

constexpr std::size_t MINIMUM_ROOM_SIZE = 2;
constexpr std::size_t PREFERRED_ROOM_SIZE = 7;
constexpr std::size_t MAXIMUM_ROOM_SIZE = 34;

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

float distance(Point first, Point second)
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return std::sqrt(x * x + y * y);
}

float distanceFromOrigin(Point point)
{
    return std::sqrt(point.x * point.x + point.y * point.y);
}

std::vector<std::vector<VertexIndex>> sortedAdjacency(const StalbergGrid& grid)
{
    std::vector<std::vector<VertexIndex>> adjacency;
    adjacency.reserve(grid.getNeighbors().size());
    for (const auto& neighbors : grid.getNeighbors()) {
        adjacency.push_back(neighbors);
        std::ranges::sort(adjacency.back());
    }
    return adjacency;
}

float estimateCellScale(
    const StalbergGrid& grid,
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<bool>& buildable)
{
    const auto vertices = grid.getVertices();
    float total = 0.0F;
    std::size_t count = 0;
    for (VertexIndex cell = 0; cell < adjacency.size(); ++cell) {
        if (!buildable[cell]) {
            continue;
        }
        for (const VertexIndex neighbor : adjacency[cell]) {
            if (neighbor > cell && buildable[neighbor]) {
                total += distance(vertices[cell].position, vertices[neighbor].position);
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

bool shapeContains(const ShapeParameters& shape, Point center, Point point)
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

std::vector<VertexIndex> makeRoomShape(
    const StalbergGrid& grid,
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    const std::vector<bool>& temporarilyBlocked,
    VertexIndex seedCell,
    const ShapeParameters& shape,
    std::size_t maximumSize)
{
    if (seedCell >= assignments.size() || !buildable[seedCell]
        || assignments[seedCell] != EMPTY_CELL || temporarilyBlocked[seedCell]) {
        return {};
    }

    const auto vertices = grid.getVertices();
    const Point center = vertices[seedCell].position;
    std::vector<bool> inside(vertices.size(), false);
    for (VertexIndex cell = 0; cell < vertices.size(); ++cell) {
        inside[cell] = buildable[cell]
            && assignments[cell] == EMPTY_CELL
            && !temporarilyBlocked[cell]
            && shapeContains(shape, center, vertices[cell].position);
    }
    inside[seedCell] = true;

    std::vector<VertexIndex> result;
    std::vector<bool> visited(vertices.size(), false);
    std::queue<VertexIndex> queue;
    queue.push(seedCell);
    visited[seedCell] = true;
    while (!queue.empty() && result.size() < maximumSize) {
        const VertexIndex cell = queue.front();
        queue.pop();
        result.push_back(cell);
        for (const VertexIndex neighbor : adjacency[cell]) {
            if (inside[neighbor] && !visited[neighbor]) {
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return result;
}

std::vector<VertexIndex> fallbackConnectedRoom(
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    VertexIndex center,
    std::size_t maximumSize)
{
    if (center >= buildable.size() || !buildable[center]
        || assignments[center] != EMPTY_CELL) {
        return {};
    }

    std::vector<VertexIndex> result;
    std::vector<bool> visited(buildable.size(), false);
    std::queue<VertexIndex> queue;
    queue.push(center);
    visited[center] = true;
    while (!queue.empty() && result.size() < maximumSize) {
        const VertexIndex cell = queue.front();
        queue.pop();
        result.push_back(cell);
        for (const VertexIndex neighbor : adjacency[cell]) {
            if (buildable[neighbor] && assignments[neighbor] == EMPTY_CELL
                && !visited[neighbor]) {
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return result;
}

std::vector<VertexIndex> boundarySideCenters(const StalbergGrid& grid)
{
    constexpr std::size_t sideCount = 6;
    const auto vertices = grid.getVertices();
    std::vector<VertexIndex> centers;
    centers.reserve(sideCount);

    for (std::size_t side = 0; side < sideCount; ++side) {
        const float angle = -std::numbers::pi_v<float> * 0.5F
            + static_cast<float>(side) * std::numbers::pi_v<float> / 3.0F;
        const float directionX = std::cos(angle);
        const float directionY = std::sin(angle);
        VertexIndex selected = vertices.size();
        float bestProjection = -std::numeric_limits<float>::infinity();
        float bestTangentDistance = std::numeric_limits<float>::infinity();

        for (VertexIndex cell = 0; cell < vertices.size(); ++cell) {
            if (!vertices[cell].fixed) {
                continue;
            }
            const Point point = vertices[cell].position;
            const float projection = point.x * directionX + point.y * directionY;
            const float tangentDistance = std::abs(
                -point.x * directionY + point.y * directionX);
            if (projection > bestProjection + 0.001F
                || (std::abs(projection - bestProjection) <= 0.001F
                    && (tangentDistance < bestTangentDistance - 0.001F
                        || (std::abs(tangentDistance - bestTangentDistance) <= 0.001F
                            && cell < selected)))) {
                selected = cell;
                bestProjection = projection;
                bestTangentDistance = tangentDistance;
            }
        }

        if (selected != vertices.size()
            && std::ranges::find(centers, selected) == centers.end()) {
            centers.push_back(selected);
        }
    }
    return centers;
}

std::vector<VertexIndex> shortestConnector(
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    const std::vector<VertexIndex>& roomCells)
{
    const VertexIndex noCell = assignments.size();
    std::vector<bool> roomMask(assignments.size(), false);
    std::vector<VertexIndex> parent(assignments.size(), noCell);
    std::queue<VertexIndex> queue;
    for (const VertexIndex cell : roomCells) {
        roomMask[cell] = true;
        parent[cell] = cell;
        queue.push(cell);
    }

    while (!queue.empty()) {
        const VertexIndex cell = queue.front();
        queue.pop();
        for (const VertexIndex neighbor : adjacency[cell]) {
            if (assignments[neighbor] != EMPTY_CELL) {
                std::vector<VertexIndex> connector;
                VertexIndex pathCell = cell;
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

std::vector<VertexIndex> builtFrontier(
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<int>& assignments)
{
    std::vector<VertexIndex> frontier;
    for (VertexIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == EMPTY_CELL) {
            continue;
        }
        if (std::ranges::any_of(adjacency[cell], [&](VertexIndex neighbor) {
                return assignments[neighbor] == EMPTY_CELL;
            })) {
            frontier.push_back(cell);
        }
    }
    return frontier;
}

std::vector<VertexIndex> growConnector(
    const StalbergGrid& grid,
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    VertexIndex attachment,
    int connectorLength,
    float directionX,
    float directionY,
    std::uint32_t seed,
    std::uint64_t salt)
{
    const auto vertices = grid.getVertices();
    std::vector<VertexIndex> path;
    VertexIndex current = attachment;

    // One extra step is the future room seed; preceding steps join that room.
    for (int step = 0; step <= connectorLength; ++step) {
        VertexIndex selected = vertices.size();
        float selectedScore = -std::numeric_limits<float>::infinity();
        for (const VertexIndex neighbor : adjacency[current]) {
            if (!buildable[neighbor] || assignments[neighbor] != EMPTY_CELL
                || std::ranges::find(path, neighbor) != path.end()) {
                continue;
            }

            const Point from = vertices[current].position;
            const Point to = vertices[neighbor].position;
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
        if (selected == vertices.size()) {
            return {};
        }
        path.push_back(selected);
        current = selected;
    }
    return path;
}

} // namespace

int RoomLayout::getCellAssignment(VertexIndex cell) const
{
    return cell < cellAssignments.size() ? cellAssignments[cell] : EMPTY_CELL;
}

void RoomLayout::generate(const StalbergGrid& grid, std::uint32_t newSeed)
{
    seed = newSeed;
    rooms.clear();
    doorways.clear();
    connectedEdgeCenters.clear();
    cellAssignments.assign(grid.getVertexCount(), EMPTY_CELL);
    if (grid.getVertexCount() == 0) {
        return;
    }

    const auto vertices = grid.getVertices();
    const auto adjacency = sortedAdjacency(grid);
    std::vector<bool> buildable(vertices.size(), false);
    std::vector<VertexIndex> buildableCells;
    for (VertexIndex cell = 0; cell < vertices.size(); ++cell) {
        if (!vertices[cell].fixed) {
            buildable[cell] = true;
            buildableCells.push_back(cell);
        }
    }
    if (buildableCells.empty()) {
        return;
    }

    const std::uint32_t generationSeed = static_cast<std::uint32_t>(mix(
        (static_cast<std::uint64_t>(grid.getSeed()) << 32U)
        ^ static_cast<std::uint64_t>(seed)));
    std::mt19937 random(generationSeed);
    const VertexIndex center = *std::ranges::min_element(buildableCells,
        [&](VertexIndex lhs, VertexIndex rhs) {
            return distanceFromOrigin(vertices[lhs].position)
                < distanceFromOrigin(vertices[rhs].position);
        });
    const float cellScale = estimateCellScale(grid, adjacency, buildable);
    const std::size_t desiredRooms = std::clamp<std::size_t>(
        buildableCells.size() / 34, 7, MAX_GENERATED_ROOMS);
    std::uniform_real_distribution<float> coverageDistribution(0.42F, 0.55F);
    const std::size_t coverageTarget = static_cast<std::size_t>(
        static_cast<float>(buildableCells.size()) * coverageDistribution(random));

    std::vector<bool> noTemporaryBlocks(vertices.size(), false);
    const std::size_t centralRoomLimit = std::clamp<std::size_t>(
        buildableCells.size() / 5,
        PREFERRED_ROOM_SIZE,
        MAXIMUM_ROOM_SIZE);
    std::vector<VertexIndex> centralCells = makeRoomShape(
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
        return;
    }
    for (const VertexIndex cell : centralCells) {
        cellAssignments[cell] = 0;
    }
    rooms.push_back(GeneratedRoom { 0, centralCells.size() });
    std::size_t occupiedCount = centralCells.size();

    // Randomize all six outer sides so compact grids do not repeatedly produce
    // the same three-way silhouette. Only an exact side-center cell is made
    // buildable, preserving negative space along the rest of the fixed boundary.
    const std::vector<VertexIndex> sideCenters = boundarySideCenters(grid);
    std::vector<std::size_t> sideOrder;
    sideOrder.reserve(sideCenters.size());
    for (std::size_t side = 0; side < sideCenters.size(); ++side) {
        sideOrder.push_back(side);
    }
    std::ranges::shuffle(sideOrder, random);

    for (const std::size_t side : sideOrder) {
        if (connectedEdgeCenters.size() >= 3) {
            break;
        }
        const VertexIndex edgeCenter = sideCenters[side];
        if (cellAssignments[edgeCenter] != EMPTY_CELL) {
            continue;
        }

        std::vector<bool> edgeRoomBuildable = buildable;
        edgeRoomBuildable[edgeCenter] = true;
        std::vector<VertexIndex> roomCells = makeRoomShape(
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
        const auto touchesExistingFloor = [&](const std::vector<VertexIndex>& cells) {
            return std::ranges::any_of(cells, [&](VertexIndex cell) {
                return std::ranges::any_of(adjacency[cell], [&](VertexIndex neighbor) {
                    return cellAssignments[neighbor] != EMPTY_CELL;
                });
            });
        };

        if (roomCells.size() < MINIMUM_ROOM_SIZE) {
            roomCells = { edgeCenter };
            const std::vector<VertexIndex> connector = shortestConnector(
                adjacency, buildable, cellAssignments, roomCells);
            if (connector.empty() && !touchesExistingFloor(roomCells)) {
                continue;
            }

            const VertexIndex connectionCell = connector.empty()
                ? edgeCenter
                : connector.back();
            const auto touchingRoom = std::ranges::find_if(
                adjacency[connectionCell], [&](VertexIndex neighbor) {
                    return cellAssignments[neighbor] >= 0;
                });
            if (touchingRoom == adjacency[connectionCell].end()) {
                continue;
            }

            const int roomId = cellAssignments[*touchingRoom];
            for (const VertexIndex cell : connector) {
                cellAssignments[cell] = roomId;
            }
            cellAssignments[edgeCenter] = roomId;
            rooms[static_cast<std::size_t>(roomId)].cellCount
                += connector.size() + 1;
            connectedEdgeCenters.push_back(edgeCenter);
            occupiedCount += connector.size() + 1;
            continue;
        }

        const std::vector<VertexIndex> connector = shortestConnector(
            adjacency, buildable, cellAssignments, roomCells);
        if (connector.empty() && !touchesExistingFloor(roomCells)) {
            continue;
        }

        const int roomId = static_cast<int>(rooms.size());
        for (const VertexIndex cell : connector) {
            cellAssignments[cell] = roomId;
        }
        for (const VertexIndex cell : roomCells) {
            cellAssignments[cell] = roomId;
        }
        rooms.push_back(GeneratedRoom {
            roomId,
            roomCells.size() + connector.size()
        });
        connectedEdgeCenters.push_back(edgeCenter);
        occupiedCount += connector.size() + roomCells.size();
    }

    std::uniform_int_distribution<int> connectorLengthDistribution(1, 3);
    std::uniform_real_distribution<float> turnDistribution(-0.8F, 0.8F);
    constexpr std::size_t attemptsPerRoom = 90;

    while (rooms.size() < desiredRooms && occupiedCount < coverageTarget) {
        bool placed = false;
        std::vector<VertexIndex> frontier = builtFrontier(
            adjacency, cellAssignments);
        if (frontier.empty()) {
            break;
        }

        for (std::size_t attempt = 0; attempt < attemptsPerRoom; ++attempt) {
            std::uniform_int_distribution<std::size_t> attachmentDistribution(
                0, frontier.size() - 1);
            const VertexIndex attachment = frontier[attachmentDistribution(random)];

            Point direction = vertices[attachment].position;
            float directionLength = distanceFromOrigin(direction);
            if (directionLength <= 0.0001F) {
                const float angle = turnDistribution(random) * std::numbers::pi_v<float>;
                direction = Point { std::cos(angle), std::sin(angle) };
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
            const std::vector<VertexIndex> connector = growConnector(
                grid,
                adjacency,
                buildable,
                cellAssignments,
                attachment,
                connectorLength,
                directionX,
                directionY,
                generationSeed,
                rooms.size() * 100000U + attempt * 1000U);
            if (connector.size() != static_cast<std::size_t>(connectorLength + 1)) {
                continue;
            }

            std::vector<bool> blocked(vertices.size(), false);
            for (std::size_t i = 0; i + 1 < connector.size(); ++i) {
                blocked[connector[i]] = true;
            }
            const VertexIndex roomSeed = connector.back();
            const std::vector<VertexIndex> roomCells = makeRoomShape(
                grid,
                adjacency,
                buildable,
                cellAssignments,
                blocked,
                roomSeed,
                randomShape(random, cellScale),
                MAXIMUM_ROOM_SIZE);
            if (roomCells.size() < PREFERRED_ROOM_SIZE) {
                continue;
            }

            const int roomId = static_cast<int>(rooms.size());
            for (std::size_t i = 0; i + 1 < connector.size(); ++i) {
                cellAssignments[connector[i]] = roomId;
            }
            for (const VertexIndex cell : roomCells) {
                cellAssignments[cell] = roomId;
            }
            rooms.push_back(GeneratedRoom {
                roomId,
                roomCells.size() + connector.size() - 1
            });
            occupiedCount += roomCells.size() + connector.size() - 1;
            placed = true;
            break;
        }
        if (!placed) {
            break;
        }
    }

    using RegionPair = std::pair<int, int>;
    using CellPair = std::pair<VertexIndex, VertexIndex>;
    std::map<RegionPair, std::vector<CellPair>> sharedBoundaries;
    for (VertexIndex cell = 0; cell < cellAssignments.size(); ++cell) {
        const int firstRegion = cellAssignments[cell];
        if (firstRegion == EMPTY_CELL) {
            continue;
        }
        for (const VertexIndex neighbor : adjacency[cell]) {
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
}

} // namespace stalberg
