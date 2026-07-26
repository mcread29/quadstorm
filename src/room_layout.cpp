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

constexpr std::size_t MINIMUM_ROOM_SIZE = 7;
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
    const ShapeParameters& shape)
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
    while (!queue.empty() && result.size() < MAXIMUM_ROOM_SIZE) {
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

std::vector<VertexIndex> fallbackCentralRoom(
    const std::vector<std::vector<VertexIndex>>& adjacency,
    const std::vector<bool>& buildable,
    VertexIndex center)
{
    std::vector<VertexIndex> result;
    std::vector<bool> visited(buildable.size(), false);
    std::queue<VertexIndex> queue;
    queue.push(center);
    visited[center] = true;
    while (!queue.empty() && result.size() < 12) {
        const VertexIndex cell = queue.front();
        queue.pop();
        result.push_back(cell);
        for (const VertexIndex neighbor : adjacency[cell]) {
            if (buildable[neighbor] && !visited[neighbor]) {
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return result;
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
    int corridorLength,
    float directionX,
    float directionY,
    std::uint32_t seed,
    std::uint64_t salt)
{
    const auto vertices = grid.getVertices();
    std::vector<VertexIndex> path;
    VertexIndex current = attachment;

    // One extra step is the future room seed; preceding steps become corridor.
    for (int step = 0; step <= corridorLength; ++step) {
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
    corridorCellCount = 0;
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

    std::mt19937 random(seed);
    const VertexIndex center = *std::ranges::min_element(buildableCells,
        [&](VertexIndex lhs, VertexIndex rhs) {
            return distanceFromOrigin(vertices[lhs].position)
                < distanceFromOrigin(vertices[rhs].position);
        });
    const float cellScale = estimateCellScale(grid, adjacency, buildable);
    const std::size_t desiredRooms = std::clamp<std::size_t>(
        buildableCells.size() / 34, 7, 16);
    std::uniform_real_distribution<float> coverageDistribution(0.42F, 0.55F);
    const std::size_t coverageTarget = static_cast<std::size_t>(
        static_cast<float>(buildableCells.size()) * coverageDistribution(random));

    std::vector<bool> noTemporaryBlocks(vertices.size(), false);
    std::vector<VertexIndex> centralCells = makeRoomShape(
        grid,
        adjacency,
        buildable,
        cellAssignments,
        noTemporaryBlocks,
        center,
        randomShape(random, cellScale));
    if (centralCells.size() < MINIMUM_ROOM_SIZE) {
        centralCells = fallbackCentralRoom(adjacency, buildable, center);
    }
    for (const VertexIndex cell : centralCells) {
        cellAssignments[cell] = 0;
    }
    rooms.push_back(GeneratedRoom { 0, centralCells.size() });
    std::size_t occupiedCount = centralCells.size();

    std::uniform_int_distribution<int> corridorLengthDistribution(1, 3);
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
            const int corridorLength = corridorLengthDistribution(random);
            const std::vector<VertexIndex> connector = growConnector(
                grid,
                adjacency,
                buildable,
                cellAssignments,
                attachment,
                corridorLength,
                directionX,
                directionY,
                seed,
                rooms.size() * 100000U + attempt * 1000U);
            if (connector.size() != static_cast<std::size_t>(corridorLength + 1)) {
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
                randomShape(random, cellScale));
            if (roomCells.size() < MINIMUM_ROOM_SIZE) {
                continue;
            }

            const int roomId = static_cast<int>(rooms.size());
            for (std::size_t i = 0; i + 1 < connector.size(); ++i) {
                cellAssignments[connector[i]] = CORRIDOR_CELL;
                ++corridorCellCount;
            }
            for (const VertexIndex cell : roomCells) {
                cellAssignments[cell] = roomId;
            }
            rooms.push_back(GeneratedRoom { roomId, roomCells.size() });
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
            const std::uint64_t left = mix(static_cast<std::uint64_t>(seed)
                ^ mix(lhs.first) ^ (mix(lhs.second) << 1U));
            const std::uint64_t right = mix(static_cast<std::uint64_t>(seed)
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
