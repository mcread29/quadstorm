#include "grid/dual_grid.hpp"
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
#include <stdexcept>
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
    valid &= check(input.connections.size() == grid.getNeighbors().size(),
        "adapter provides one canonical connection list per cell");
    for (std::size_t cell = 0; cell < input.cells.size(); ++cell) {
        valid &= check(input.cells[cell].buildable == !grid.getVertices()[cell].fixed,
            "adapter maps relaxation boundaries to room buildability");
        std::vector<std::size_t> expected(
            grid.getNeighbors()[cell].begin(), grid.getNeighbors()[cell].end());
        std::ranges::sort(expected);
        valid &= check(std::ranges::equal(input.neighbors(cell), expected),
            "adapter preserves cell adjacency as a canonical projection");
        valid &= check(input.cells[cell].area > 0.0F
                && input.cells[cell].clearance >= 0.0F
                && (!input.cells[cell].buildable
                    || input.cells[cell].clearance > 0.0F),
            "adapter measures usable dual-cell geometry");
        valid &= check(input.connections[cell].size()
                == static_cast<std::size_t>(
                    std::ranges::distance(input.neighbors(cell))),
            "every neighbor projects from physical connection metadata");
        for (const auto& physical : input.connections[cell]) {
            valid &= check(physical.distance > 0.0F
                    && physical.sharedBoundaryLength > 0.0F,
                "physical connections are positive and aligned with topology");
        }
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
    bool sameDoorways = first.getDoorways().size() == second.getDoorways().size();
    for (std::size_t index = 0;
         sameDoorways && index < first.getDoorways().size(); ++index) {
        const auto& lhs = first.getDoorways()[index];
        const auto& rhs = second.getDoorways()[index];
        sameDoorways = lhs.firstRegion == rhs.firstRegion
            && lhs.firstCell == rhs.firstCell
            && lhs.secondRegion == rhs.secondRegion
            && lhs.secondCell == rhs.secondCell
            && lhs.width == rhs.width
            && lhs.quality == rhs.quality;
    }
    bool sameRoomMetadata = first.getRooms().size() == second.getRooms().size();
    for (std::size_t index = 0;
         sameRoomMetadata && index < first.getRooms().size(); ++index) {
        const auto& lhs = first.getRooms()[index];
        const auto& rhs = second.getRooms()[index];
        sameRoomMetadata = lhs.id == rhs.id
            && lhs.cellCount == rhs.cellCount
            && lhs.area == rhs.area
            && lhs.role == rhs.role
            && lhs.coverCandidates == rhs.coverCandidates
            && lhs.enemySpawnCandidates == rhs.enemySpawnCandidates;
    }

    return check(first.getMethod() == method,
               "layout records its generation method")
        && check(first.getRoomCount() == second.getRoomCount(),
            "same room seed produces the same room count")
        && check(first.getSelectedCandidate() == second.getSelectedCandidate()
                && first.getQualityScore() == second.getQualityScore()
                && first.hasSmallMapRecipe() == second.hasSmallMapRecipe()
                && first.getSmallMapRecipe() == second.getSmallMapRecipe(),
            "best-of-N selection and recipe selection are deterministic")
        && check(std::ranges::equal(
                     first.getCellAssignments(), second.getCellAssignments()),
            "same room seed and method produce the same layout")
        && check(sameDoorways, "same room seed produces the same circulation graph")
        && check(sameRoomMetadata, "same room seed produces the same room metadata")
        && check(std::ranges::equal(first.getConnectedEntrances(),
                     second.getConnectedEntrances()),
            "same room seed and method connect the same entrances");
}

