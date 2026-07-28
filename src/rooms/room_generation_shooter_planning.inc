struct ShooterGenerationResult {
    CellIndex startCell = 0;
    CellIndex exitCell = 0;
    std::size_t arenaRoomCount = 0;
    std::vector<int> connectorRooms;
    std::vector<std::pair<int, int>> requiredDoorways;
    bool complete = true;
};

enum class ArenaDistribution {
    Dispersed,
    Landmark,
    Cluster
};

struct ArenaDistributionBrief {
    ArenaDistribution distribution = ArenaDistribution::Dispersed;
    std::size_t firstFeaturedArena = 0;
    std::size_t featuredArenaCount = 0;
};

ArenaDistributionBrief chooseArenaDistribution(std::size_t buildableCellCount,
    std::size_t arenaCount,
    std::uint32_t requestedSeed,
    std::uint32_t generationSeed)
{
    // Compact maps need every arena placement to carry mission structure. Larger
    // maps can spend some of that freedom on an occasional landmark or district.
    if (buildableCellCount < 160 || arenaCount < 5) {
        return {};
    }

    const float choice = unitNoise(requestedSeed,
        0x4152454e41545950ULL ^ buildableCellCount);
    if (choice < 0.45F) {
        return {};
    }

    if (choice < 0.725F) {
        const std::size_t available = arenaCount - 2;
        const std::size_t offset = std::min<std::size_t>(
            static_cast<std::size_t>(unitNoise(generationSeed,
                0x4c414e444d41524bULL)
                * static_cast<float>(available)),
            available - 1);
        return { ArenaDistribution::Landmark, 2 + offset, 1 };
    }

    const std::size_t clusterSize = arenaCount >= 7 ? 3 : 2;
    const std::size_t availableStarts = arenaCount - clusterSize - 1;
    const std::size_t offset = std::min<std::size_t>(
        static_cast<std::size_t>(unitNoise(generationSeed,
            0x434c555354455253ULL)
            * static_cast<float>(availableStarts)),
        availableStarts - 1);
    return { ArenaDistribution::Cluster, 2 + offset, clusterSize };
}

void addRequiredDoorway(
    std::vector<std::pair<int, int>>& requiredDoorways, int first, int second)
{
    const std::pair<int, int> doorway {
        std::min(first, second),
        std::max(first, second)
    };
    if (std::ranges::find(requiredDoorways, doorway) == requiredDoorways.end()) {
        requiredDoorways.push_back(doorway);
    }
}

