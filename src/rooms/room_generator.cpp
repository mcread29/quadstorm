#include "rooms/room_generator.hpp"
#include "rooms/room_annotation.hpp"
#include "rooms/room_doorway_planning.hpp"
#include "rooms/room_generation_random.hpp"
#include "rooms/room_generation_scoring.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
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
constexpr std::size_t MINIMUM_DIRECT_CONNECTION_LENGTH = 2;
constexpr std::size_t MAXIMUM_DIRECT_CONNECTION_LENGTH = 8;

SmallMapRecipe smallMapRecipeForSeed(std::uint32_t seed)
{
    switch ((seed - 1U) % 3U) {
    case 0:
        return SmallMapRecipe::HubCircuit;
    case 1:
        return SmallMapRecipe::BrokenRing;
    default:
        return SmallMapRecipe::TwinWings;
    }
}

using detail::breadthFirstDistances;
using detail::CellAdjacency;
using detail::DisjointSet;
using detail::findConnection;
using detail::generateDoorways;
using detail::generatePlannedDoorways;
using detail::mix;
using detail::annotateRooms;
using detail::unitNoise;
using detail::reachableCellCount;
using detail::sharedBoundaryLength;

#include "rooms/room_generation_strategies.inc"

} // namespace

std::uint64_t canonicalTopologyFingerprint(const RoomGrid& grid)
{
    return detail::canonicalTopologyFingerprint(grid);
}

RoomLayout RoomGenerator::generateCandidate(
    const detail::PreparedGenerationContext& prepared,
    std::uint32_t requestedSeed,
    std::uint32_t variantSeed,
    RoomGenerationMethod method,
    SmallMapRecipe smallMapRecipe,
    const std::vector<CellIndex>& entranceOrder,
    std::size_t entranceTargetCount) const
{
    RoomLayout result;
    result.seed = requestedSeed;
    result.method = method;
    result.smallMapRecipe = smallMapRecipe;
    auto& rooms = result.rooms;
    auto& doorways = result.doorways;
    auto& connectedEntrances = result.connectedEntrances;
    auto& cellAssignments = result.cellAssignments;

    const RoomGrid& grid = prepared.grid;
    cellAssignments.assign(grid.getCellCount(), EMPTY_CELL);
    if (grid.getCellCount() == 0 || !prepared.valid) {
        return result;
    }

    const auto cells = grid.getCells();
    const CellAdjacency& adjacency = prepared.adjacency;
    const std::vector<bool>& buildable = prepared.buildable;
    const std::vector<CellIndex>& buildableCells = prepared.buildableCells;
    if (buildableCells.empty()) {
        return result;
    }

    const std::uint32_t generationSeed = static_cast<std::uint32_t>(mix(
        prepared.fingerprint ^ static_cast<std::uint64_t>(variantSeed)));
    std::mt19937 random(generationSeed);
    const CellIndex center = prepared.center;
    // Consume the same random choices in every candidate, but keep the requested
    // entrance brief fixed so best-of-N selection cannot bias entrance counts.
    static_cast<void>(selectEntrances(prepared.canonicalEntrances, random));
    const EntranceSelection entranceSelection {
        entranceOrder,
        entranceTargetCount
    };
    if (method == RoomGenerationMethod::ShooterLayout) {
        const ShooterGenerationResult shooter = generateShooterLayout(grid,
            adjacency,
            buildable,
            buildableCells,
            center,
            prepared.cellScale,
            entranceSelection,
            requestedSeed,
            generationSeed,
            smallMapRecipe,
            random,
            cellAssignments,
            rooms,
            connectedEntrances);
        result.smallMapRecipeSelected = shooter.arenaRoomCount == 5;
        if (!shooter.complete || rooms.size() < 2
            || !generatePlannedDoorways(grid,
                adjacency,
                generationSeed,
                cellAssignments,
                shooter.requiredDoorways,
                doorways)) {
            rooms.clear();
            doorways.clear();
            connectedEntrances.clear();
            std::ranges::fill(cellAssignments, EMPTY_CELL);
            return result;
        }
        annotateRooms(grid,
            adjacency,
            cellAssignments,
            doorways,
            connectedEntrances,
            shooter.startCell,
            generationSeed,
            rooms,
            shooter.exitCell);
        std::vector<std::size_t> doorwayDegrees(rooms.size(), 0);
        for (const Doorway& doorway : doorways) {
            ++doorwayDegrees[static_cast<std::size_t>(doorway.firstRegion)];
            ++doorwayDegrees[static_cast<std::size_t>(doorway.secondRegion)];
        }
        for (std::size_t arena = 0;
             arena < shooter.arenaRoomCount && arena < rooms.size(); ++arena) {
            GeneratedRoom& room = rooms[arena];
            if (room.role != RoomRole::Start && room.role != RoomRole::Exit) {
                room.role = RoomRole::Combat;
            }
        }
        if (shooter.arenaRoomCount == 5 && rooms.size() > 2) {
            rooms[2].role = RoomRole::Hub;
        } else if (shooter.arenaRoomCount > 2) {
            std::size_t hub = 2;
            for (std::size_t arena = 3;
                 arena < shooter.arenaRoomCount && arena < rooms.size();
                 ++arena) {
                if (doorwayDegrees[arena] > doorwayDegrees[hub]) {
                    hub = arena;
                }
            }
            rooms[hub].role = RoomRole::Hub;
        }
        if (rooms.size() >= shooter.arenaRoomCount
            && shooter.arenaRoomCount > 3) {
            std::size_t reward = shooter.arenaRoomCount;
            for (std::size_t arena = shooter.arenaRoomCount;
                 arena-- > 2;) {
                if (rooms[arena].role != RoomRole::Combat) {
                    continue;
                }
                if (reward == shooter.arenaRoomCount
                    || doorwayDegrees[arena] == 1) {
                    reward = arena;
                }
                if (doorwayDegrees[arena] == 1) {
                    break;
                }
            }
            if (reward < shooter.arenaRoomCount) {
                rooms[reward].role = RoomRole::Reward;
            }
        }
        for (const int connector : shooter.connectorRooms) {
            if (connector < 0 || static_cast<std::size_t>(connector) >= rooms.size()) {
                continue;
            }
            GeneratedRoom& room = rooms[static_cast<std::size_t>(connector)];
            if (room.role != RoomRole::Start && room.role != RoomRole::Exit) {
                room.role = RoomRole::Connector;
            }
        }
        return result;
    }
    if (method == RoomGenerationMethod::OrganicGrowth) {
        generateOrganicGrowth(grid,
            adjacency,
            buildable,
            buildableCells,
            center,
            prepared.cellScale,
            entranceSelection,
            generationSeed,
            random,
            cellAssignments,
            rooms,
            connectedEntrances);
        generateDoorways(
            grid, adjacency, generationSeed, cellAssignments, rooms.size(), doorways);
        annotateRooms(grid,
            adjacency,
            cellAssignments,
            doorways,
            connectedEntrances,
            center,
            generationSeed,
            rooms);
        return result;
    }

    const float cellScale = prepared.cellScale;
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
        adjacency,
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
    return generate(grid, seed, RoomGenerationOptions {
        .method = method,
        .candidateCount = 6,
        .smallMapRecipe = std::nullopt
    });
}