std::set<std::pair<int, int>> contractedArenaEdges(
    const stalberg::rooms::RoomLayout& layout)
{
    const auto rooms = layout.getRooms();
    std::set<std::pair<int, int>> result;
    std::vector<std::vector<int>> connectorNeighbors(rooms.size());
    for (const auto& doorway : layout.getDoorways()) {
        const bool firstConnector = rooms[static_cast<std::size_t>(
            doorway.firstRegion)].role == stalberg::rooms::RoomRole::Connector;
        const bool secondConnector = rooms[static_cast<std::size_t>(
            doorway.secondRegion)].role == stalberg::rooms::RoomRole::Connector;
        if (!firstConnector && !secondConnector) {
            result.insert(std::minmax(
                doorway.firstRegion, doorway.secondRegion));
        } else if (firstConnector != secondConnector) {
            const int connector = firstConnector
                ? doorway.firstRegion : doorway.secondRegion;
            const int arena = firstConnector
                ? doorway.secondRegion : doorway.firstRegion;
            connectorNeighbors[static_cast<std::size_t>(connector)]
                .push_back(arena);
        }
    }
    for (const auto& neighbors : connectorNeighbors) {
        if (neighbors.size() == 2) {
            result.insert(std::minmax(neighbors[0], neighbors[1]));
        }
    }
    return result;
}

bool smallMapRecipesPublishDistinctIntent()
{
    stalberg::StalbergGrid grid;
    grid.generate(5, 1);
    grid.relaxToCompletion();
    const auto input = stalberg::makeRoomGrid(grid);
    const stalberg::rooms::RoomGenerator generator;
    const std::array recipes {
        stalberg::rooms::SmallMapRecipe::HubCircuit,
        stalberg::rooms::SmallMapRecipe::BrokenRing,
        stalberg::rooms::SmallMapRecipe::TwinWings
    };
    std::vector<std::set<std::pair<int, int>>> signatures;
    bool valid = true;
    for (std::size_t index = 0; index < recipes.size(); ++index) {
        const auto layout = generator.generate(input,
            static_cast<std::uint32_t>(index + 1),
            stalberg::rooms::RoomGenerationOptions {
                .method = stalberg::rooms::RoomGenerationMethod::ShooterLayout,
                .candidateCount = 12,
                .smallMapRecipe = recipes[index]
            });
        valid &= check(layout.getRoomCount() > 0
                && layout.hasSmallMapRecipe()
                && layout.getSmallMapRecipe() == recipes[index],
            "every small-map recipe is selected before candidate generation");
        const std::size_t substantialRooms = static_cast<std::size_t>(
            std::ranges::count_if(layout.getRooms(), [](const auto& room) {
                return room.role != stalberg::rooms::RoomRole::Connector;
            }));
        valid &= check(substantialRooms == 5,
            "radius-5 puzzle maps contain five intentional substantial rooms");
        valid &= check(std::ranges::count(layout.getRooms(),
                           stalberg::rooms::RoomRole::Hub,
                           &stalberg::rooms::GeneratedRoom::role)
                == 1
                && std::ranges::count(layout.getRooms(),
                       stalberg::rooms::RoomRole::Reward,
                       &stalberg::rooms::GeneratedRoom::role)
                    == 1,
            "small recipes publish one Hub and one Reward room");
        const auto signature = contractedArenaEdges(layout);
        std::set<std::pair<int, int>> requiredEdges;
        if (recipes[index] == stalberg::rooms::SmallMapRecipe::HubCircuit
            || recipes[index] == stalberg::rooms::SmallMapRecipe::TwinWings) {
            requiredEdges = { { 0, 2 }, { 1, 2 }, { 2, 3 }, { 2, 4 } };
        } else {
            requiredEdges = { { 0, 2 }, { 2, 3 }, { 1, 3 }, { 2, 4 } };
        }
        valid &= check(std::ranges::all_of(requiredEdges,
                           [&](const auto& edge) {
                               return signature.contains(edge);
                           }),
            "each recipe materializes its exact required semantic edges");
        valid &= check(recipes[index]
                    != stalberg::rooms::SmallMapRecipe::HubCircuit
                || signature == requiredEdges,
            "Hub Circuit has one distinct doorway branch per semantic room");
        valid &= check(std::ranges::count_if(signature,
                           [](const auto& edge) {
                               return edge.first == 4 || edge.second == 4;
                           })
                == 1
                && signature.contains({ 2, 4 }),
            "every small recipe keeps Reward as an optional Hub leaf");
        signatures.push_back(signature);
    }
    valid &= check(signatures.size() == 3
            && signatures[0] != signatures[1]
            && signatures[0] != signatures[2]
            && signatures[1] != signatures[2],
        "Hub Circuit, Broken Ring, and Twin Wings publish distinct graphs");
    return valid;
}