CellIndex interiorCellForEntrance(
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    CellIndex entrance)
{
    const CellIndex noCell = adjacency.size();
    if (entrance >= adjacency.size()) {
        return noCell;
    }
    std::vector<bool> visited(adjacency.size(), false);
    std::queue<CellIndex> queue;
    queue.push(entrance);
    visited[entrance] = true;
    while (!queue.empty()) {
        const CellIndex cell = queue.front();
        queue.pop();
        if (buildable[cell]) {
            return cell;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return noCell;
}

std::vector<CellIndex> growArenaRoom(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<bool>& reservedSeeds,
    const std::vector<int>& assignments,
    CellIndex seedCell,
    std::size_t targetSize,
    std::uint32_t generationSeed,
    std::uint64_t salt)
{
    if (seedCell >= assignments.size() || !buildable[seedCell]
        || assignments[seedCell] != EMPTY_CELL) {
        return {};
    }

    std::vector<CellIndex> result { seedCell };
    std::vector<bool> inRoom(assignments.size(), false);
    std::vector<bool> inFrontier(assignments.size(), false);
    inRoom[seedCell] = true;
    std::vector<CellIndex> frontier;
    const auto addFrontier = [&](CellIndex cell) {
        const bool protectsAnotherSeed = std::ranges::any_of(
            adjacency[cell], [&](CellIndex neighbor) {
                return reservedSeeds[neighbor] && neighbor != seedCell;
            });
        if (buildable[cell] && assignments[cell] == EMPTY_CELL && !inRoom[cell]
            && !inFrontier[cell] && (!reservedSeeds[cell] || cell == seedCell)
            && !protectsAnotherSeed) {
            frontier.push_back(cell);
            inFrontier[cell] = true;
        }
    };
    for (const CellIndex neighbor : adjacency[seedCell]) {
        addFrontier(neighbor);
    }

    const float scale = std::max(grid.cells[seedCell].clearance * 2.0F, 1.0F);
    while (result.size() < targetSize && !frontier.empty()) {
        std::size_t selectedIndex = frontier.size();
        float selectedScore = -std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < frontier.size(); ++index) {
            const CellIndex candidate = frontier[index];
            const bool touchesOtherRoom = std::ranges::any_of(
                adjacency[candidate], [&](CellIndex neighbor) {
                    return assignments[neighbor] != EMPTY_CELL;
                });
            if (touchesOtherRoom) {
                continue;
            }
            const std::size_t sameRoomNeighbors = std::ranges::count_if(
                adjacency[candidate], [&](CellIndex neighbor) {
                    return inRoom[neighbor];
                });
            const float radius = distance(
                grid.cells[seedCell].position, grid.cells[candidate].position);
            const float noise = unitNoise(generationSeed,
                salt + static_cast<std::uint64_t>(result.size()) * 65537U
                    + candidate);
            const float score = static_cast<float>(sameRoomNeighbors) * 1.15F
                + std::clamp(grid.cells[candidate].clearance / scale, 0.0F, 1.5F)
                    * 0.45F
                - radius / scale * 0.12F + noise * 0.35F;
            if (score > selectedScore
                || (score == selectedScore && candidate < frontier[selectedIndex])) {
                selectedIndex = index;
                selectedScore = score;
            }
        }
        if (selectedIndex == frontier.size()) {
            break;
        }

        const CellIndex selected = frontier[selectedIndex];
        frontier[selectedIndex] = frontier.back();
        frontier.pop_back();
        inFrontier[selected] = false;
        inRoom[selected] = true;
        result.push_back(selected);
        for (const CellIndex neighbor : adjacency[selected]) {
            addFrontier(neighbor);
        }
    }
    return result;
}

std::vector<CellIndex> routeBetweenRooms(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    int sourceRoom,
    int targetRoom)
{
    using QueueEntry = std::pair<float, CellIndex>;
    const CellIndex noCell = assignments.size();
    std::vector<CellIndex> parent(assignments.size(), noCell);
    std::vector<float> costs(assignments.size(), std::numeric_limits<float>::infinity());
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> queue;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == sourceRoom) {
            parent[cell] = cell;
            costs[cell] = 0.0F;
            queue.emplace(0.0F, cell);
        }
    }

    CellIndex destinationApproach = noCell;
    float destinationCost = std::numeric_limits<float>::infinity();
    while (!queue.empty()) {
        const auto [cost, cell] = queue.top();
        queue.pop();
        if (cost > costs[cell]) {
            continue;
        }
        if (cost >= destinationCost) {
            break;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (assignments[neighbor] == targetRoom) {
                const float finalCost = cost + connectionCost(grid, cell, neighbor);
                if (finalCost < destinationCost) {
                    destinationApproach = cell;
                    destinationCost = finalCost;
                }
                continue;
            }
            if (!buildable[neighbor] || assignments[neighbor] != EMPTY_CELL) {
                continue;
            }
            const std::size_t foreignNeighbors = std::ranges::count_if(
                adjacency[neighbor], [&](CellIndex adjacent) {
                    return assignments[adjacent] != EMPTY_CELL
                        && assignments[adjacent] != sourceRoom
                        && assignments[adjacent] != targetRoom;
                });
            const float stepCost = connectionCost(grid, cell, neighbor)
                * (1.0F + static_cast<float>(foreignNeighbors) * 2.5F);
            const float nextCost = cost + stepCost;
            if (nextCost >= costs[neighbor]) {
                continue;
            }
            costs[neighbor] = nextCost;
            parent[neighbor] = cell;
            queue.emplace(nextCost, neighbor);
        }
    }
    if (destinationApproach == noCell) {
        return {};
    }

    std::vector<CellIndex> path;
    for (CellIndex cell = destinationApproach;
         assignments[cell] != sourceRoom; cell = parent[cell]) {
        path.push_back(cell);
    }
    std::ranges::reverse(path);
    return path;
}

std::vector<CellIndex> widenCorridor(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    const std::vector<int>& assignments,
    const std::vector<CellIndex>& centerline,
    int firstAllowedRoom,
    int secondAllowedRoom)
{
    std::vector<CellIndex> result = centerline;
    std::vector<bool> selected(assignments.size(), false);
    for (const CellIndex cell : centerline) {
        selected[cell] = true;
    }

    for (std::size_t step = 1; step + 1 < centerline.size(); ++step) {
        const CellIndex center = centerline[step];
        CellIndex best = assignments.size();
        float bestClearance = -1.0F;
        for (const CellIndex candidate : adjacency[center]) {
            if (!buildable[candidate] || assignments[candidate] != EMPTY_CELL
                || selected[candidate]) {
                continue;
            }
            const bool touchesForeignRoom = std::ranges::any_of(
                adjacency[candidate], [&](CellIndex neighbor) {
                    const int room = assignments[neighbor];
                    return room != EMPTY_CELL && room != firstAllowedRoom
                        && room != secondAllowedRoom;
                });
            if (touchesForeignRoom) {
                continue;
            }
            if (grid.cells[candidate].clearance > bestClearance) {
                best = candidate;
                bestClearance = grid.cells[candidate].clearance;
            }
        }
        if (best != assignments.size()) {
            selected[best] = true;
            result.push_back(best);
        }
    }
    return result;
}