RoomLayout RoomGenerator::generate(const RoomGrid& grid,
    std::uint32_t seed,
    const RoomGenerationOptions& options) const
{
    const detail::PreparedGenerationContext prepared(grid);
    const std::size_t candidateCount
        = std::clamp<std::size_t>(options.candidateCount, 1, 32);
    const SmallMapRecipe smallMapRecipe = options.smallMapRecipe.value_or(
        prepared.buildableCells.size() < 160
            ? SmallMapRecipe::HubCircuit
            : smallMapRecipeForSeed(seed));
    std::size_t candidateLimit = candidateCount;
    RoomLayout best;
    bool haveBest = false;
    float bestScore = -std::numeric_limits<float>::infinity();
    EntranceSelection entranceBrief { prepared.canonicalEntrances, 0 };
    if (prepared.valid) {
        const std::uint32_t briefSeed = static_cast<std::uint32_t>(mix(
            prepared.fingerprint ^ static_cast<std::uint64_t>(seed)));
        std::mt19937 briefRandom(briefSeed);
        entranceBrief = selectEntrances(
            prepared.canonicalEntrances, briefRandom);
    }

    for (std::size_t candidateIndex = 0;
         candidateIndex < candidateLimit; ++candidateIndex) {
        const std::uint32_t variantSeed = candidateIndex == 0
            ? seed
            : static_cast<std::uint32_t>(mix(
                (static_cast<std::uint64_t>(seed) << 32U)
                ^ mix(0x53484f4f544552ULL + candidateIndex)));
        RoomLayout candidate = generateCandidate(prepared,
            seed,
            variantSeed,
            options.method,
            smallMapRecipe,
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
        const bool entrancesValid
            = std::ranges::equal(expectedEntrances, actualEntrances);
        const bool structureValid = detail::candidateIsValid(grid,
            prepared.adjacency,
            candidate,
            MINIMUM_ROOM_SIZE,
            MAX_GENERATED_ROOMS);
        const bool shooterValid = options.method != RoomGenerationMethod::ShooterLayout
            || detail::shooterCandidateIsValid(grid, candidate);
        if (!entrancesValid || !structureValid || !shooterValid) {
            if (candidateIndex + 1 == candidateLimit && !haveBest
                && options.method == RoomGenerationMethod::ShooterLayout
                && candidateLimit < 32) {
                // Retry constrained grids only after the normal candidate budget
                // failed to embed a complete mission graph.
                candidateLimit = 32;
            }
            continue;
        }

        candidate.qualityScore = detail::candidateScore(
            prepared, candidate, options.method);
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
    empty.smallMapRecipe = smallMapRecipe;
    empty.cellAssignments.assign(grid.getCellCount(), EMPTY_CELL);
    return empty;
}

} // namespace stalberg::rooms
