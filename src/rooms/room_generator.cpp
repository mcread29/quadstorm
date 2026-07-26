#include "rooms/room_generator.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <queue>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

namespace stalberg::rooms {
namespace {

constexpr std::size_t MINIMUM_ROOM_SIZE = 2;
constexpr std::size_t PREFERRED_ROOM_SIZE = 7;
constexpr std::size_t MAXIMUM_ROOM_SIZE = 34;
constexpr std::size_t MINIMUM_EDGE_CONNECTIONS = 3;
constexpr std::size_t MAXIMUM_EDGE_CONNECTIONS = 6;

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
    if (grid.neighbors.size() != grid.cells.size()
        || grid.connections.size() != grid.cells.size()) {
        return false;
    }
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        const Cell& geometry = grid.cells[cell];
        if (!std::isfinite(geometry.position.x) || !std::isfinite(geometry.position.y)
            || !std::isfinite(geometry.area) || geometry.area <= 0.0F
            || !std::isfinite(geometry.clearance) || geometry.clearance < 0.0F
            || grid.neighbors[cell].size() != grid.connections[cell].size()) {
            return false;
        }
        std::vector<CellIndex> neighbors = grid.neighbors[cell];
        std::vector<CellIndex> connectedCells;
        connectedCells.reserve(grid.connections[cell].size());
        for (const CellConnection& connection : grid.connections[cell]) {
            connectedCells.push_back(connection.cell);
        }
        std::ranges::sort(neighbors);
        std::ranges::sort(connectedCells);
        if (!std::ranges::equal(neighbors, connectedCells)
            || std::ranges::adjacent_find(neighbors) != neighbors.end()) {
            return false;
        }
        for (const CellConnection& connection : grid.connections[cell]) {
            if (connection.cell >= grid.cells.size() || connection.cell == cell
                || !std::isfinite(connection.distance) || connection.distance <= 0.0F
                || !std::isfinite(connection.sharedBoundaryLength)
                || connection.sharedBoundaryLength <= 0.0F) {
                return false;
            }
            const auto reverse = std::ranges::find(grid.connections[connection.cell],
                cell,
                &CellConnection::cell);
            if (reverse == grid.connections[connection.cell].end()) {
                return false;
            }
            const float distanceTolerance = std::max(connection.distance, 1.0F) * 0.0001F;
            const float widthTolerance
                = std::max(connection.sharedBoundaryLength, 1.0F) * 0.0001F;
            if (std::abs(reverse->distance - connection.distance) > distanceTolerance
                || std::abs(reverse->sharedBoundaryLength
                       - connection.sharedBoundaryLength)
                    > widthTolerance) {
                return false;
            }
        }
    }
    std::vector<CellIndex> entrances = grid.entranceCandidates;
    std::ranges::sort(entrances);
    return std::ranges::all_of(entrances, [&](CellIndex entrance) {
               return entrance < grid.cells.size();
           })
        && std::ranges::adjacent_find(entrances) == entrances.end();
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
        fingerprint = mix(fingerprint
            ^ std::bit_cast<std::uint32_t>(value.area)
            ^ (static_cast<std::uint64_t>(
                   std::bit_cast<std::uint32_t>(value.clearance))
                << 32U));
        std::vector<CellConnection> connections = grid.connections[cell];
        std::ranges::sort(connections, {}, &CellConnection::cell);
        for (const CellConnection& connection : connections) {
            fingerprint = mix(fingerprint ^ mix(cell)
                ^ (mix(connection.cell) << 1U)
                ^ std::bit_cast<std::uint32_t>(connection.sharedBoundaryLength)
                ^ (static_cast<std::uint64_t>(
                       std::bit_cast<std::uint32_t>(connection.distance))
                    << 32U));
        }
    }
    std::vector<CellIndex> entrances = grid.entranceCandidates;
    std::ranges::sort(entrances);
    for (const CellIndex entrance : entrances) {
        fingerprint = mix(fingerprint ^ mix(entrance));
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
    adjacency.reserve(grid.getConnections().size());
    for (const auto& connections : grid.getConnections()) {
        std::vector<CellIndex> neighbors;
        neighbors.reserve(connections.size());
        for (const CellConnection& connection : connections) {
            neighbors.push_back(connection.cell);
        }
        std::ranges::sort(neighbors);
        adjacency.push_back(std::move(neighbors));
    }
    return adjacency;
}

struct EntranceSelection {
    std::vector<CellIndex> order;
    std::size_t targetCount;
};

double combinationCount(std::size_t population, std::size_t selection)
{
    selection = std::min(selection, population - selection);
    double result = 1.0;
    for (std::size_t item = 1; item <= selection; ++item) {
        result *= static_cast<double>(population - selection + item)
            / static_cast<double>(item);
    }
    return result;
}

