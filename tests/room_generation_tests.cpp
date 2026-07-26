#include "grid/stalberg_grid.hpp"
#include "integration/room_grid_adapter.hpp"
#include "rooms/room_generator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numbers>
#include <queue>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

stalberg::rooms::RoomLayout generateRooms(const stalberg::StalbergGrid& grid,
    std::uint32_t seed,
    stalberg::rooms::RoomGenerationMethod method
        = stalberg::rooms::RoomGenerationMethod::BranchingShapes)
{
    return stalberg::rooms::RoomGenerator {}.generate(
        stalberg::makeRoomGrid(grid), seed, method);
}

bool adapterProducesValidRoomInput(const stalberg::StalbergGrid& grid)
{
    const auto input = stalberg::makeRoomGrid(grid);
    bool valid = check(input.cells.size() == grid.getVertexCount(),
        "adapter preserves the grid cell count");
    valid &= check(input.neighbors.size() == grid.getNeighbors().size(),
        "adapter preserves one adjacency list per cell");
    for (std::size_t cell = 0; cell < input.cells.size(); ++cell) {
        valid &= check(input.cells[cell].buildable == !grid.getVertices()[cell].fixed,
            "adapter maps relaxation boundaries to room buildability");
        valid &= check(std::ranges::equal(
                input.neighbors[cell], grid.getNeighbors()[cell]),
            "adapter preserves cell adjacency");
    }
    valid &= check(input.entranceCandidates.size() == 6,
        "adapter supplies one candidate for each hex side");
    for (std::size_t index = 0; index < input.entranceCandidates.size(); ++index) {
        const auto entrance = input.entranceCandidates[index];
        valid &= check(entrance < grid.getVertexCount()
                && grid.getVertices()[entrance].fixed,
            "adapter entrance candidates belong to the grid boundary");
        valid &= check(std::ranges::find(
                input.entranceCandidates.begin(),
                input.entranceCandidates.begin() + static_cast<std::ptrdiff_t>(index),
                entrance)
                == input.entranceCandidates.begin() + static_cast<std::ptrdiff_t>(index),
            "adapter entrance candidates are unique");
    }
    return valid;
}

bool roomGenerationIsRepeatable(const stalberg::StalbergGrid& grid,
    stalberg::rooms::RoomGenerationMethod method)
{
    const auto first = generateRooms(grid, 17, method);
    const auto second = generateRooms(grid, 17, method);

    return check(first.getMethod() == method,
               "layout records its generation method")
        && check(first.getRoomCount() == second.getRoomCount(),
            "same room seed produces the same room count")
        && check(std::ranges::equal(
                     first.getCellAssignments(), second.getCellAssignments()),
            "same room seed and method produce the same layout")
        && check(std::ranges::equal(first.getConnectedEntrances(),
                     second.getConnectedEntrances()),
            "same room seed and method connect the same entrances");
}

bool roomInputIsIndependentFromLaterRelaxation()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);
    const auto input = stalberg::makeRoomGrid(grid);
    const auto before = stalberg::rooms::RoomGenerator {}.generate(input, 17);

    for (int step = 0; step < stalberg::MAX_RELAXATION_STEPS; ++step) {
        grid.relaxOnce();
    }
    const auto after = stalberg::rooms::RoomGenerator {}.generate(input, 17);

    return check(std::ranges::equal(
                     before.getCellAssignments(), after.getCellAssignments()),
        "later grid relaxation cannot mutate an owned room input");
}