bool shooterLayoutHasExplicitCombatStructure(const stalberg::StalbergGrid& grid)
{
    const auto layout = generateRooms(
        grid, 11, stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    if (layout.getRoomCount() == 0) {
        return check(false, "shooter layout produces a playable mission graph");
    }
    std::size_t arenaCount = 0;
    std::size_t connectorCount = 0;
    float arenaArea = 0.0F;
    float connectorArea = 0.0F;
    std::vector<std::size_t> degrees(layout.getRoomCount(), 0);
    std::vector<std::vector<int>> roomGraph(layout.getRoomCount());
    int startRoom = -1;
    int exitRoom = -1;
    bool valid = true;
    for (const auto& room : layout.getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Start) {
            startRoom = room.id;
        } else if (room.role == stalberg::rooms::RoomRole::Exit) {
            exitRoom = room.id;
        }
        if (room.role == stalberg::rooms::RoomRole::Connector) {
            ++connectorCount;
            connectorArea += room.area;
        } else {
            ++arenaCount;
            arenaArea += room.area;
        }
    }
    for (const auto& doorway : layout.getDoorways()) {
        const bool firstConnector = layout.getRooms()[
            static_cast<std::size_t>(doorway.firstRegion)].role
            == stalberg::rooms::RoomRole::Connector;
        const bool secondConnector = layout.getRooms()[
            static_cast<std::size_t>(doorway.secondRegion)].role
            == stalberg::rooms::RoomRole::Connector;
        valid &= check(!(firstConnector && secondConnector),
            "shooter corridors never connect directly to other corridors");
        ++degrees[static_cast<std::size_t>(doorway.firstRegion)];
        ++degrees[static_cast<std::size_t>(doorway.secondRegion)];
        roomGraph[static_cast<std::size_t>(doorway.firstRegion)]
            .push_back(doorway.secondRegion);
        roomGraph[static_cast<std::size_t>(doorway.secondRegion)]
            .push_back(doorway.firstRegion);
    }
    for (const auto& room : layout.getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Connector) {
            valid &= check(degrees[static_cast<std::size_t>(room.id)] >= 1
                    && degrees[static_cast<std::size_t>(room.id)] <= 2,
                "shooter corridors are explicit one- or two-ended passages");
        }
    }
    valid &= check(arenaCount >= 3 && connectorCount >= 1,
        "shooter layout creates multiple arenas and an explicit corridor");
    valid &= check(arenaArea / static_cast<float>(std::max<std::size_t>(arenaCount, 1))
            > connectorArea
                / static_cast<float>(std::max<std::size_t>(connectorCount, 1)),
        "shooter arenas are physically larger than their connectors");
    const std::size_t loopCount = layout.getDoorways().size()
        - layout.getRoomCount() + 1;
    valid &= check(loopCount <= 1,
        "shooter mission graph has at most one deliberate route loop");
    std::vector<int> distances(layout.getRoomCount(), -1);
    std::queue<int> queue;
    if (startRoom >= 0) {
        distances[static_cast<std::size_t>(startRoom)] = 0;
        queue.push(startRoom);
    }
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
    valid &= check(exitRoom >= 0
            && distances[static_cast<std::size_t>(exitRoom)] >= 3,
        "shooter start and exit are separated by a meaningful main route");
    return valid;
}

bool largeShooterLayoutUsesDirectArenaLinks()
{
    stalberg::StalbergGrid grid;
    grid.generate(14, 1);
    grid.relaxToCompletion();
    const auto layout = generateRooms(
        grid, 2, stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    if (layout.getRoomCount() == 0) {
        return check(false, "large shooter layout produces a playable mission graph");
    }

    std::size_t arenaCount = 0;
    std::size_t connectorCount = 0;
    std::size_t twoCellConnectorCount = 0;
    std::size_t directArenaLinks = 0;
    for (const auto& room : layout.getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Connector) {
            ++connectorCount;
            if (room.cellCount == 2) {
                ++twoCellConnectorCount;
            }
        } else {
            ++arenaCount;
        }
    }
    for (const auto& doorway : layout.getDoorways()) {
        const auto firstRole = layout.getRooms()[
            static_cast<std::size_t>(doorway.firstRegion)].role;
        const auto secondRole = layout.getRooms()[
            static_cast<std::size_t>(doorway.secondRegion)].role;
        if (firstRole != stalberg::rooms::RoomRole::Connector
            && secondRole != stalberg::rooms::RoomRole::Connector) {
            ++directArenaLinks;
        }
    }

    return check(!layout.hasSmallMapRecipe(),
               "large shooter layouts do not misreport a small-map recipe")
        && check(directArenaLinks > 0,
            "large shooter layouts turn short routes into direct arena links")
        && check(connectorCount <= arenaCount / 2,
            "large shooter layouts reserve connector rooms for long links")
        && check(twoCellConnectorCount <= 1,
            "large shooter layouts publish at most one structural two-cell connector");
}

