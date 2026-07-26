#include "room_layout.hpp"
#include "stalberg_grid.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <queue>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct EdgeHash {
    std::size_t operator()(const stalberg::Edge& edge) const
    {
        return std::hash<std::size_t> {}(edge.a)
            ^ (std::hash<std::size_t> {}(edge.b) << 1U);
    }
};

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool topologyIsValid(const stalberg::StalbergGrid& grid)
{
    bool valid = true;
    const auto vertices = grid.getVertices();
    const auto neighbors = grid.getNeighbors();
    std::unordered_map<stalberg::Edge, std::size_t, EdgeHash> edgeUseCounts;

    valid &= check(neighbors.size() == vertices.size(), "each vertex has a neighbor list");

    for (const stalberg::Quad& quad : grid.getQuads()) {
        const bool indicesAreValid = std::ranges::all_of(quad,
            [vertexCount = vertices.size()](stalberg::VertexIndex index) {
                return index < vertexCount;
            });
        valid &= check(indicesAreValid, "quad indices are in range");
        if (!indicesAreValid) {
            continue;
        }

        for (std::size_t i = 0; i < quad.size(); ++i) {
            ++edgeUseCounts[stalberg::Edge(quad[i], quad[(i + 1) % quad.size()])];
        }
    }

    for (const auto& [edge, count] : edgeUseCounts) {
        valid &= check(count == 1 || count == 2, "edge belongs to one or two quads");
        if (count == 1) {
            valid &= check(
                vertices[edge.a].fixed && vertices[edge.b].fixed,
                "boundary edge endpoints are fixed");
        }
    }

    for (std::size_t vertex = 0; vertex < neighbors.size(); ++vertex) {
        for (const stalberg::VertexIndex neighbor : neighbors[vertex]) {
            const bool neighborIsValid = neighbor < neighbors.size();
            valid &= check(neighborIsValid, "neighbor index is in range");
            if (!neighborIsValid) {
                continue;
            }
            valid &= check(
                std::ranges::find(neighbors[neighbor], vertex) != neighbors[neighbor].end(),
                "neighbor relationship is symmetric");
        }
    }

    return valid;
}

bool generationIsRepeatable()
{
    stalberg::StalbergGrid first;
    stalberg::StalbergGrid second;
    first.generate(6, 1);
    second.generate(6, 1);

    return check(first.getQuads().size() == second.getQuads().size(),
               "same settings produce the same quad count")
        && check(std::ranges::equal(first.getQuads(), second.getQuads()),
            "same settings produce the same topology");
}

bool roomGenerationIsRepeatable(const stalberg::StalbergGrid& grid)
{
    stalberg::RoomLayout first;
    stalberg::RoomLayout second;
    first.generate(grid, 17);
    second.generate(grid, 17);

    return check(first.getRoomCount() == second.getRoomCount(),
               "same room seed produces the same room count")
        && check(std::ranges::equal(
                     first.getCellAssignments(), second.getCellAssignments()),
            "same room seed produces the same layout");
}