bool roomRemainsConnectedWithout(
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<int>& assignments,
    int roomId,
    CellIndex removed)
{
    CellIndex start = assignments.size();
    std::size_t expected = 0;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        if (assignments[cell] == roomId && cell != removed) {
            ++expected;
            if (start == assignments.size()) {
                start = cell;
            }
        }
    }
    if (expected < MINIMUM_ROOM_SIZE || start == assignments.size()) {
        return false;
    }
    const std::size_t reached = reachableCellCount(adjacency,
        start,
        [&](CellIndex cell) {
            return cell != removed && assignments[cell] == roomId;
        });
    return reached == expected;
}

bool addCorridorRoom(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    const std::vector<bool>& buildable,
    int sourceRoom,
    int targetRoom,
    CellIndex protectedStart,
    CellIndex protectedExit,
    std::size_t maximumDirectConnectionLength,
    bool requireExplicitConnector,
    std::vector<int>& assignments,
    std::vector<GeneratedRoom>& rooms,
    std::vector<int>& connectorRooms,
    std::vector<std::pair<int, int>>& requiredDoorways)
{
    std::vector<CellIndex> path = routeBetweenRooms(
        grid, adjacency, buildable, assignments, sourceRoom, targetRoom);
    if (path.empty()) {
        bool touching = false;
        for (CellIndex cell = 0; cell < assignments.size() && !touching; ++cell) {
            if (assignments[cell] != sourceRoom) {
                continue;
            }
            touching = std::ranges::any_of(adjacency[cell],
                [&](CellIndex neighbor) {
                    return assignments[neighbor] == targetRoom;
                });
        }
        if (touching) {
            addRequiredDoorway(requiredDoorways, sourceRoom, targetRoom);
        }
        return touching;
    }
    if (!requireExplicitConnector
        && path.size() <= maximumDirectConnectionLength) {
        for (const CellIndex cell : path) {
            assignments[cell] = sourceRoom;
        }
        rooms[static_cast<std::size_t>(sourceRoom)].cellCount += path.size();
        addRequiredDoorway(requiredDoorways, sourceRoom, targetRoom);
        return true;
    }
    if (path.size() == 1) {
        const CellIndex bridge = path.front();
        CellIndex donor = assignments.size();
        int donorRoom = EMPTY_CELL;
        for (const int room : std::array { sourceRoom, targetRoom }) {
            if (rooms[static_cast<std::size_t>(room)].cellCount
                <= MINIMUM_ROOM_SIZE) {
                continue;
            }
            for (const CellIndex neighbor : adjacency[bridge]) {
                if (neighbor != protectedStart && neighbor != protectedExit
                    && assignments[neighbor] == room
                    && roomRemainsConnectedWithout(
                        adjacency, assignments, room, neighbor)) {
                    donor = neighbor;
                    donorRoom = room;
                    break;
                }
            }
            if (donor != assignments.size()) {
                break;
            }
        }
        if (donor != assignments.size()) {
            const int corridorId = static_cast<int>(rooms.size());
            assignments[bridge] = corridorId;
            assignments[donor] = corridorId;
            --rooms[static_cast<std::size_t>(donorRoom)].cellCount;
            rooms.emplace_back(corridorId, 2);
            connectorRooms.push_back(corridorId);
            addRequiredDoorway(requiredDoorways, sourceRoom, corridorId);
            addRequiredDoorway(requiredDoorways, corridorId, targetRoom);
            return true;
        }
        assignments[bridge] = sourceRoom;
        ++rooms[static_cast<std::size_t>(sourceRoom)].cellCount;
        addRequiredDoorway(requiredDoorways, sourceRoom, targetRoom);
        return true;
    }

    path = widenCorridor(
        grid, adjacency, buildable, assignments, path, sourceRoom, targetRoom);
    const int corridorId = static_cast<int>(rooms.size());
    for (const CellIndex cell : path) {
        assignments[cell] = corridorId;
    }
    rooms.emplace_back(corridorId, path.size());
    connectorRooms.push_back(corridorId);
    addRequiredDoorway(requiredDoorways, sourceRoom, corridorId);
    addRequiredDoorway(requiredDoorways, corridorId, targetRoom);
    return true;
}