bool shooterLayoutsCanCreateDenseAreas()
{
    stalberg::StalbergGrid grid;
    grid.generate(14, 1);
    grid.relaxToCompletion();
    const auto input = stalberg::makeRoomGrid(grid);
    const stalberg::rooms::RoomGenerator generator;
    const auto landmark = generator.generate(input,
        2,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    const auto cluster = generator.generate(input,
        1,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    if (landmark.getRoomCount() == 0 || cluster.getRoomCount() == 0) {
        return check(false,
            "large shooter maps can realize landmark and clustered arena briefs");
    }

    std::vector<std::size_t> landmarkSizes;
    for (const auto& room : landmark.getRooms()) {
        if (room.role != stalberg::rooms::RoomRole::Connector) {
            landmarkSizes.push_back(room.cellCount);
        }
    }
    std::ranges::sort(landmarkSizes);
    const std::size_t landmarkMedian
        = landmarkSizes[landmarkSizes.size() / 2];
    bool valid = check(landmarkSizes.back() * 4 >= landmarkMedian * 7,
        "some shooter maps contain one substantially larger landmark arena");

    float cellScale = 0.0F;
    std::size_t measuredConnections = 0;
    for (std::size_t cell = 0; cell < input.connections.size(); ++cell) {
        for (const auto& connection : input.connections[cell]) {
            if (connection.cell > cell && input.cells[cell].buildable
                && input.cells[connection.cell].buildable) {
                cellScale += connection.distance;
                ++measuredConnections;
            }
        }
    }
    cellScale /= static_cast<float>(measuredConnections);

    std::vector<stalberg::rooms::CellPoint> centroids(cluster.getRoomCount());
    std::vector<std::size_t> centroidCounts(cluster.getRoomCount(), 0);
    for (std::size_t cell = 0; cell < input.cells.size(); ++cell) {
        const int room = cluster.getCellAssignment(cell);
        if (room < 0) {
            continue;
        }
        auto& centroid = centroids[static_cast<std::size_t>(room)];
        centroid.x += input.cells[cell].position.x;
        centroid.y += input.cells[cell].position.y;
        ++centroidCounts[static_cast<std::size_t>(room)];
    }
    for (std::size_t room = 0; room < centroids.size(); ++room) {
        centroids[room].x /= static_cast<float>(centroidCounts[room]);
        centroids[room].y /= static_cast<float>(centroidCounts[room]);
    }

    std::vector<std::size_t> clusterArenaSizes;
    for (const auto& room : cluster.getRooms()) {
        if (room.role != stalberg::rooms::RoomRole::Connector) {
            clusterArenaSizes.push_back(room.cellCount);
        }
    }
    std::ranges::sort(clusterArenaSizes);
    const std::size_t clusterMedian
        = clusterArenaSizes[clusterArenaSizes.size() / 2];
    bool foundDenseCluster = false;
    for (const auto& room : cluster.getRooms()) {
        if (room.role == stalberg::rooms::RoomRole::Connector
            || room.cellCount * 4 < clusterMedian * 3) {
            continue;
        }
        std::size_t denseNeighbors = 0;
        for (const auto& doorway : cluster.getDoorways()) {
            int neighbor = -1;
            if (doorway.firstRegion == room.id) {
                neighbor = doorway.secondRegion;
            } else if (doorway.secondRegion == room.id) {
                neighbor = doorway.firstRegion;
            }
            if (neighbor < 0) {
                continue;
            }
            const auto& neighborRoom
                = cluster.getRooms()[static_cast<std::size_t>(neighbor)];
            if (neighborRoom.role == stalberg::rooms::RoomRole::Connector
                || neighborRoom.cellCount * 4 < clusterMedian * 3) {
                continue;
            }
            const auto first = centroids[static_cast<std::size_t>(room.id)];
            const auto second = centroids[static_cast<std::size_t>(neighbor)];
            const float x = second.x - first.x;
            const float y = second.y - first.y;
            if (std::sqrt(x * x + y * y) <= cellScale * 9.0F) {
                ++denseNeighbors;
            }
        }
        if (denseNeighbors >= 2) {
            foundDenseCluster = true;
            break;
        }
    }
    valid &= check(foundDenseCluster,
        "some shooter maps group three substantial nearby arenas into a dense cluster");
    return valid;
}

bool bestOfCandidatesDoesNotReduceQuality(const stalberg::StalbergGrid& grid)
{
    const auto input = stalberg::makeRoomGrid(grid);
    const auto single = stalberg::rooms::RoomGenerator {}.generate(input,
        23,
        stalberg::rooms::RoomGenerationOptions {
            .method = stalberg::rooms::RoomGenerationMethod::OrganicGrowth,
            .candidateCount = 1,
            .smallMapRecipe = {}
        });
    const auto selected = stalberg::rooms::RoomGenerator {}.generate(input,
        23,
        stalberg::rooms::RoomGenerationOptions {
            .method = stalberg::rooms::RoomGenerationMethod::OrganicGrowth,
            .candidateCount = 8,
            .smallMapRecipe = {}
        });
    return check(selected.getQualityScore() >= single.getQualityScore(),
               "best-of-N selection never reduces candidate quality")
        && check(selected.getSelectedCandidate() < 8,
            "selected candidate index stays within the requested budget")
        && check(std::ranges::equal(single.getConnectedEntrances(),
                     selected.getConnectedEntrances()),
            "candidate scoring preserves the seed's entrance brief");
}

bool connectionOrderingDoesNotAffectGeneration(const stalberg::StalbergGrid& grid)
{
    const auto input = stalberg::makeRoomGrid(grid);
    auto permuted = input;
    for (auto& connections : permuted.connections) {
        std::ranges::reverse(connections);
    }
    std::ranges::reverse(permuted.entranceCandidates);
    const auto canonical = stalberg::rooms::RoomGenerator {}.generate(input, 29);
    const auto reordered = stalberg::rooms::RoomGenerator {}.generate(permuted, 29);
    return check(std::ranges::equal(
                     canonical.getCellAssignments(), reordered.getCellAssignments()),
               "neutral connection ordering does not affect generation")
        && check(canonical.getQualityScore() == reordered.getQualityScore(),
            "neutral connection ordering does not affect quality scoring");
}

bool canonicalTopologyFingerprintIsStable()
{
    stalberg::rooms::RoomGrid grid;
    grid.cells = {
        { { 1.0F, -2.0F }, 3.5F, 0.75F, true },
        { { 2.0F, 1.0F }, 4.5F, 1.25F, false },
        { { -3.0F, 0.5F }, 2.25F, 0.5F, true }
    };
    grid.setConnections({
        { { 2, 2.25F, 0.5F }, { 1, 1.25F, 0.75F } },
        { { 0, 1.25F, 0.75F }, { 2, 3.0F, 1.5F } },
        { { 1, 3.0F, 1.5F }, { 0, 2.25F, 0.5F } }
    });
    grid.entranceCandidates = { 2, 0 };

    constexpr std::uint64_t expectedFingerprint = 6343521984407022216ULL;
    const std::uint64_t fingerprint
        = stalberg::rooms::canonicalTopologyFingerprint(grid);
    auto reordered = grid;
    for (auto& connections : reordered.connections) {
        std::ranges::reverse(connections);
    }
    std::ranges::reverse(reordered.entranceCandidates);

    auto changed = grid;
    changed.cells[0].clearance += 0.25F;
    return check(fingerprint == expectedFingerprint,
               "canonical topology fingerprint retains its regression value")
        && check(stalberg::rooms::canonicalTopologyFingerprint(reordered)
                == fingerprint,
            "canonical topology fingerprint ignores neutral input ordering")
        && check(stalberg::rooms::canonicalTopologyFingerprint(changed)
                != fingerprint,
            "canonical topology fingerprint includes generation geometry");
}

bool adapterRejectsMisalignedDualGrid()
{
    stalberg::StalbergGrid grid;
    grid.generate(2, 9);
    const stalberg::DualGrid aligned = stalberg::buildDualGrid(grid);
    bool valid = true;

    const auto isRejected = [&](const stalberg::DualGrid& dual) {
        try {
            static_cast<void>(stalberg::makeRoomGrid(grid, dual));
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    auto missingCell = aligned;
    missingCell.cells.pop_back();
    valid &= check(isRejected(missingCell),
        "adapter rejects a dual grid with a misaligned cell count");

    auto staleCenters = aligned;
    staleCenters.cells[0].center.x += 1.0F;
    valid &= check(isRejected(staleCenters),
        "adapter rejects dual cells from different grid geometry");

    auto missingConnection = aligned;
    missingConnection.connections.pop_back();
    valid &= check(isRejected(missingConnection),
        "adapter rejects a dual grid with different topology");
    return valid;
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
    std::vector<std::vector<stalberg::rooms::CellIndex>> neighbors;
    neighbors.reserve(roomGrid.getCellCount());
    for (stalberg::rooms::CellIndex cell = 0;
         cell < roomGrid.getCellCount(); ++cell) {
        neighbors.emplace_back(
            roomGrid.neighbors(cell).begin(), roomGrid.neighbors(cell).end());
    }
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
    if (layout.getRoomCount() < 2) {
        return false;
    }
    valid &= check(connectedEntrances.size() >= 3,
        "rooms connect to the centers of at least three outer edges");
    valid &= check(connectedEntrances.size() <= 6,
        "rooms connect to no more than six outer edges");
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

    valid &= check(layout.getDoorways().size() >= layout.getRoomCount() - 1
            && layout.getDoorways().size() <= layout.getRoomCount() - 1 + 4,
        "doorway planner creates a connected graph with a bounded loop budget");
    std::vector<std::size_t> doorwayCounts(layout.getRoomCount(), 0);
    std::vector<std::vector<std::size_t>> regionConnections(
        layout.getRoomCount());
    std::set<std::pair<int, int>> doorwayRegionPairs;
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
            const auto physical = std::ranges::find(roomGrid.connections[doorway.firstCell],
                doorway.secondCell,
                &stalberg::rooms::CellConnection::cell);
            valid &= check(physical != roomGrid.connections[doorway.firstCell].end(),
                "doorway cells share a physical edge");
            if (physical != roomGrid.connections[doorway.firstCell].end()) {
                valid &= check(doorway.width == physical->sharedBoundaryLength
                        && doorway.width > 0.0F
                        && doorway.quality > 0.0F
                        && doorway.quality <= 1.0F,
                    "doorway publishes positive physical width and quality");
            }
        }
        valid &= check(doorwayRegionPairs.emplace(
                           std::min(doorway.firstRegion, doorway.secondRegion),
                           std::max(doorway.firstRegion, doorway.secondRegion))
                           .second,
            "each selected room pair has only one doorway");
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

    std::size_t startRoomCount = 0;
    std::size_t exitRoomCount = 0;
    for (const stalberg::rooms::GeneratedRoom& room : layout.getRooms()) {
        valid &= check(room.cellCount > 1, "single-cell rooms are never generated");
        float measuredArea = 0.0F;
        for (std::size_t cell = 0; cell < assignments.size(); ++cell) {
            if (assignments[cell] == room.id) {
                measuredArea += roomGrid.cells[cell].area;
            }
        }
        valid &= check(std::abs(room.area - measuredArea) <= measuredArea * 0.0001F,
            "room metadata reports physical floor area");
        valid &= check(std::ranges::all_of(room.coverCandidates,
                           [&](std::size_t cell) {
                               return cell < assignments.size()
                                   && assignments[cell] == room.id;
                           }),
            "cover candidates belong to their reported room");
        valid &= check(std::ranges::all_of(room.enemySpawnCandidates,
                           [&](std::size_t cell) {
                               return cell < assignments.size()
                                   && assignments[cell] == room.id;
                           }),
            "enemy spawn candidates belong to their reported room");
        startRoomCount += room.role == stalberg::rooms::RoomRole::Start ? 1 : 0;
        exitRoomCount += room.role == stalberg::rooms::RoomRole::Exit ? 1 : 0;
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
    valid &= check(startRoomCount == 1, "layout identifies one start room");
    valid &= check(exitRoomCount == 1, "layout identifies one distinct exit room");
    valid &= check(std::isfinite(layout.getQualityScore())
            && layout.getQualityScore() > 0.0F,
        "selected candidate publishes a finite quality score");

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
        valid &= check(*minimum * 5 >= *maximum * 3,
            "organic growth remains dispersed across all six sectors");
    }
    return valid;
}

bool generationMethodsVaryRoomShapes(const stalberg::StalbergGrid& grid)
{
    const auto input = stalberg::makeRoomGrid(grid);
    bool valid = true;
    for (const auto method : std::array {
             stalberg::rooms::RoomGenerationMethod::BranchingShapes,
             stalberg::rooms::RoomGenerationMethod::OrganicGrowth }) {
        const auto layout
            = stalberg::rooms::RoomGenerator {}.generate(input, 1, method);
        std::vector<float> boundaryRatios;
        for (const auto& room : layout.getRooms()) {
            std::size_t boundaryEdges = 0;
            for (std::size_t cell = 0; cell < input.cells.size(); ++cell) {
                if (layout.getCellAssignment(cell) != room.id) {
                    continue;
                }
                boundaryEdges += std::ranges::count_if(
                    input.neighbors(cell), [&](std::size_t neighbor) {
                        return layout.getCellAssignment(neighbor) != room.id;
                    });
            }
            boundaryRatios.push_back(static_cast<float>(boundaryEdges)
                / std::sqrt(static_cast<float>(room.cellCount)));
        }
        const auto [smallest, largest]
            = std::ranges::minmax_element(boundaryRatios);
        valid &= check(smallest != boundaryRatios.end()
                && *largest >= *smallest * 1.35F,
            "growth profiles produce varied room silhouettes");
    }
    return valid;
}

bool organicGrowthVariesRoomSizes(const stalberg::StalbergGrid& grid)
{
    const auto layout = generateRooms(
        grid, 1, stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
    const auto rooms = layout.getRooms();
    const auto [smallest, largest] = std::ranges::minmax_element(
        rooms, {}, &stalberg::rooms::GeneratedRoom::cellCount);
    return check(smallest != rooms.end()
            && largest->cellCount >= smallest->cellCount * 3,
        "organic growth produces substantial room-size variance");
}

bool organicGrowthCanUseEveryValidEntranceSet()
{
    stalberg::StalbergGrid grid;
    grid.generate(3, 1);
    const auto input = stalberg::makeRoomGrid(grid);
    std::set<std::vector<stalberg::rooms::CellIndex>> entranceSets;
    std::array<std::size_t, 7> countFrequencies {};
    for (std::uint32_t seed = 1; seed <= 600; ++seed) {
        const auto layout = stalberg::rooms::RoomGenerator {}.generate(input,
            seed,
            stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
        std::vector<stalberg::rooms::CellIndex> entrances(
            layout.getConnectedEntrances().begin(),
            layout.getConnectedEntrances().end());
        if (entrances.size() < countFrequencies.size()) {
            ++countFrequencies[entrances.size()];
        }
        std::ranges::sort(entrances);
        entranceSets.insert(std::move(entrances));
    }
    return check(entranceSets.size() == 42,
               "organic growth can select every three-to-six entrance set")
        && check(countFrequencies[3] > countFrequencies[4]
                && countFrequencies[4] > countFrequencies[5]
                && countFrequencies[5] > countFrequencies[6]
                && countFrequencies[6] > 0,
            "entrance counts follow their proportional combination weights");
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
    grid.cells = {
        { { 0.0F, 0.0F }, 1.0F, 0.5F, true },
        { { 1.0F, 0.0F }, 1.0F, 0.5F, true }
    };
    grid.setConnections({
        { { 1, 1.0F, 1.0F } },
        { { 0, 1.0F, 1.0F } }
    });
    grid.connections.pop_back();

    const auto malformedAdjacency
        = stalberg::rooms::RoomGenerator {}.generate(grid, 1);
    bool valid = check(malformedAdjacency.getRoomCount() == 0,
        "malformed adjacency does not generate rooms");
    valid &= check(malformedAdjacency.getCellAssignments().size() == grid.cells.size(),
        "invalid neutral topology still returns aligned assignments");

    grid.setConnections({
        { { 1, 1.0F, 1.0F } },
        { { 0, 1.0F, 1.0F } }
    });
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
    grid.relaxToCompletion();

    bool valid = adapterProducesValidRoomInput(grid);
    valid &= roomGenerationIsRepeatable(
        grid, stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    valid &= roomGenerationIsRepeatable(
        grid, stalberg::rooms::RoomGenerationMethod::BranchingShapes);
    valid &= roomGenerationIsRepeatable(
        grid, stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
    valid &= smallMapRecipesPublishDistinctIntent();
    valid &= shooterLayoutHasExplicitCombatStructure(grid);
    valid &= largeShooterLayoutUsesDirectArenaLinks();
    valid &= shooterLayoutsCanCreateDenseAreas();
    valid &= bestOfCandidatesDoesNotReduceQuality(grid);
    valid &= connectionOrderingDoesNotAffectGeneration(grid);
    valid &= canonicalTopologyFingerprintIsStable();
    valid &= adapterRejectsMisalignedDualGrid();
    valid &= roomInputIsIndependentFromLaterRelaxation();
    for (std::uint32_t roomSeed = 1; roomSeed <= 12; ++roomSeed) {
        valid &= roomLayoutIsValid(grid, roomSeed);
        valid &= roomLayoutIsValid(grid,
            roomSeed,
            stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
        if (roomSeed <= 6) {
            valid &= roomLayoutIsValid(grid,
                roomSeed,
                stalberg::rooms::RoomGenerationMethod::ShooterLayout);
        }
    }
    for (const int radius : std::array { 2, 7, 14 }) {
        stalberg::StalbergGrid representativeGrid;
        representativeGrid.generate(radius, 3);
        for (const std::uint32_t roomSeed : std::array<std::uint32_t, 2> { 1, 7 }) {
            valid &= roomLayoutIsValid(representativeGrid, roomSeed);
            valid &= roomLayoutIsValid(representativeGrid,
                roomSeed,
                stalberg::rooms::RoomGenerationMethod::OrganicGrowth);
            valid &= roomLayoutIsValid(representativeGrid,
                roomSeed,
                stalberg::rooms::RoomGenerationMethod::ShooterLayout);
        }
    }
    stalberg::StalbergGrid compactShooterGrid;
    compactShooterGrid.generate(2, 1);
    compactShooterGrid.relaxToCompletion();
    valid &= roomLayoutIsValid(compactShooterGrid,
        4,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    valid &= roomLayoutIsValid(compactShooterGrid,
        7,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    stalberg::StalbergGrid constrainedShooterGrid;
    constrainedShooterGrid.generate(2, 6);
    constrainedShooterGrid.relaxToCompletion();
    valid &= roomLayoutIsValid(constrainedShooterGrid,
        27,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    stalberg::StalbergGrid smallShooterGrid;
    smallShooterGrid.generate(3, 1);
    smallShooterGrid.relaxToCompletion();
    valid &= roomLayoutIsValid(smallShooterGrid,
        2,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    stalberg::StalbergGrid largeShooterGrid;
    largeShooterGrid.generate(14, 1);
    largeShooterGrid.relaxToCompletion();
    valid &= roomLayoutIsValid(largeShooterGrid,
        2,
        stalberg::rooms::RoomGenerationMethod::ShooterLayout);

    valid &= organicGrowthIsEvenlyDispersed(grid);
    valid &= generationMethodsVaryRoomShapes(grid);
    valid &= organicGrowthVariesRoomSizes(grid);
    valid &= organicGrowthCanUseEveryValidEntranceSet();
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
