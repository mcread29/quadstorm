ShooterGenerationResult generateShooterLayout(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<CellIndex>& buildableCells,
    CellIndex center,
    float cellScale,
    const EntranceSelection& entranceSelection,
    std::uint32_t requestedSeed,
    std::uint32_t generationSeed,
    SmallMapRecipe smallMapRecipe,
    LargeMapArchetype largeMapArchetype,
    std::mt19937& random,
    std::vector<int>& assignments,
    std::vector<GeneratedRoom>& rooms,
    std::vector<CellIndex>& connectedEntrances)
{
    ShooterGenerationResult result;
    result.startCell = center;
    result.exitCell = center;
    if (buildableCells.size() < 8) {
        return result;
    }

    const std::size_t entranceCount = std::min(
        entranceSelection.targetCount, entranceSelection.order.size());
    if (entranceCount > 0) {
        const CellIndex start = interiorCellForEntrance(
            adjacency, buildable, entranceSelection.order.front());
        if (start < assignments.size()) {
            result.startCell = start;
        }
    }

    float farthestExitDistance = -1.0F;
    for (std::size_t index = 1; index < entranceCount; ++index) {
        const CellIndex candidate = interiorCellForEntrance(
            adjacency, buildable, entranceSelection.order[index]);
        if (candidate >= assignments.size()) {
            continue;
        }
        const float candidateDistance = distance(
            grid.cells[result.startCell].position, grid.cells[candidate].position);
        if (candidateDistance > farthestExitDistance) {
            result.exitCell = candidate;
            farthestExitDistance = candidateDistance;
        }
    }
    if (result.exitCell == result.startCell) {
        result.exitCell = *std::ranges::max_element(buildableCells,
            {},
            [&](CellIndex cell) {
                return distance(grid.cells[result.startCell].position,
                    grid.cells[cell].position);
            });
    }

    const std::size_t maximumArenaCount = std::max<std::size_t>(
        2, buildableCells.size() / 8);
    const std::size_t minimumArenaCount
        = buildableCells.size() >= 160 ? 5 : 4;
    const std::size_t desiredArenaCount = std::min(maximumArenaCount,
        std::clamp<std::size_t>(
            buildableCells.size() / 55, minimumArenaCount, 20));
    const float averageTarget = static_cast<float>(buildableCells.size())
        * 0.36F
        / static_cast<float>(std::max<std::size_t>(desiredArenaCount, 1));
    const ArenaDistributionBrief distribution = chooseArenaDistribution(
        buildableCells.size(), desiredArenaCount, requestedSeed, generationSeed);

    std::vector<CellIndex> requestedSeeds { result.startCell };
    if (result.exitCell != result.startCell) {
        requestedSeeds.push_back(result.exitCell);
    }
    if (desiredArenaCount == 5
        && smallMapRecipe != SmallMapRecipe::BrokenRing
        && center < assignments.size() && buildable[center]
        && std::ranges::find(requestedSeeds, center) == requestedSeeds.end()) {
        requestedSeeds.push_back(center);
    }
    while (requestedSeeds.size() < desiredArenaCount) {
        CellIndex selected = assignments.size();
        float selectedScore = -std::numeric_limits<float>::infinity();
        const std::size_t arenaIndex = requestedSeeds.size();
        const bool clusteredSatellite
            = distribution.distribution == ArenaDistribution::Cluster
            && arenaIndex > distribution.firstFeaturedArena
            && arenaIndex < distribution.firstFeaturedArena
                    + distribution.featuredArenaCount;
        const CellIndex clusterAnchor = clusteredSatellite
            ? requestedSeeds[distribution.firstFeaturedArena]
            : assignments.size();
        const float nominalRadius
            = cellScale * std::sqrt(std::max(averageTarget, 4.0F)
                / std::numbers::pi_v<float>);
        const float preferredClusterDistance
            = nominalRadius * 1.75F + cellScale * 0.5F;
        const float minimumClusterDistance
            = nominalRadius * 1.35F + cellScale * 0.5F;
        const float maximumClusterDistance
            = nominalRadius * 2.35F + cellScale * 0.75F;
        const bool brokenRingIntermediate = desiredArenaCount == 5
            && smallMapRecipe == SmallMapRecipe::BrokenRing;
        CellPoint brokenRingTarget {};
        if (brokenRingIntermediate) {
            const CellPoint start = grid.cells[result.startCell].position;
            const CellPoint exit = grid.cells[result.exitCell].position;
            const float x = exit.x - start.x;
            const float y = exit.y - start.y;
            const float routeLength = std::max(std::sqrt(x * x + y * y), 1.0F);
            const float amount = static_cast<float>(arenaIndex - 1)
                / static_cast<float>(desiredArenaCount - 1);
            const float bendDirection = (requestedSeed & 1U) == 0 ? 1.0F : -1.0F;
            const float bend = std::sin(amount * std::numbers::pi_v<float>)
                * routeLength * 0.2F * bendDirection;
            brokenRingTarget = CellPoint {
                start.x + x * amount - y / routeLength * bend,
                start.y + y * amount + x / routeLength * bend
            };
        }

        for (const CellIndex candidate : buildableCells) {
            if (std::ranges::find(requestedSeeds, candidate) != requestedSeeds.end()) {
                continue;
            }
            float nearestDistance = std::numeric_limits<float>::infinity();
            for (const CellIndex seed : requestedSeeds) {
                nearestDistance = std::min(nearestDistance,
                    distance(grid.cells[seed].position, grid.cells[candidate].position));
            }
            const float noise = unitNoise(generationSeed,
                arenaIndex * 104729U + candidate);
            float score = nearestDistance * (0.9F + noise * 0.2F);
            if (brokenRingIntermediate) {
                score = -distance(
                            grid.cells[candidate].position, brokenRingTarget)
                    + nearestDistance * 0.08F
                    + grid.cells[candidate].clearance * 0.12F
                    + noise * cellScale * 0.2F;
            } else if (clusteredSatellite) {
                const float anchorDistance = distance(
                    grid.cells[clusterAnchor].position,
                    grid.cells[candidate].position);
                float distanceFromOtherSeeds = std::numeric_limits<float>::infinity();
                for (std::size_t index = 0; index < requestedSeeds.size(); ++index) {
                    if (index != distribution.firstFeaturedArena) {
                        distanceFromOtherSeeds = std::min(distanceFromOtherSeeds,
                            distance(grid.cells[requestedSeeds[index]].position,
                                grid.cells[candidate].position));
                    }
                }
                if (anchorDistance < minimumClusterDistance
                    || anchorDistance > maximumClusterDistance
                    || distanceFromOtherSeeds < minimumClusterDistance) {
                    continue;
                }
                score = -std::abs(anchorDistance - preferredClusterDistance)
                    + distanceFromOtherSeeds * 0.12F
                    + grid.cells[candidate].clearance * 0.2F
                    + noise * cellScale * 0.35F;
            }
            if (score > selectedScore
                || (score == selectedScore && candidate < selected)) {
                selected = candidate;
                selectedScore = score;
            }
        }

        // A narrow or oddly shaped map may not contain the desired cluster
        // annulus. Fall back to normal farthest-point placement rather than
        // sacrificing a required arena.
        if (selected == assignments.size() && clusteredSatellite) {
            selectedScore = -1.0F;
            for (const CellIndex candidate : buildableCells) {
                if (std::ranges::find(requestedSeeds, candidate)
                    != requestedSeeds.end()) {
                    continue;
                }
                float nearestDistance = std::numeric_limits<float>::infinity();
                for (const CellIndex seed : requestedSeeds) {
                    nearestDistance = std::min(nearestDistance,
                        distance(grid.cells[seed].position,
                            grid.cells[candidate].position));
                }
                const float noise = unitNoise(generationSeed,
                    arenaIndex * 104729U + candidate);
                const float score
                    = nearestDistance * (0.9F + noise * 0.2F);
                if (score > selectedScore
                    || (score == selectedScore && candidate < selected)) {
                    selected = candidate;
                    selectedScore = score;
                }
            }
        }
        if (selected == assignments.size()) {
            break;
        }
        requestedSeeds.push_back(selected);
    }

    const bool usesSmallMapRecipe = requestedSeeds.size() == 5;
    const bool usesLargeMapArchetype = requestedSeeds.size() > 5;
    result.smallMapRecipeSelected = usesSmallMapRecipe;
    result.largeMapArchetypeSelected = usesLargeMapArchetype;
    const auto plannedArenaEdges = usesSmallMapRecipe
        ? planRecipeArenaConnections(requestedSeeds.size(), smallMapRecipe)
        : (usesLargeMapArchetype
                ? planArenaConnections(grid, requestedSeeds, largeMapArchetype)
                : planSpatialTreeConnections(
                    grid, requestedSeeds, generationSeed));
    const MissionGraphBrief missionGraph = makeMissionGraphBrief(
        plannedArenaEdges, requestedSeeds.size(), usesLargeMapArchetype);
    result.missionEdges = missionGraph.edges;
    const auto& plannedConnections = missionGraph.edges;
    // A passage shorter than an arena's approximate diameter is a doorway
    // transition, not another room. Keep one explicit connector for shooter
    // structure, then reserve additional connector identities for long routes.
    const std::size_t maximumDirectConnectionLength = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::sqrt(averageTarget)),
        MINIMUM_DIRECT_CONNECTION_LENGTH,
        MAXIMUM_DIRECT_CONNECTION_LENGTH);
    std::vector<bool> reservedSeeds(assignments.size(), false);
    for (const CellIndex seed : requestedSeeds) {
        reservedSeeds[seed] = true;
    }
    std::vector<CellIndex> arenaSeeds;
    std::uniform_real_distribution<float> sizeVariation(0.82F, 1.18F);
    for (std::size_t index = 0; index < requestedSeeds.size(); ++index) {
        const bool featured = index >= distribution.firstFeaturedArena
            && index < distribution.firstFeaturedArena
                    + distribution.featuredArenaCount;
        float featureScale = 1.0F;
        std::size_t maximumTarget = 60;
        if (distribution.distribution == ArenaDistribution::Landmark) {
            featureScale = featured
                ? 2.1F
                : static_cast<float>(requestedSeeds.size() - 2)
                    / static_cast<float>(requestedSeeds.size() - 1);
            maximumTarget = featured ? 96 : 60;
        } else if (distribution.distribution == ArenaDistribution::Cluster) {
            constexpr float clusterScale = 1.2F;
            featureScale = featured
                ? clusterScale
                : (static_cast<float>(requestedSeeds.size())
                        - clusterScale
                            * static_cast<float>(distribution.featuredArenaCount))
                    / static_cast<float>(requestedSeeds.size()
                        - distribution.featuredArenaCount);
        }
        const std::size_t targetSize = std::clamp<std::size_t>(
            static_cast<std::size_t>(averageTarget * featureScale
                * sizeVariation(random)),
            4,
            maximumTarget);
        const std::vector<CellIndex> roomCells = growArenaRoom(grid,
            adjacency,
            buildable,
            reservedSeeds,
            assignments,
            requestedSeeds[index],
            targetSize,
            generationSeed,
            index * 1000003U);
        if (roomCells.size() < 4) {
            result.complete = false;
            return result;
        }
        const int roomId = static_cast<int>(rooms.size());
        for (const CellIndex cell : roomCells) {
            assignments[cell] = roomId;
        }
        rooms.emplace_back(roomId, roomCells.size());
        arenaSeeds.push_back(requestedSeeds[index]);
    }
    if (arenaSeeds.size() < 2) {
        result.complete = false;
        return result;
    }
    result.arenaRoomCount = arenaSeeds.size();
    result.startCell = arenaSeeds.front();
    result.exitCell = arenaSeeds[1];
    bool smallMapLoopAdded = false;
    std::vector<std::size_t> connectionOrder;
    const std::size_t treeEdgeCount = arenaSeeds.size() - 1;
    if (usesLargeMapArchetype && plannedConnections.size() > treeEdgeCount) {
        for (std::size_t index = treeEdgeCount;
             index < plannedConnections.size(); ++index) {
            connectionOrder.push_back(index);
        }
    }
    for (std::size_t index = 0;
         index < std::min(treeEdgeCount, plannedConnections.size()); ++index) {
        connectionOrder.push_back(index);
    }
    if (connectionOrder.size() < plannedConnections.size()) {
        for (std::size_t index = treeEdgeCount;
             index < plannedConnections.size(); ++index) {
            connectionOrder.push_back(index);
        }
    }
    for (const std::size_t connectionIndex : connectionOrder) {
        const MissionEdgeBrief& missionEdge
            = plannedConnections[connectionIndex];
        const bool requiredMissionEdge = missionEdge.required;
        if (!requiredMissionEdge && smallMapLoopAdded) {
            continue;
        }
        const int first = missionEdge.firstArena;
        const int second = missionEdge.secondArena;
        const bool connected = addCorridorRoom(grid,
            adjacency,
            buildable,
            first,
            second,
            result.startCell,
            result.exitCell,
            maximumDirectConnectionLength,
            result.connectorRooms.empty(),
            assignments,
            rooms,
            result.connectorRooms,
            result.requiredDoorways);
        if (!connected && requiredMissionEdge) {
            result.complete = false;
        } else if (connected && !requiredMissionEdge) {
            smallMapLoopAdded = true;
        }
    }
    std::vector<bool> floor(assignments.size(), false);
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        floor[cell] = assignments[cell] != EMPTY_CELL;
    }
    for (std::size_t index = 0; index < entranceCount; ++index) {
        const CellIndex entrance = entranceSelection.order[index];
        const std::vector<CellIndex> path
            = pathToFloor(grid, adjacency, buildable, floor, entrance);
        if (path.empty()) {
            result.complete = false;
            continue;
        }
        const CellIndex destination = path.back();
        const int destinationRoom = assignments[destination];
        std::vector<CellIndex> corridor(path.begin(), path.end() - 1);
        if (corridor.empty()) {
            connectedEntrances.push_back(entrance);
            continue;
        }
        if (corridor.size()
            <= std::max<std::size_t>(maximumDirectConnectionLength, 1)) {
            for (const CellIndex cell : corridor) {
                assignments[cell] = destinationRoom;
            }
            rooms[static_cast<std::size_t>(destinationRoom)].cellCount
                += corridor.size();
        } else {
            corridor = widenCorridor(grid,
                adjacency,
                buildable,
                assignments,
                corridor,
                destinationRoom,
                destinationRoom);
            const int corridorId = static_cast<int>(rooms.size());
            for (const CellIndex cell : corridor) {
                assignments[cell] = corridorId;
            }
            rooms.emplace_back(corridorId, corridor.size());
            result.connectorRooms.push_back(corridorId);
            addRequiredDoorway(
                result.requiredDoorways, corridorId, destinationRoom);
        }
        for (const CellIndex cell : corridor) {
            floor[cell] = true;
        }
        connectedEntrances.push_back(entrance);
    }
    return result;
}