std::vector<std::pair<int, int>> planRecipeArenaConnections(
    std::size_t arenaCount, SmallMapRecipe recipe)
{
    std::vector<std::pair<int, int>> edges;
    if (arenaCount < 3) {
        return edges;
    }

    switch (recipe) {
    case SmallMapRecipe::HubCircuit:
        edges.emplace_back(0, 2);
        edges.emplace_back(2, 1);
        for (std::size_t arena = 3; arena < arenaCount; ++arena) {
            edges.emplace_back(2, static_cast<int>(arena));
        }
        break;
    case SmallMapRecipe::BrokenRing:
        edges.emplace_back(0, 2);
        if (arenaCount == 3) {
            edges.emplace_back(2, 1);
            break;
        }
        edges.emplace_back(2, 3);
        edges.emplace_back(3, 1);
        for (std::size_t arena = 4; arena < arenaCount; ++arena) {
            edges.emplace_back(2, static_cast<int>(arena));
        }
        edges.emplace_back(0, 3);
        break;
    case SmallMapRecipe::TwinWings:
        edges.emplace_back(0, 2);
        edges.emplace_back(2, 1);
        for (std::size_t arena = 3; arena < arenaCount; ++arena) {
            edges.emplace_back(2, static_cast<int>(arena));
        }
        if (arenaCount >= 4) {
            edges.emplace_back(3, 1);
        }
        break;
    }
    return edges;
}

std::vector<std::pair<int, int>> planArenaConnections(
    const RoomGrid& grid,
    const std::vector<CellIndex>& arenaSeeds,
    std::uint32_t generationSeed)
{
    const std::size_t arenaCount = arenaSeeds.size();
    std::vector<std::pair<int, int>> edges;
    if (arenaCount < 2) {
        return edges;
    }

    std::vector<bool> connected(arenaCount, false);
    connected[0] = true;
    while (edges.size() + 1 < arenaCount) {
        std::size_t selectedFirst = arenaCount;
        std::size_t selectedSecond = arenaCount;
        float selectedCost = std::numeric_limits<float>::infinity();
        for (std::size_t first = 0; first < arenaCount; ++first) {
            if (!connected[first]) {
                continue;
            }
            for (std::size_t second = 0; second < arenaCount; ++second) {
                if (connected[second]
                    || (arenaCount > 2
                        && ((first == 0 && second == 1)
                            || (first == 1 && second == 0)))) {
                    continue;
                }
                const float length = distance(grid.cells[arenaSeeds[first]].position,
                    grid.cells[arenaSeeds[second]].position);
                const float noise = unitNoise(generationSeed,
                    first * 4099U + second * 65537U);
                const float cost = length * (0.92F + noise * 0.16F);
                if (cost < selectedCost) {
                    selectedFirst = first;
                    selectedSecond = second;
                    selectedCost = cost;
                }
            }
        }
        if (selectedSecond == arenaCount) {
            break;
        }
        edges.emplace_back(
            static_cast<int>(selectedFirst), static_cast<int>(selectedSecond));
        connected[selectedSecond] = true;
    }

    std::vector<std::vector<int>> graph(arenaCount);
    for (const auto& [first, second] : edges) {
        graph[static_cast<std::size_t>(first)].push_back(second);
        graph[static_cast<std::size_t>(second)].push_back(first);
    }
    if (arenaCount < 5) {
        return edges;
    }
    struct LoopCandidate {
        std::pair<int, int> edge;
        float score;
    };
    std::vector<LoopCandidate> loopCandidates;
    for (std::size_t first = 0; first < arenaCount; ++first) {
        const std::vector<int> distances
            = breadthFirstDistances(graph, static_cast<int>(first));
        for (std::size_t second = first + 1; second < arenaCount; ++second) {
            if (first < 2 || second < 2) {
                continue;
            }
            const bool alreadyConnected = std::ranges::any_of(
                edges, [&](const auto& edge) {
                    return (edge.first == static_cast<int>(first)
                               && edge.second == static_cast<int>(second))
                        || (edge.first == static_cast<int>(second)
                            && edge.second == static_cast<int>(first));
                });
            if (alreadyConnected || distances[second] < 3) {
                continue;
            }
            const float physicalDistance = distance(
                grid.cells[arenaSeeds[first]].position,
                grid.cells[arenaSeeds[second]].position);
            loopCandidates.push_back(LoopCandidate {
                { static_cast<int>(first), static_cast<int>(second) },
                static_cast<float>(distances[second]) * 1000.0F - physicalDistance
            });
        }
    }
    std::ranges::sort(loopCandidates, [](const LoopCandidate& lhs,
                                           const LoopCandidate& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.edge < rhs.edge;
    });
    const std::size_t retryCount = std::min<std::size_t>(6, loopCandidates.size());
    for (std::size_t candidate = 0; candidate < retryCount; ++candidate) {
        edges.push_back(loopCandidates[candidate].edge);
    }
    return edges;
}