bool roomLayoutIsValid(const stalberg::StalbergGrid& grid,
    std::uint32_t roomSeed,
    stalberg::rooms::RoomGenerationMethod method
        = stalberg::rooms::RoomGenerationMethod::BranchingShapes)
{
    const auto roomGrid = stalberg::makeRoomGrid(grid);
    const auto layout
        = stalberg::rooms::RoomGenerator {}.generate(roomGrid, roomSeed, method);
    const auto assignments = layout.getCellAssignments();
    const auto vertices = grid.getVertices();
    const auto neighbors = roomGrid.getNeighbors();
    const auto connectedEntrances = layout.getConnectedEntrances();
    bool valid = true;

    valid &= check(layout.getMethod() == method,
        "layout reports the selected generation method");
    valid &= check(assignments.size() == vertices.size(),
        "room layout has one assignment per grid vertex");
    valid &= check(layout.getRoomCount() >= 2,
        "room generation creates multiple rooms");
    valid &= check(layout.getRoomCount() <= stalberg::rooms::MAX_GENERATED_ROOMS,
        "every generated room has a unique renderer color");
    valid &= check(connectedEntrances.size() >= 4,
        "rooms connect to the centers of at least four outer edges");
    for (std::size_t index = 0; index < connectedEntrances.size(); ++index) {
        const std::size_t cell = connectedEntrances[index];
        valid &= check(cell < assignments.size() && vertices[cell].fixed,
            "edge connection references a boundary center");
        valid &= check(std::ranges::find(
                roomGrid.entranceCandidates, cell)
                != roomGrid.entranceCandidates.end(),
            "edge connection uses an adapter-supplied entrance candidate");
        valid &= check(cell < assignments.size()
                && assignments[cell] != stalberg::rooms::EMPTY_CELL,
            "each connected entrance belongs to the connected floor plan");
        valid &= check(std::ranges::find(
                connectedEntrances.begin(),
                connectedEntrances.begin() + static_cast<std::ptrdiff_t>(index),
                cell)
                == connectedEntrances.begin() + static_cast<std::ptrdiff_t>(index),
            "connected entrances are unique");
    }

    std::size_t floorCellCount = 0;
    std::size_t buildableCellCount = 0;
    for (std::size_t cell = 0; cell < assignments.size(); ++cell) {
        if (vertices[cell].fixed) {
            const bool isConnectedEntrance = std::ranges::find(
                connectedEntrances, cell) != connectedEntrances.end();
            valid &= check(
                isConnectedEntrance
                    == (assignments[cell] != stalberg::rooms::EMPTY_CELL),
                "only connected entrances become boundary floor cells");
        } else {
            ++buildableCellCount;
        }
        if (assignments[cell] != stalberg::rooms::EMPTY_CELL) {
            ++floorCellCount;
            valid &= check(assignments[cell] >= 0,
                "every generated floor cell belongs to a room");
        }
    }
    if (buildableCellCount >= 100) {
        valid &= check(floorCellCount * 100 <= buildableCellCount * 60,
            "room generation leaves substantial intentional negative space");
    }
    std::vector<bool> visited(assignments.size(), false);
    const auto floorStart = std::ranges::find_if(assignments, [](int assignment) {
        return assignment != stalberg::rooms::EMPTY_CELL;
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
                    && assignments[neighbor] != stalberg::rooms::EMPTY_CELL) {
                    visited[neighbor] = true;
                    queue.push(neighbor);
                }
            }
        }
        valid &= check(reached == floorCellCount,
            "center-out floor plan remains connected");
    }

    std::vector<std::size_t> doorwayCounts(layout.getRoomCount(), 0);
    std::vector<std::vector<std::size_t>> regionConnections(
        layout.getRoomCount());
    const auto validRegion = [&](int region) {
        return region >= 0
            && static_cast<std::size_t>(region) < layout.getRoomCount();
    };

    for (const stalberg::rooms::Doorway& doorway : layout.getDoorways()) {
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

        const std::size_t first = static_cast<std::size_t>(doorway.firstRegion);
        const std::size_t second = static_cast<std::size_t>(doorway.secondRegion);
        regionConnections[first].push_back(second);
        regionConnections[second].push_back(first);
        ++doorwayCounts[first];
        ++doorwayCounts[second];
    }

    std::vector<bool> reachedRegions(regionConnections.size(), false);
    std::queue<std::size_t> regionQueue;
    regionQueue.push(0);
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

    for (const stalberg::rooms::GeneratedRoom& room : layout.getRooms()) {
        valid &= check(room.cellCount > 1, "single-cell rooms are never generated");
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

std::vector<int> connectedSideSignature(
    const stalberg::StalbergGrid& grid, const stalberg::rooms::RoomLayout& layout)
{
    std::vector<int> signature;
    for (const stalberg::VertexIndex cell : layout.getConnectedEntrances()) {
        const stalberg::Point point = grid.getVertices()[cell].position;
        int selectedSide = 0;
        float bestProjection = -1.0F;
        for (int side = 0; side < 6; ++side) {
            const float angle = -std::numbers::pi_v<float> * 0.5F
                + static_cast<float>(side) * std::numbers::pi_v<float> / 3.0F;
            const float projection = point.x * std::cos(angle)
                + point.y * std::sin(angle);
            if (projection > bestProjection) {
                selectedSide = side;
                bestProjection = projection;
            }
        }
        signature.push_back(selectedSide);
    }
    std::ranges::sort(signature);
    return signature;
}

bool organicGrowthIsEvenlyDispersed(const stalberg::StalbergGrid& grid)
{
    const auto input = stalberg::makeRoomGrid(grid);
    bool valid = true;
    for (std::uint32_t seed = 1; seed <= 8; ++seed) {
        const auto layout = stalberg::rooms::RoomGenerator {}.generate(input,
            seed,
            stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
        std::array<std::size_t, 6> sectorCounts {};
        for (std::size_t cell = 0; cell < input.cells.size(); ++cell) {
            if (layout.getCellAssignment(cell) == stalberg::rooms::EMPTY_CELL) {
                continue;
            }
            float angle = std::atan2(
                input.cells[cell].position.y, input.cells[cell].position.x);
            if (angle < 0.0F) {
                angle += 2.0F * std::numbers::pi_v<float>;
            }
            const std::size_t sector = std::min(
                static_cast<std::size_t>(angle
                    / (2.0F * std::numbers::pi_v<float>) * 6.0F),
                sectorCounts.size() - 1);
            ++sectorCounts[sector];
        }
        const auto [minimum, maximum]
            = std::ranges::minmax_element(sectorCounts);
        valid &= check(*minimum * 3 >= *maximum * 2,
            "organic growth remains dispersed across all six sectors");
    }
    return valid;
}

bool organicGrowthCanUseEveryEntranceSet()
{
    stalberg::StalbergGrid grid;
    grid.generate(3, 1);
    const auto input = stalberg::makeRoomGrid(grid);
    std::set<std::vector<stalberg::rooms::CellIndex>> entranceSets;
    for (std::uint32_t seed = 1; seed <= 180; ++seed) {
        const auto layout = stalberg::rooms::RoomGenerator {}.generate(input,
            seed,
            stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
        std::vector<stalberg::rooms::CellIndex> entrances(
            layout.getConnectedEntrances().begin(),
            layout.getConnectedEntrances().end());
        std::ranges::sort(entrances);
        entranceSets.insert(std::move(entrances));
    }
    return check(entranceSets.size() == 15,
        "organic growth can select every four-of-six entrance set");
}

bool compactLayoutsVaryAcrossGridSeeds()
{
    std::set<std::vector<int>> signatures;
    for (std::uint32_t gridSeed = 40; gridSeed < 48; ++gridSeed) {
        stalberg::StalbergGrid grid;
        grid.generate(3, gridSeed);
        const auto layout = generateRooms(grid, 1);
        signatures.insert(connectedSideSignature(grid, layout));
    }
    return check(signatures.size() >= 4,
        "compact layouts vary their outer connections across grid seeds");
}

bool largeLayoutUsesExpandedRoomBudget()
{
    stalberg::StalbergGrid grid;
    grid.generate(14, 1);
    const auto layout = generateRooms(grid, 1);
    return check(layout.getRoomCount() == stalberg::rooms::MAX_GENERATED_ROOMS,
        "large layouts can generate all 64 rooms");
}

bool degenerateGridDoesNotCreateSingleCellRoom()
{
    stalberg::StalbergGrid grid;
    grid.generate(0, 1);
    const auto layout = generateRooms(grid, 1);

    return check(layout.getRoomCount() == 0,
               "a one-cell grid does not publish a single-cell room")
        && check(std::ranges::all_of(layout.getCellAssignments(), [](int assignment) {
            return assignment == stalberg::rooms::EMPTY_CELL;
        }), "a one-cell grid remains unassigned");
}

bool invalidNeutralTopologyIsRejected()
{
    stalberg::rooms::RoomGrid grid;
    grid.cells.resize(2);
    grid.neighbors = { { 1 } };

    const auto malformedAdjacency
        = stalberg::rooms::RoomGenerator {}.generate(grid, 1);
    bool valid = check(malformedAdjacency.getRoomCount() == 0,
        "malformed adjacency does not generate rooms");
    valid &= check(malformedAdjacency.getCellAssignments().size() == grid.cells.size(),
        "invalid neutral topology still returns aligned assignments");

    grid.neighbors = { { 1 }, { 0 } };
    grid.entranceCandidates = { 2 };
    const auto malformedEntrance
        = stalberg::rooms::RoomGenerator {}.generate(grid, 1);
    valid &= check(malformedEntrance.getRoomCount() == 0,
        "out-of-range entrances do not generate rooms");
    return valid;
}

} // namespace

int main()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);

    bool valid = adapterProducesValidRoomInput(grid);
    valid &= roomGenerationIsRepeatable(
        grid, stalberg::rooms::RoomGenerationMethod::BranchingShapes);
    valid &= roomGenerationIsRepeatable(
        grid, stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
    valid &= roomInputIsIndependentFromLaterRelaxation();
    for (std::uint32_t roomSeed = 1; roomSeed <= 12; ++roomSeed) {
        valid &= roomLayoutIsValid(grid, roomSeed);
        valid &= roomLayoutIsValid(grid,
            roomSeed,
            stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
    }
    for (const int radius : std::array { 2, 7, 14 }) {
        stalberg::StalbergGrid representativeGrid;
        representativeGrid.generate(radius, 3);
        for (const std::uint32_t roomSeed : std::array<std::uint32_t, 2> { 1, 7 }) {
            valid &= roomLayoutIsValid(representativeGrid, roomSeed);
            valid &= roomLayoutIsValid(representativeGrid,
                roomSeed,
                stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
        }
    }
    valid &= organicGrowthIsEvenlyDispersed(grid);
    valid &= organicGrowthCanUseEveryEntranceSet();
    valid &= compactLayoutsVaryAcrossGridSeeds();
    valid &= largeLayoutUsesExpandedRoomBudget();
    valid &= degenerateGridDoesNotCreateSingleCellRoom();
    valid &= invalidNeutralTopologyIsRejected();

    if (!valid) {
        return 1;
    }

    std::cout << "All room generation tests passed\n";
    return 0;
}