bool roomLayoutIsValid(
    const stalberg::StalbergGrid& grid, std::uint32_t roomSeed)
{
    stalberg::RoomLayout layout;
    layout.generate(grid, roomSeed);
    const auto assignments = layout.getCellAssignments();
    const auto vertices = grid.getVertices();
    const auto neighbors = grid.getNeighbors();
    bool valid = true;

    valid &= check(assignments.size() == vertices.size(),
        "room layout has one assignment per grid vertex");
    valid &= check(layout.getRoomCount() >= 4,
        "room generation creates multiple rooms");
    valid &= check(layout.getCorridorCellCount() > 0,
        "room generation creates corridors");

    std::size_t corridorCount = 0;
    std::size_t floorCellCount = 0;
    std::size_t buildableCellCount = 0;
    for (std::size_t cell = 0; cell < assignments.size(); ++cell) {
        if (vertices[cell].fixed) {
            valid &= check(assignments[cell] == stalberg::EMPTY_CELL,
                "fixed boundary cells remain outside the floor plan");
        } else {
            ++buildableCellCount;
        }
        if (assignments[cell] != stalberg::EMPTY_CELL) {
            ++floorCellCount;
        }
        if (assignments[cell] == stalberg::CORRIDOR_CELL) {
            ++corridorCount;
        }
    }
    valid &= check(floorCellCount * 100 <= buildableCellCount * 60,
        "room generation leaves substantial intentional negative space");
    valid &= check(corridorCount == layout.getCorridorCellCount(),
        "reported corridor count matches assignments");

    std::vector<bool> visited(assignments.size(), false);
    const auto floorStart = std::ranges::find_if(assignments, [](int assignment) {
        return assignment != stalberg::EMPTY_CELL;
    });
    if (floorStart != assignments.end()) {
        std::queue<std::size_t> queue;
        queue.push(static_cast<std::size_t>(floorStart - assignments.begin()));
        visited[queue.front()] = true;
        std::size_t reached = 0;
        while (!queue.empty()) {
            const std::size_t cell = queue.front();
            queue.pop();
            ++reached;
            for (const std::size_t neighbor : neighbors[cell]) {
                if (!visited[neighbor]
                    && assignments[neighbor] != stalberg::EMPTY_CELL) {
                    visited[neighbor] = true;
                    queue.push(neighbor);
                }
            }
        }
        valid &= check(reached == floorCellCount,
            "center-out floor plan remains connected");
    }

    const std::size_t corridorRegion = layout.getRoomCount();
    std::vector<std::size_t> doorwayCounts(layout.getRoomCount(), 0);
    std::vector<std::vector<std::size_t>> regionConnections(
        layout.getRoomCount() + 1);
    const auto regionIndex = [&](int region) {
        return region == stalberg::CORRIDOR_CELL
            ? corridorRegion
            : static_cast<std::size_t>(region);
    };
    const auto validRegion = [&](int region) {
        return region == stalberg::CORRIDOR_CELL
            || (region >= 0
                && static_cast<std::size_t>(region) < layout.getRoomCount());
    };

    for (const stalberg::Doorway& doorway : layout.getDoorways()) {
        valid &= check(validRegion(doorway.firstRegion)
                && validRegion(doorway.secondRegion),
            "doorway references valid regions");
        valid &= check(doorway.firstCell < assignments.size()
                && assignments[doorway.firstCell] == doorway.firstRegion,
            "doorway first cell matches its region");
        valid &= check(doorway.secondCell < assignments.size()
                && assignments[doorway.secondCell] == doorway.secondRegion,
            "doorway second cell matches its region");
        if (doorway.firstCell < neighbors.size()) {
            valid &= check(std::ranges::find(
                    neighbors[doorway.firstCell], doorway.secondCell)
                    != neighbors[doorway.firstCell].end(),
                "doorway cells share an edge");
        }
        if (!validRegion(doorway.firstRegion)
            || !validRegion(doorway.secondRegion)) {
            continue;
        }

        const std::size_t first = regionIndex(doorway.firstRegion);
        const std::size_t second = regionIndex(doorway.secondRegion);
        regionConnections[first].push_back(second);
        regionConnections[second].push_back(first);
        if (doorway.firstRegion >= 0) {
            ++doorwayCounts[first];
        }
        if (doorway.secondRegion >= 0) {
            ++doorwayCounts[second];
        }
    }

    std::vector<bool> reachedRegions(regionConnections.size(), false);
    std::queue<std::size_t> regionQueue;
    regionQueue.push(corridorCount > 0 ? corridorRegion : 0);
    reachedRegions[regionQueue.front()] = true;
    while (!regionQueue.empty()) {
        const std::size_t region = regionQueue.front();
        regionQueue.pop();
        for (const std::size_t connected : regionConnections[region]) {
            if (!reachedRegions[connected]) {
                reachedRegions[connected] = true;
                regionQueue.push(connected);
            }
        }
    }
    for (std::size_t room = 0; room < layout.getRoomCount(); ++room) {
        valid &= check(reachedRegions[room],
            "doorways connect every room into one circulation graph");
    }

    for (const stalberg::GeneratedRoom& room : layout.getRooms()) {
        valid &= check(doorwayCounts[static_cast<std::size_t>(room.id)] > 0,
            "every room has at least one doorway");
        const auto roomStart = std::ranges::find(assignments, room.id);
        valid &= check(roomStart != assignments.end(), "reported room has cells");
        if (roomStart == assignments.end()) {
            continue;
        }

        std::fill(visited.begin(), visited.end(), false);
        std::queue<std::size_t> queue;
        queue.push(static_cast<std::size_t>(roomStart - assignments.begin()));
        visited[queue.front()] = true;
        std::size_t reached = 0;
        while (!queue.empty()) {
            const std::size_t cell = queue.front();
            queue.pop();
            ++reached;
            for (const std::size_t neighbor : neighbors[cell]) {
                if (!visited[neighbor] && assignments[neighbor] == room.id) {
                    visited[neighbor] = true;
                    queue.push(neighbor);
                }
            }
        }
        valid &= check(reached == room.cellCount, "every room is connected");
    }

    return valid;
}

bool relaxationPreservesBoundary()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);

    std::vector<stalberg::Point> boundaryPositions(grid.getVertexCount());
    for (std::size_t i = 0; i < grid.getVertices().size(); ++i) {
        if (grid.getVertices()[i].fixed) {
            boundaryPositions[i] = grid.getVertices()[i].position;
        }
    }

    for (int step = 0; step < stalberg::MAX_RELAXATION_STEPS + 1; ++step) {
        grid.relaxOnce();
    }

    bool valid = check(grid.getRelaxationSteps() == stalberg::MAX_RELAXATION_STEPS,
        "relaxation stops at the configured step limit");
    for (std::size_t i = 0; i < grid.getVertices().size(); ++i) {
        if (grid.getVertices()[i].fixed) {
            valid &= check(grid.getVertices()[i].position == boundaryPositions[i],
                "fixed boundary vertex does not move");
        }
    }
    return valid;
}

} // namespace

int main()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);

    bool valid = true;
    valid &= check(grid.getVertexCount() > 0, "generation creates vertices");
    valid &= check(grid.getQuadCount() == 460, "radius 6, seed 1 produces 460 quads");
    valid &= topologyIsValid(grid);
    valid &= generationIsRepeatable();
    valid &= roomGenerationIsRepeatable(grid);
    for (std::uint32_t roomSeed = 1; roomSeed <= 12; ++roomSeed) {
        valid &= roomLayoutIsValid(grid, roomSeed);
    }
    valid &= relaxationPreservesBoundary();

    if (!valid) {
        return 1;
    }

    std::cout << "All grid tests passed\n";
    return 0;
}