EntranceSelection selectEntrances(
    const std::vector<CellIndex>& candidates, std::mt19937& random)
{
    EntranceSelection result { candidates, 0 };
    if (candidates.empty()) {
        return result;
    }

    const std::size_t minimumCount
        = std::min(MINIMUM_EDGE_CONNECTIONS, candidates.size());
    const std::size_t maximumCount
        = std::min(MAXIMUM_EDGE_CONNECTIONS, candidates.size());
    // Weight each count by its number of combinations, then shuffle uniformly.
    // This gives every concrete subset the same probability.
    std::vector<double> countWeights;
    for (std::size_t count = minimumCount; count <= maximumCount; ++count) {
        countWeights.push_back(combinationCount(candidates.size(), count));
    }
    std::discrete_distribution<std::size_t> countDistribution(
        countWeights.begin(), countWeights.end());
    result.targetCount = minimumCount + countDistribution(random);
    std::ranges::shuffle(result.order, random);
    return result;
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

enum class GrowthStyle {
    Compact,
    Elongated,
    Branching,
    Irregular,
    LShaped
};

struct GrowthProfile {
    GrowthStyle style;
    float axisX;
    float axisY;
    float phase;
};

GrowthProfile randomGrowthProfile(std::mt19937& random)
{
    std::discrete_distribution<int> styleDistribution { 30, 25, 15, 20, 10 };
    std::uniform_real_distribution<float> angleDistribution(
        0.0F, 2.0F * std::numbers::pi_v<float>);
    const float angle = angleDistribution(random);
    return GrowthProfile {
        static_cast<GrowthStyle>(styleDistribution(random)),
        std::cos(angle),
        std::sin(angle),
        angleDistribution(random)
    };
}

GrowthProfile randomRadialGrowthProfile(
    std::mt19937& random, float directionX, float directionY)
{
    GrowthProfile profile = randomGrowthProfile(random);
    std::uniform_real_distribution<float> turnDistribution(-0.35F, 0.35F);
    const float turn = turnDistribution(random);
    const float cosine = std::cos(turn);
    const float sine = std::sin(turn);
    profile.axisX = directionX * cosine - directionY * sine;
    profile.axisY = directionX * sine + directionY * cosine;
    return profile;
}

struct ShapeParameters {
    GrowthProfile growth;
    float halfLength;
    float halfWidth;
    float secondLength;
};

ShapeParameters shapeForProfile(
    const GrowthProfile& growth, std::mt19937& random, float scale)
{
    std::uniform_real_distribution<float> lengthDistribution(2.0F, 3.8F);
    std::uniform_real_distribution<float> widthDistribution(1.25F, 2.25F);
    float halfLength = lengthDistribution(random);
    float halfWidth = widthDistribution(random);
    float secondLength = lengthDistribution(random);

    switch (growth.style) {
    case GrowthStyle::Compact:
        halfLength *= 0.82F;
        halfWidth *= 1.15F;
        break;
    case GrowthStyle::Elongated:
        halfLength *= 1.45F;
        halfWidth *= 0.62F;
        break;
    case GrowthStyle::Branching:
        halfLength *= 1.25F;
        halfWidth *= 0.7F;
        secondLength *= 1.2F;
        break;
    case GrowthStyle::Irregular:
        halfLength *= 1.05F;
        halfWidth *= 1.05F;
        break;
    case GrowthStyle::LShaped:
        halfLength *= 1.15F;
        halfWidth *= 0.72F;
        secondLength *= 1.25F;
        break;
    }

    return ShapeParameters {
        growth,
        halfLength * scale,
        halfWidth * scale,
        secondLength * scale
    };
}

ShapeParameters randomShape(std::mt19937& random, float scale)
{
    return shapeForProfile(randomGrowthProfile(random), random, scale);
}

ShapeParameters randomRadialShape(
    std::mt19937& random, float scale, float directionX, float directionY)
{
    return shapeForProfile(
        randomRadialGrowthProfile(random, directionX, directionY), random, scale);
}

bool shapeContains(const ShapeParameters& shape, CellPoint center, CellPoint point)
{
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    const float along = x * shape.growth.axisX + y * shape.growth.axisY;
    const float across = -x * shape.growth.axisY + y * shape.growth.axisX;
    const float absoluteAlong = std::abs(along);
    const float absoluteAcross = std::abs(across);

    switch (shape.growth.style) {
    case GrowthStyle::Compact:
        return absoluteAlong <= shape.halfLength
            && absoluteAcross <= shape.halfWidth
            && absoluteAlong / shape.halfLength
                    + absoluteAcross / shape.halfWidth
                <= 1.7F;
    case GrowthStyle::Elongated:
        return absoluteAlong <= shape.halfLength
            && absoluteAcross <= shape.halfWidth;
    case GrowthStyle::Branching:
        return (absoluteAlong <= shape.halfLength
                   && absoluteAcross <= shape.halfWidth * 0.6F)
            || (along >= shape.halfLength * 0.1F
                && along <= shape.halfLength * 0.65F
                && absoluteAcross <= shape.secondLength);
    case GrowthStyle::Irregular: {
        const float normalizedAlong = along / shape.halfLength;
        const float normalizedAcross = across / shape.halfWidth;
        const float angle = std::atan2(normalizedAcross, normalizedAlong);
        const float boundary = 0.9F
            + std::sin(angle * 3.0F + shape.growth.phase) * 0.16F
            + std::sin(angle * 5.0F - shape.growth.phase) * 0.1F;
        return std::sqrt(normalizedAlong * normalizedAlong
                   + normalizedAcross * normalizedAcross)
            <= boundary;
    }
    case GrowthStyle::LShaped:
        return (along >= -shape.halfWidth && along <= shape.halfLength
                   && absoluteAcross <= shape.halfWidth)
            || (absoluteAlong <= shape.halfWidth
                && across >= -shape.halfWidth && across <= shape.secondLength);
    }
    return false;
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

float connectionCost(const RoomGrid& grid, CellIndex first, CellIndex second)
{
    const auto connection = std::ranges::find(
        grid.connections[first], second, &CellConnection::cell);
    const Cell& destination = grid.cells[second];
    const float directDistance = distance(grid.cells[first].position, destination.position);
    const float edgeDistance = connection == grid.connections[first].end()
        ? directDistance
        : std::max(connection->distance, 0.001F);
    const float preferredClearance = edgeDistance * 0.32F;
    const float clearancePenalty = preferredClearance <= 0.0F
        ? 0.0F
        : std::max(0.0F, preferredClearance - destination.clearance)
            / preferredClearance;
    return edgeDistance * (1.0F + clearancePenalty * 2.5F);
}

std::vector<CellIndex> shortestConnector(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    const std::vector<CellIndex>& roomCells)
{
    using QueueEntry = std::pair<float, CellIndex>;
    const CellIndex noCell = assignments.size();
    std::vector<bool> roomMask(assignments.size(), false);
    std::vector<CellIndex> parent(assignments.size(), noCell);
    std::vector<float> costs(assignments.size(), std::numeric_limits<float>::infinity());
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> queue;
    for (const CellIndex cell : roomCells) {
        roomMask[cell] = true;
        parent[cell] = cell;
        costs[cell] = 0.0F;
        queue.emplace(0.0F, cell);
    }

    while (!queue.empty()) {
        const auto [cost, cell] = queue.top();
        queue.pop();
        if (cost > costs[cell]) {
            continue;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (assignments[neighbor] != EMPTY_CELL) {
                std::vector<CellIndex> connector;
                for (CellIndex pathCell = cell; !roomMask[pathCell];
                     pathCell = parent[pathCell]) {
                    connector.push_back(pathCell);
                }
                std::ranges::reverse(connector);
                return connector;
            }
            if (!buildable[neighbor] && !roomMask[neighbor]) {
                continue;
            }
            const float nextCost = cost + connectionCost(grid, cell, neighbor);
            if (nextCost >= costs[neighbor]) {
                continue;
            }
            costs[neighbor] = nextCost;
            parent[neighbor] = cell;
            queue.emplace(nextCost, neighbor);
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
            const auto connection = std::ranges::find(
                grid.connections[current], neighbor, &CellConnection::cell);
            const float width = connection == grid.connections[current].end()
                ? 0.0F
                : connection->sharedBoundaryLength;
            const float widthScore = std::clamp(width / edgeLength, 0.0F, 1.5F);
            const float noise = unitNoise(seed,
                salt + static_cast<std::uint64_t>(step) * 4099U + neighbor);
            const float score = alignment * 2.0F
                + outward / edgeLength * 0.65F
                + widthScore * 0.5F + noise * 0.8F;
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

std::vector<CellIndex> pathToFloor(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<bool>& floor,
    CellIndex start)
{
    using QueueEntry = std::pair<float, CellIndex>;
    const CellIndex noCell = adjacency.size();
    if (start >= adjacency.size()) {
        return {};
    }
    if (floor[start]) {
        return { start };
    }

    std::vector<CellIndex> parent(adjacency.size(), noCell);
    std::vector<float> costs(adjacency.size(), std::numeric_limits<float>::infinity());
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> queue;
    parent[start] = start;
    costs[start] = 0.0F;
    queue.emplace(0.0F, start);

    CellIndex destination = noCell;
    while (!queue.empty()) {
        const auto [cost, cell] = queue.top();
        queue.pop();
        if (cost > costs[cell]) {
            continue;
        }
        if (floor[cell]) {
            destination = cell;
            break;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (!floor[neighbor] && !buildable[neighbor]) {
                continue;
            }
            const float nextCost = cost + connectionCost(grid, cell, neighbor);
            if (nextCost >= costs[neighbor]) {
                continue;
            }
            costs[neighbor] = nextCost;
            parent[neighbor] = cell;
            queue.emplace(nextCost, neighbor);
        }
    }
    if (destination == noCell) {
        return {};
    }

    std::vector<CellIndex> path;
    for (CellIndex cell = destination;; cell = parent[cell]) {
        path.push_back(cell);
        if (cell == start) {
            break;
        }
    }
    std::ranges::reverse(path);
    return path;
}

void generateOrganicGrowth(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<CellIndex>& buildableCells,
    CellIndex center,
    const EntranceSelection& entranceSelection,
    std::uint32_t generationSeed,
    std::mt19937& random,
    std::vector<int>& assignments,
    std::vector<GeneratedRoom>& rooms,
    std::vector<CellIndex>& connectedEntrances)
{
    const auto cells = grid.getCells();
    const float cellScale = estimateCellScale(grid, adjacency, buildable);
    std::vector<bool> floor(cells.size(), false);
    floor[center] = true;
    std::size_t floorCount = 1;

    constexpr std::size_t sectorCount = 6;
    const auto sectorFor = [&](CellIndex cell) {
        const float x = cells[cell].position.x - cells[center].position.x;
        const float y = cells[cell].position.y - cells[center].position.y;
        float angle = std::atan2(y, x);
        if (angle < 0.0F) {
            angle += 2.0F * std::numbers::pi_v<float>;
        }
        return std::min(static_cast<std::size_t>(angle
                            / (2.0F * std::numbers::pi_v<float>)
                            * static_cast<float>(sectorCount)),
            sectorCount - 1);
    };
    std::array<std::size_t, sectorCount> sectorFloorCounts {};
    ++sectorFloorCounts[sectorFor(center)];

    const std::size_t entranceCount = std::min(
        entranceSelection.targetCount, entranceSelection.order.size());
    for (std::size_t entranceIndex = 0;
         entranceIndex < entranceCount; ++entranceIndex) {
        const CellIndex entrance = entranceSelection.order[entranceIndex];
        const std::vector<CellIndex> path
            = pathToFloor(grid, adjacency, buildable, floor, entrance);
        if (path.empty()) {
            continue;
        }
        for (const CellIndex cell : path) {
            if (!floor[cell]) {
                floor[cell] = true;
                ++floorCount;
                ++sectorFloorCounts[sectorFor(cell)];
            }
        }
        connectedEntrances.push_back(entrance);
    }

    std::uniform_real_distribution<float> coverageDistribution(0.44F, 0.54F);
    const std::size_t coverageTarget = std::max(floorCount,
        static_cast<std::size_t>(static_cast<float>(buildableCells.size())
            * coverageDistribution(random)));

    while (floorCount < coverageTarget) {
        CellIndex selected = cells.size();
        float selectedScore = -std::numeric_limits<float>::infinity();
        for (const CellIndex cell : buildableCells) {
            if (floor[cell]) {
                continue;
            }
            const std::size_t floorNeighbors = std::ranges::count_if(
                adjacency[cell], [&](CellIndex neighbor) { return floor[neighbor]; });
            if (floorNeighbors == 0) {
                continue;
            }
            const float noise = unitNoise(generationSeed,
                static_cast<std::uint64_t>(floorCount) * 65537U + cell);
            const std::size_t sector = sectorFor(cell);
            const float expectedPerSector = static_cast<float>(floorCount)
                / static_cast<float>(sectorCount);
            const float sectorBalance = std::clamp(
                (expectedPerSector
                    - static_cast<float>(sectorFloorCounts[sector]))
                    / std::max(expectedPerSector, 1.0F),
                -1.0F,
                1.0F);
            const float score = static_cast<float>(floorNeighbors) * 0.2F
                + noise * 1.05F + sectorBalance * 1.5F;
            if (score > selectedScore
                || (score == selectedScore && cell < selected)) {
                selected = cell;
                selectedScore = score;
            }
        }
        if (selected == cells.size()) {
            break;
        }
        floor[selected] = true;
        ++floorCount;
        ++sectorFloorCounts[sectorFor(selected)];
    }

    if (floorCount < MINIMUM_ROOM_SIZE) {
        return;
    }

    const std::size_t desiredRoomCount = std::clamp<std::size_t>(
        floorCount / 15, 2, MAX_GENERATED_ROOMS);
    const std::size_t roomCount = std::min(desiredRoomCount, floorCount / 2);
    std::vector<CellIndex> seeds;
    seeds.reserve(roomCount);
    seeds.push_back(center);
    while (seeds.size() < roomCount) {
        CellIndex selected = cells.size();
        float selectedScore = -1.0F;
        for (CellIndex cell = 0; cell < floor.size(); ++cell) {
            if (!floor[cell]
                || std::ranges::find(seeds, cell) != seeds.end()) {
                continue;
            }
            float nearestSquared = std::numeric_limits<float>::infinity();
            for (const CellIndex seed : seeds) {
                const float x = cells[cell].position.x - cells[seed].position.x;
                const float y = cells[cell].position.y - cells[seed].position.y;
                nearestSquared = std::min(nearestSquared, x * x + y * y);
            }
            const float noise = unitNoise(generationSeed,
                seeds.size() * 104729U + cell);
            const float score = nearestSquared * (0.8F + noise * 0.4F);
            if (score > selectedScore
                || (score == selectedScore && cell < selected)) {
                selected = cell;
                selectedScore = score;
            }
        }
        if (selected == cells.size()) {
            break;
        }
        seeds.push_back(selected);
    }

    std::vector<std::vector<CellIndex>> frontiers(seeds.size());
    std::vector<std::size_t> roomSizes(seeds.size(), 1);
    std::vector<float> roomSizeWeights;
    std::vector<GrowthProfile> growthProfiles;
    roomSizeWeights.reserve(seeds.size());
    growthProfiles.reserve(seeds.size());
    std::uniform_real_distribution<float> roomSizeDistribution(-0.8F, 0.8F);
    for (std::size_t room = 0; room < seeds.size(); ++room) {
        roomSizeWeights.push_back(seeds.size() < 3
                ? 1.0F
                : std::exp(roomSizeDistribution(random)));
        growthProfiles.push_back(randomGrowthProfile(random));
    }
    std::size_t assignedCount = 0;
    for (std::size_t room = 0; room < seeds.size(); ++room) {
        assignments[seeds[room]] = static_cast<int>(room);
        ++assignedCount;
        for (const CellIndex neighbor : adjacency[seeds[room]]) {
            if (floor[neighbor] && assignments[neighbor] == EMPTY_CELL) {
                frontiers[room].push_back(neighbor);
            }
        }
    }

    while (assignedCount < floorCount) {
        std::vector<std::size_t> activeRooms;
        for (std::size_t room = 0; room < frontiers.size(); ++room) {
            std::erase_if(frontiers[room], [&](CellIndex cell) {
                return assignments[cell] != EMPTY_CELL;
            });
            if (!frontiers[room].empty()) {
                activeRooms.push_back(room);
            }
        }
        if (activeRooms.empty()) {
            break;
        }

        std::ranges::shuffle(activeRooms, random);
        const std::size_t selectedRoom = *std::ranges::min_element(
            activeRooms, [&](std::size_t lhs, std::size_t rhs) {
                const float leftFill = static_cast<float>(roomSizes[lhs])
                    / roomSizeWeights[lhs];
                const float rightFill = static_cast<float>(roomSizes[rhs])
                    / roomSizeWeights[rhs];
                return leftFill < rightFill;
            });
        auto& frontier = frontiers[selectedRoom];
        const GrowthProfile& profile = growthProfiles[selectedRoom];
        const CellPoint seedPosition = cells[seeds[selectedRoom]].position;
        std::size_t frontierIndex = 0;
        float bestGrowthScore = -std::numeric_limits<float>::infinity();
        for (std::size_t candidateIndex = 0;
             candidateIndex < frontier.size(); ++candidateIndex) {
            const CellIndex candidate = frontier[candidateIndex];
            const float x = cells[candidate].position.x - seedPosition.x;
            const float y = cells[candidate].position.y - seedPosition.y;
            const float along = x * profile.axisX + y * profile.axisY;
            const float across = -x * profile.axisY + y * profile.axisX;
            const float radius = std::max(std::sqrt(x * x + y * y), cellScale);
            const float axisAlignment = std::abs(along) / radius;
            const float perpendicularAlignment = std::abs(across) / radius;
            const std::size_t sameRoomNeighbors = std::ranges::count_if(
                adjacency[candidate], [&](CellIndex neighbor) {
                    return assignments[neighbor] == static_cast<int>(selectedRoom);
                });
            const float noise = unitNoise(generationSeed,
                static_cast<std::uint64_t>(assignedCount) * 131071U
                    + selectedRoom * 8191U + candidate)
                    * 2.0F
                - 1.0F;

            float score = 0.0F;
            switch (profile.style) {
            case GrowthStyle::Compact:
                score = static_cast<float>(sameRoomNeighbors) * 1.0F
                    - radius / cellScale * 0.06F + noise * 0.3F;
                break;
            case GrowthStyle::Elongated:
                score = axisAlignment * 1.8F
                    - perpendicularAlignment * 0.7F
                    + static_cast<float>(sameRoomNeighbors) * 0.12F
                    + noise * 0.4F;
                break;
            case GrowthStyle::Branching:
                score = (sameRoomNeighbors == 1 ? 1.4F : 0.0F)
                    - static_cast<float>(sameRoomNeighbors) * 0.22F
                    + radius / cellScale * 0.04F + noise * 0.75F;
                break;
            case GrowthStyle::Irregular:
                score = static_cast<float>(sameRoomNeighbors) * 0.12F
                    + noise * 1.7F;
                break;
            case GrowthStyle::LShaped: {
                const bool followsForwardWing
                    = (axisAlignment >= perpendicularAlignment && along >= 0.0F)
                    || (perpendicularAlignment > axisAlignment && across >= 0.0F);
                score = std::max(axisAlignment, perpendicularAlignment) * 1.45F
                    + (followsForwardWing ? 0.55F : -0.45F)
                    + static_cast<float>(sameRoomNeighbors) * 0.15F
                    + noise * 0.4F;
                break;
            }
            }
            if (score > bestGrowthScore) {
                frontierIndex = candidateIndex;
                bestGrowthScore = score;
            }
        }
        const CellIndex cell = frontier[frontierIndex];
        frontier[frontierIndex] = frontier.back();
        frontier.pop_back();
        if (assignments[cell] != EMPTY_CELL) {
            continue;
        }

        assignments[cell] = static_cast<int>(selectedRoom);
        ++roomSizes[selectedRoom];
        ++assignedCount;
        for (const CellIndex neighbor : adjacency[cell]) {
            if (floor[neighbor] && assignments[neighbor] == EMPTY_CELL) {
                frontier.push_back(neighbor);
            }
        }
    }

    for (std::size_t room = 0; room < roomSizes.size(); ++room) {
        if (roomSizes[room] >= MINIMUM_ROOM_SIZE) {
            continue;
        }
        const auto roomCell = std::ranges::find(assignments, static_cast<int>(room));
        if (roomCell == assignments.end()) {
            continue;
        }
        const CellIndex cell = static_cast<CellIndex>(roomCell - assignments.begin());
        const auto neighbor = std::ranges::find_if(adjacency[cell], [&](CellIndex adjacent) {
            return assignments[adjacent] >= 0
                && assignments[adjacent] != static_cast<int>(room);
        });
        if (neighbor == adjacency[cell].end()) {
            continue;
        }
        const std::size_t destination
            = static_cast<std::size_t>(assignments[*neighbor]);
        assignments[cell] = static_cast<int>(destination);
        ++roomSizes[destination];
        roomSizes[room] = 0;
    }

    std::vector<int> remappedRoomIds(roomSizes.size(), EMPTY_CELL);
    for (std::size_t oldRoom = 0; oldRoom < roomSizes.size(); ++oldRoom) {
        if (roomSizes[oldRoom] < MINIMUM_ROOM_SIZE) {
            continue;
        }
        const int roomId = static_cast<int>(rooms.size());
        remappedRoomIds[oldRoom] = roomId;
        rooms.push_back(GeneratedRoom { roomId, roomSizes[oldRoom] });
    }
    for (int& assignment : assignments) {
        if (assignment >= 0) {
            assignment = remappedRoomIds[static_cast<std::size_t>(assignment)];
        }
    }
}

float sharedBoundaryLength(
    const RoomGrid& grid, CellIndex first, CellIndex second)
{
    const auto connection = std::ranges::find(
        grid.connections[first], second, &CellConnection::cell);
    return connection == grid.connections[first].end()
        ? 0.0F
        : connection->sharedBoundaryLength;
}

void generateDoorways(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    std::uint32_t generationSeed,
    const std::vector<int>& assignments,
    std::size_t roomCount,
    std::vector<Doorway>& doorways)
{
    using RegionPair = std::pair<int, int>;
    using CellPair = std::pair<CellIndex, CellIndex>;
    struct Contact {
        RegionPair regions;
        std::vector<CellPair> candidates;
        float quality = 0.0F;
        bool selected = false;
    };

    std::map<RegionPair, std::vector<CellPair>> sharedBoundaries;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int firstRegion = assignments[cell];
        if (firstRegion == EMPTY_CELL) {
            continue;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor <= cell) {
                continue;
            }
            const int secondRegion = assignments[neighbor];
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

    std::vector<Contact> contacts;
    contacts.reserve(sharedBoundaries.size());
    for (auto& [regions, candidates] : sharedBoundaries) {
        float bestQuality = 0.0F;
        for (const CellPair& candidate : candidates) {
            const float width = sharedBoundaryLength(grid, candidate.first, candidate.second);
            const float clearance = std::min(
                grid.cells[candidate.first].clearance,
                grid.cells[candidate.second].clearance);
            bestQuality = std::max(bestQuality, width + clearance * 0.35F);
        }
        contacts.push_back(Contact { regions, std::move(candidates), bestQuality });
    }

    std::ranges::sort(contacts, [&](const Contact& lhs, const Contact& rhs) {
        if (lhs.quality != rhs.quality) {
            return lhs.quality > rhs.quality;
        }
        const auto tieBreak = [&](const Contact& contact) {
            return mix(static_cast<std::uint64_t>(generationSeed)
                ^ mix(static_cast<std::uint64_t>(contact.regions.first))
                ^ (mix(static_cast<std::uint64_t>(contact.regions.second)) << 1U));
        };
        return tieBreak(lhs) < tieBreak(rhs);
    });

    std::vector<std::size_t> parent(roomCount);
    for (std::size_t room = 0; room < roomCount; ++room) {
        parent[room] = room;
    }
    const auto findRoot = [&](std::size_t room) {
        while (parent[room] != room) {
            parent[room] = parent[parent[room]];
            room = parent[room];
        }
        return room;
    };
    std::vector<std::vector<int>> roomGraph(roomCount);
    const auto selectContact = [&](Contact& contact) {
        contact.selected = true;
        roomGraph[static_cast<std::size_t>(contact.regions.first)]
            .push_back(contact.regions.second);
        roomGraph[static_cast<std::size_t>(contact.regions.second)]
            .push_back(contact.regions.first);
    };

    std::size_t treeEdges = 0;
    for (Contact& contact : contacts) {
        const std::size_t first = static_cast<std::size_t>(contact.regions.first);
        const std::size_t second = static_cast<std::size_t>(contact.regions.second);
        const std::size_t firstRoot = findRoot(first);
        const std::size_t secondRoot = findRoot(second);
        if (firstRoot == secondRoot) {
            continue;
        }
        parent[secondRoot] = firstRoot;
        selectContact(contact);
        ++treeEdges;
        if (treeEdges + 1 >= roomCount) {
            break;
        }
    }

    const auto graphDistance = [&](int start, int destination) {
        std::vector<int> distances(roomCount, -1);
        std::queue<int> queue;
        distances[static_cast<std::size_t>(start)] = 0;
        queue.push(start);
        while (!queue.empty()) {
            const int room = queue.front();
            queue.pop();
            if (room == destination) {
                return distances[static_cast<std::size_t>(room)];
            }
            for (const int neighbor : roomGraph[static_cast<std::size_t>(room)]) {
                if (distances[static_cast<std::size_t>(neighbor)] >= 0) {
                    continue;
                }
                distances[static_cast<std::size_t>(neighbor)]
                    = distances[static_cast<std::size_t>(room)] + 1;
                queue.push(neighbor);
            }
        }
        return -1;
    };

    const std::size_t desiredLoops = roomCount < 4
        ? 0
        : std::min<std::size_t>(4, std::max<std::size_t>(1, roomCount / 6));
    for (std::size_t loop = 0; loop < desiredLoops; ++loop) {
        Contact* selected = nullptr;
        float bestScore = -std::numeric_limits<float>::infinity();
        for (Contact& contact : contacts) {
            if (contact.selected) {
                continue;
            }
            const int cycleLength = graphDistance(
                contact.regions.first, contact.regions.second);
            if (cycleLength < 2) {
                continue;
            }
            const float noise = unitNoise(generationSeed,
                static_cast<std::uint64_t>(contact.regions.first) * 8191U
                    + static_cast<std::uint64_t>(contact.regions.second));
            const float score = static_cast<float>(cycleLength) * 100.0F
                + contact.quality + noise * 4.0F;
            if (score > bestScore) {
                selected = &contact;
                bestScore = score;
            }
        }
        if (selected == nullptr) {
            break;
        }
        selectContact(*selected);
    }

    for (Contact& contact : contacts) {
        if (!contact.selected || contact.candidates.empty()) {
            continue;
        }
        const auto best = std::ranges::max_element(
            contact.candidates, {}, [&](const CellPair& candidate) {
                const float width
                    = sharedBoundaryLength(grid, candidate.first, candidate.second);
                const float clearance = std::min(
                    grid.cells[candidate.first].clearance,
                    grid.cells[candidate.second].clearance);
                const float noise = unitNoise(generationSeed,
                    mix(candidate.first) ^ (mix(candidate.second) << 1U));
                return width + clearance * 0.35F + noise * 0.05F;
            });
        const float width = sharedBoundaryLength(grid, best->first, best->second);
        const float clearance = std::min(
            grid.cells[best->first].clearance,
            grid.cells[best->second].clearance);
        const float quality = contact.quality <= 0.0F
            ? 0.0F
            : std::clamp((width + clearance * 0.35F) / contact.quality,
                0.0F,
                1.0F);
        doorways.push_back(Doorway {
            contact.regions.first,
            best->first,
            contact.regions.second,
            best->second,
            width,
            quality
        });
    }
}

void annotateRooms(
    const RoomGrid& grid,
    const std::vector<int>& assignments,
    const std::vector<Doorway>& doorways,
    const std::vector<CellIndex>& connectedEntrances,
    CellIndex center,
    std::uint32_t generationSeed,
    std::vector<GeneratedRoom>& rooms)
{
    if (rooms.empty()) {
        return;
    }

    for (GeneratedRoom& room : rooms) {
        room.area = 0.0F;
        room.role = RoomRole::Combat;
        room.coverCandidates.clear();
        room.enemySpawnCandidates.clear();
    }
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int room = assignments[cell];
        if (room >= 0 && static_cast<std::size_t>(room) < rooms.size()) {
            rooms[static_cast<std::size_t>(room)].area += grid.cells[cell].area;
        }
    }

    std::vector<std::vector<int>> roomGraph(rooms.size());
    for (const Doorway& doorway : doorways) {
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }

    float totalArea = 0.0F;
    for (const GeneratedRoom& room : rooms) {
        totalArea += room.area;
    }
    const float averageArea = totalArea / static_cast<float>(rooms.size());
    for (GeneratedRoom& room : rooms) {
        const std::size_t degree = roomGraph[static_cast<std::size_t>(room.id)].size();
        if (degree >= 3) {
            room.role = RoomRole::Hub;
        } else if (degree == 2 && room.area < averageArea * 0.72F) {
            room.role = RoomRole::Connector;
        } else if (degree == 1
            && unitNoise(generationSeed, static_cast<std::uint64_t>(room.id) + 991U)
                < 0.55F) {
            room.role = RoomRole::Reward;
        }
    }

    const int startRoom = center < assignments.size() ? assignments[center] : EMPTY_CELL;
    if (startRoom < 0 || static_cast<std::size_t>(startRoom) >= rooms.size()) {
        return;
    }
    std::vector<int> distances(rooms.size(), -1);
    std::queue<int> queue;
    distances[static_cast<std::size_t>(startRoom)] = 0;
    queue.push(startRoom);
    while (!queue.empty()) {
        const int room = queue.front();
        queue.pop();
        for (const int neighbor : roomGraph[static_cast<std::size_t>(room)]) {
            if (distances[static_cast<std::size_t>(neighbor)] >= 0) {
                continue;
            }
            distances[static_cast<std::size_t>(neighbor)]
                = distances[static_cast<std::size_t>(room)] + 1;
            queue.push(neighbor);
        }
    }

    int exitRoom = startRoom;
    for (const CellIndex entrance : connectedEntrances) {
        if (entrance >= assignments.size()) {
            continue;
        }
        const int room = assignments[entrance];
        if (room >= 0 && distances[static_cast<std::size_t>(room)]
                > distances[static_cast<std::size_t>(exitRoom)]) {
            exitRoom = room;
        }
    }
    if (exitRoom == startRoom && rooms.size() > 1) {
        exitRoom = static_cast<int>(std::ranges::max_element(distances) - distances.begin());
    }

    rooms[static_cast<std::size_t>(startRoom)].role = RoomRole::Start;
    if (exitRoom != startRoom) {
        rooms[static_cast<std::size_t>(exitRoom)].role = RoomRole::Exit;
    }

    std::vector<int> distanceFromDoor(assignments.size(), -1);
    std::queue<CellIndex> cellQueue;
    const auto seedDoorCell = [&](CellIndex cell) {
        if (cell < assignments.size() && distanceFromDoor[cell] < 0) {
            distanceFromDoor[cell] = 0;
            cellQueue.push(cell);
        }
    };
    for (const Doorway& doorway : doorways) {
        seedDoorCell(doorway.firstCell);
        seedDoorCell(doorway.secondCell);
    }
    while (!cellQueue.empty()) {
        const CellIndex cell = cellQueue.front();
        cellQueue.pop();
        for (const CellIndex neighbor : grid.neighbors[cell]) {
            if (assignments[neighbor] == assignments[cell]
                && distanceFromDoor[neighbor] < 0) {
                distanceFromDoor[neighbor] = distanceFromDoor[cell] + 1;
                cellQueue.push(neighbor);
            }
        }
    }

    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int roomId = assignments[cell];
        if (roomId < 0 || distanceFromDoor[cell] < 1) {
            continue;
        }
        GeneratedRoom& room = rooms[static_cast<std::size_t>(roomId)];
        const bool touchesBoundary = std::ranges::any_of(
            grid.neighbors[cell], [&](CellIndex neighbor) {
                return assignments[neighbor] != roomId;
            });
        if (touchesBoundary) {
            room.coverCandidates.push_back(cell);
        }
        if (distanceFromDoor[cell] >= 2 && grid.cells[cell].clearance > 0.0F) {
            room.enemySpawnCandidates.push_back(cell);
        }
    }
}

bool candidateIsValid(const RoomGrid& grid, const RoomLayout& layout)
{
    const auto assignments = layout.getCellAssignments();
    const auto rooms = layout.getRooms();
    if (assignments.size() != grid.cells.size() || rooms.size() < 2) {
        return false;
    }
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int room = assignments[cell];
        if (room != EMPTY_CELL
            && (room < 0 || static_cast<std::size_t>(room) >= rooms.size())) {
            return false;
        }
    }
    if (rooms.size() > 1 && layout.getDoorways().size() < rooms.size() - 1) {
        return false;
    }

    std::vector<std::vector<int>> graph(rooms.size());
    for (const Doorway& doorway : layout.getDoorways()) {
        if (doorway.firstRegion < 0 || doorway.secondRegion < 0
            || static_cast<std::size_t>(doorway.firstRegion) >= rooms.size()
            || static_cast<std::size_t>(doorway.secondRegion) >= rooms.size()
            || doorway.firstCell >= assignments.size()
            || doorway.secondCell >= assignments.size()
            || assignments[doorway.firstCell] != doorway.firstRegion
            || assignments[doorway.secondCell] != doorway.secondRegion
            || std::ranges::find(grid.neighbors[doorway.firstCell], doorway.secondCell)
                == grid.neighbors[doorway.firstCell].end()) {
            return false;
        }
        graph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        graph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }

    const std::size_t startRooms = std::ranges::count(
        rooms, RoomRole::Start, &GeneratedRoom::role);
    const std::size_t exitRooms = std::ranges::count(
        rooms, RoomRole::Exit, &GeneratedRoom::role);
    if (startRooms != 1 || exitRooms != 1) {
        return false;
    }

    std::vector<bool> reached(rooms.size(), false);
    std::queue<int> queue;
    reached[0] = true;
    queue.push(0);
    while (!queue.empty()) {
        const int room = queue.front();
        queue.pop();
        for (const int neighbor : graph[static_cast<std::size_t>(room)]) {
            if (!reached[static_cast<std::size_t>(neighbor)]) {
                reached[static_cast<std::size_t>(neighbor)] = true;
                queue.push(neighbor);
            }
        }
    }
    return std::ranges::all_of(reached, [](bool value) { return value; });
}

float candidateScore(const RoomGrid& grid, const RoomLayout& layout)
{
    const auto rooms = layout.getRooms();
    const auto assignments = layout.getCellAssignments();
    float buildableArea = 0.0F;
    float floorArea = 0.0F;
    for (CellIndex cell = 0; cell < grid.cells.size(); ++cell) {
        if (grid.cells[cell].buildable) {
            buildableArea += grid.cells[cell].area;
        }
        if (assignments[cell] != EMPTY_CELL) {
            floorArea += grid.cells[cell].area;
        }
    }
    const float coverage = buildableArea <= 0.0F ? 0.0F : floorArea / buildableArea;
    const float coverageScore = std::clamp(
        1.0F - std::abs(coverage - 0.48F) / 0.16F, 0.0F, 1.0F);

    float averagePortalWidth = 0.0F;
    std::size_t portalCount = 0;
    for (CellIndex cell = 0; cell < grid.connections.size(); ++cell) {
        for (const CellConnection& connection : grid.connections[cell]) {
            if (connection.cell > cell && grid.cells[cell].buildable
                && grid.cells[connection.cell].buildable) {
                averagePortalWidth += connection.sharedBoundaryLength;
                ++portalCount;
            }
        }
    }
    averagePortalWidth = portalCount == 0
        ? 1.0F
        : averagePortalWidth / static_cast<float>(portalCount);

    float doorwayQuality = 0.0F;
    float minimumWidthRatio = 1.0F;
    for (const Doorway& doorway : layout.getDoorways()) {
        const float width
            = sharedBoundaryLength(grid, doorway.firstCell, doorway.secondCell);
        const float ratio = std::clamp(width / std::max(averagePortalWidth, 0.001F),
            0.0F,
            1.5F);
        doorwayQuality += ratio / 1.5F;
        minimumWidthRatio = std::min(minimumWidthRatio, std::min(ratio, 1.0F));
    }
    if (!layout.getDoorways().empty()) {
        doorwayQuality /= static_cast<float>(layout.getDoorways().size());
    }

    float meanArea = 0.0F;
    for (const GeneratedRoom& room : rooms) {
        meanArea += room.area;
    }
    meanArea /= static_cast<float>(rooms.size());
    float areaVariance = 0.0F;
    for (const GeneratedRoom& room : rooms) {
        const float difference = room.area - meanArea;
        areaVariance += difference * difference;
    }
    areaVariance /= static_cast<float>(rooms.size());
    const float variation = meanArea <= 0.0F
        ? 0.0F
        : std::sqrt(areaVariance) / meanArea;
    const float variationScore = std::clamp(
        1.0F - std::abs(variation - 0.55F) / 0.55F, 0.0F, 1.0F);

    const std::size_t loopCount = layout.getDoorways().size() >= rooms.size() - 1
        ? layout.getDoorways().size() - (rooms.size() - 1)
        : 0;
    const std::size_t desiredLoops = rooms.size() < 4
        ? 0
        : std::min<std::size_t>(4, std::max<std::size_t>(1, rooms.size() / 6));
    const float loopScore = desiredLoops == 0
        ? 1.0F
        : std::min(1.0F,
              static_cast<float>(loopCount) / static_cast<float>(desiredLoops));

    const float entranceScore = std::clamp(
        static_cast<float>(layout.getConnectedEntrances().size()) / 6.0F,
        0.0F,
        1.0F);

    std::vector<std::vector<int>> roomGraph(rooms.size());
    for (const Doorway& doorway : layout.getDoorways()) {
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }
    const auto start = std::ranges::find(
        rooms, RoomRole::Start, &GeneratedRoom::role);
    const auto exit = std::ranges::find(
        rooms, RoomRole::Exit, &GeneratedRoom::role);
    std::vector<int> distances(rooms.size(), -1);
    std::queue<int> queue;
    distances[static_cast<std::size_t>(start->id)] = 0;
    queue.push(start->id);
    while (!queue.empty()) {
        const int room = queue.front();
        queue.pop();
        for (const int neighbor : roomGraph[static_cast<std::size_t>(room)]) {
            if (distances[static_cast<std::size_t>(neighbor)] < 0) {
                distances[static_cast<std::size_t>(neighbor)]
                    = distances[static_cast<std::size_t>(room)] + 1;
                queue.push(neighbor);
            }
        }
    }
    const float normalizedPath = static_cast<float>(
        distances[static_cast<std::size_t>(exit->id)])
        / static_cast<float>(std::max<std::size_t>(rooms.size() - 1, 1));
    const float pathScore = std::clamp(
        1.0F - std::abs(normalizedPath - 0.65F) / 0.65F, 0.0F, 1.0F);
    const std::size_t deadEnds = std::ranges::count_if(roomGraph,
        [](const auto& connections) { return connections.size() == 1; });
    const float deadEndRatio
        = static_cast<float>(deadEnds) / static_cast<float>(rooms.size());
    const float branchScore = std::clamp(
        1.0F - std::abs(deadEndRatio - 0.25F) / 0.35F, 0.0F, 1.0F);
    const std::size_t maximumDegree = std::ranges::max(
        roomGraph | std::views::transform([](const auto& connections) {
            return connections.size();
        }));
    const float degreeScore = maximumDegree <= 4
        ? 1.0F
        : 1.0F / static_cast<float>(maximumDegree - 3);
    const float circulationScore
        = pathScore * 0.5F + branchScore * 0.3F + degreeScore * 0.2F;

    return coverageScore * 20.0F
        + doorwayQuality * 20.0F
        + minimumWidthRatio * 16.0F
        + variationScore * 12.0F
        + loopScore * 10.0F
        + circulationScore * 16.0F
        + entranceScore * 6.0F;
}

} // namespace

RoomLayout RoomGenerator::generateCandidate(const RoomGrid& grid,
    std::uint32_t requestedSeed,
    std::uint32_t variantSeed,
    RoomGenerationMethod method,
    const std::vector<CellIndex>& entranceOrder,
    std::size_t entranceTargetCount) const
{
    RoomLayout result;
    result.seed = requestedSeed;
    result.method = method;
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
        gridFingerprint(grid) ^ static_cast<std::uint64_t>(variantSeed)));
    std::mt19937 random(generationSeed);
    const CellIndex center = *std::ranges::min_element(buildableCells,
        [&](CellIndex lhs, CellIndex rhs) {
            return distanceFromOrigin(cells[lhs].position)
                < distanceFromOrigin(cells[rhs].position);
        });
    // Consume the same random choices in every candidate, but keep the requested
    // entrance brief fixed so best-of-N selection cannot bias entrance counts.
    static_cast<void>(selectEntrances(grid.entranceCandidates, random));
    const EntranceSelection entranceSelection {
        entranceOrder,
        entranceTargetCount
    };
    if (method == RoomGenerationMethod::OrganicGrowth) {
        generateOrganicGrowth(grid,
            adjacency,
            buildable,
            buildableCells,
            center,
            entranceSelection,
            generationSeed,
            random,
            cellAssignments,
            rooms,
            connectedEntrances);
        generateDoorways(
            grid, adjacency, generationSeed, cellAssignments, rooms.size(), doorways);
        annotateRooms(grid,
            cellAssignments,
            doorways,
            connectedEntrances,
            center,
            generationSeed,
            rooms);
        return result;
    }

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
            > entranceSelection.targetCount + 1
        ? std::min<std::size_t>(
              4, desiredRooms - entranceSelection.targetCount - 1)
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

    // Make only the selected candidates buildable, preserving negative space
    // around the rest of the footprint.
    const std::size_t entranceCount = std::min(
        entranceSelection.targetCount, entranceSelection.order.size());
    for (std::size_t entranceIndex = 0;
         entranceIndex < entranceCount; ++entranceIndex) {
        const CellIndex edgeCenter = entranceSelection.order[entranceIndex];
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
                grid, adjacency, buildable, cellAssignments, roomCells);
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
            grid, adjacency, buildable, cellAssignments, roomCells);
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

    generateDoorways(
        grid, adjacency, generationSeed, cellAssignments, rooms.size(), doorways);
    annotateRooms(grid,
        cellAssignments,
        doorways,
        connectedEntrances,
        center,
        generationSeed,
        rooms);
    return result;
}

RoomLayout RoomGenerator::generate(const RoomGrid& grid,
    std::uint32_t seed,
    RoomGenerationMethod method) const
{
    return generate(grid, seed, RoomGenerationOptions { method });
}

RoomLayout RoomGenerator::generate(const RoomGrid& grid,
    std::uint32_t seed,
    const RoomGenerationOptions& options) const
{
    const std::size_t candidateCount
        = std::clamp<std::size_t>(options.candidateCount, 1, 32);
    RoomLayout best;
    bool haveBest = false;
    float bestScore = -std::numeric_limits<float>::infinity();
    EntranceSelection entranceBrief { grid.entranceCandidates, 0 };
    if (topologyIsValid(grid)) {
        const std::uint32_t briefSeed = static_cast<std::uint32_t>(mix(
            gridFingerprint(grid) ^ static_cast<std::uint64_t>(seed)));
        std::mt19937 briefRandom(briefSeed);
        entranceBrief = selectEntrances(grid.entranceCandidates, briefRandom);
    }

    for (std::size_t candidateIndex = 0;
         candidateIndex < candidateCount; ++candidateIndex) {
        const std::uint32_t variantSeed = candidateIndex == 0
            ? seed
            : static_cast<std::uint32_t>(mix(
                (static_cast<std::uint64_t>(seed) << 32U)
                ^ mix(0x53484f4f544552ULL + candidateIndex)));
        RoomLayout candidate = generateCandidate(grid,
            seed,
            variantSeed,
            options.method,
            entranceBrief.order,
            entranceBrief.targetCount);
        candidate.selectedCandidate = candidateIndex;
        std::vector<CellIndex> expectedEntrances(entranceBrief.order.begin(),
            entranceBrief.order.begin()
                + static_cast<std::ptrdiff_t>(std::min(
                    entranceBrief.targetCount, entranceBrief.order.size())));
        std::vector<CellIndex> actualEntrances(
            candidate.getConnectedEntrances().begin(),
            candidate.getConnectedEntrances().end());
        std::ranges::sort(expectedEntrances);
        std::ranges::sort(actualEntrances);
        if (!std::ranges::equal(expectedEntrances, actualEntrances)
            || !candidateIsValid(grid, candidate)) {
            continue;
        }

        candidate.qualityScore = candidateScore(grid, candidate);
        if (!haveBest || candidate.qualityScore > bestScore) {
            bestScore = candidate.qualityScore;
            best = std::move(candidate);
            haveBest = true;
        }
    }
    if (haveBest) {
        return best;
    }

    RoomLayout empty;
    empty.seed = seed;
    empty.method = options.method;
    empty.cellAssignments.assign(grid.getCellCount(), EMPTY_CELL);
    return empty;
}

} // namespace stalberg::rooms
