struct ShooterGenerationResult {
    CellIndex startCell = 0;
    CellIndex exitCell = 0;
    std::size_t arenaRoomCount = 0;
    bool smallMapRecipeSelected = false;
    bool largeMapArchetypeSelected = false;
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

std::pair<int, int> canonicalArenaEdge(int first, int second)
{
    return std::minmax(first, second);
}

void addArenaEdge(std::vector<std::pair<int, int>>& edges, int first, int second)
{
    if (first == second) {
        return;
    }
    const auto edge = canonicalArenaEdge(first, second);
    if (std::ranges::find(edges, edge) == edges.end()) {
        edges.push_back(edge);
    }
}

float arenaSeedDistance(const RoomGrid& grid,
    const std::vector<CellIndex>& arenaSeeds, int first, int second)
{
    return distance(grid.cells[arenaSeeds[static_cast<std::size_t>(first)]].position,
        grid.cells[arenaSeeds[static_cast<std::size_t>(second)]].position);
}

int arenaMedoid(const RoomGrid& grid,
    const std::vector<CellIndex>& arenaSeeds,
    const std::vector<int>& candidates,
    int avoided = -1)
{
    int selected = candidates.front();
    float selectedDistance = std::numeric_limits<float>::infinity();
    for (const int candidate : candidates) {
        if (candidate == avoided && candidates.size() > 1) {
            continue;
        }
        float totalDistance = 0.0F;
        for (const int other : candidates) {
            totalDistance += arenaSeedDistance(
                grid, arenaSeeds, candidate, other);
        }
        if (totalDistance < selectedDistance
            || (totalDistance == selectedDistance && candidate < selected)) {
            selected = candidate;
            selectedDistance = totalDistance;
        }
    }
    return selected;
}

std::vector<std::pair<int, int>> planHubAndSpokes(
    const RoomGrid& grid, const std::vector<CellIndex>& arenaSeeds)
{
    std::vector<int> hubCandidates;
    for (std::size_t arena = 2; arena < arenaSeeds.size(); ++arena) {
        hubCandidates.push_back(static_cast<int>(arena));
    }
    const int hub = arenaMedoid(grid, arenaSeeds, hubCandidates);
    int startSpoke = -1;
    float startSpokeDistance = std::numeric_limits<float>::infinity();
    for (std::size_t arena = 2; arena < arenaSeeds.size(); ++arena) {
        if (static_cast<int>(arena) == hub) {
            continue;
        }
        const float candidateDistance = arenaSeedDistance(
            grid, arenaSeeds, 0, static_cast<int>(arena));
        if (candidateDistance < startSpokeDistance) {
            startSpoke = static_cast<int>(arena);
            startSpokeDistance = candidateDistance;
        }
    }

    std::vector<std::pair<int, int>> edges;
    std::vector<bool> connected(arenaSeeds.size(), false);
    addArenaEdge(edges, 0, startSpoke);
    addArenaEdge(edges, startSpoke, hub);
    addArenaEdge(edges, hub, 1);
    connected[0] = true;
    connected[1] = true;
    connected[static_cast<std::size_t>(hub)] = true;
    connected[static_cast<std::size_t>(startSpoke)] = true;

    // Keep the landmark junction readable instead of creating an unroutable
    // all-to-one star on twenty-arena maps.
    for (int spoke = 0; spoke < 2; ++spoke) {
        int selected = -1;
        float selectedDistance = std::numeric_limits<float>::infinity();
        for (std::size_t arena = 2; arena < arenaSeeds.size(); ++arena) {
            if (connected[arena]) {
                continue;
            }
            const float candidateDistance = arenaSeedDistance(
                grid, arenaSeeds, hub, static_cast<int>(arena));
            if (candidateDistance < selectedDistance) {
                selected = static_cast<int>(arena);
                selectedDistance = candidateDistance;
            }
        }
        if (selected >= 0) {
            addArenaEdge(edges, hub, selected);
            connected[static_cast<std::size_t>(selected)] = true;
        }
    }
    while (std::ranges::find(connected, false) != connected.end()) {
        int selected = -1;
        int parent = -1;
        float selectedDistance = std::numeric_limits<float>::infinity();
        for (std::size_t arena = 0; arena < arenaSeeds.size(); ++arena) {
            if (connected[arena]) {
                continue;
            }
            for (std::size_t candidateParent = 0;
                 candidateParent < arenaSeeds.size(); ++candidateParent) {
                if (!connected[candidateParent]
                    || static_cast<int>(candidateParent) == hub) {
                    continue;
                }
                const float candidateDistance = arenaSeedDistance(grid,
                    arenaSeeds,
                    static_cast<int>(arena),
                    static_cast<int>(candidateParent));
                if (candidateDistance < selectedDistance) {
                    selected = static_cast<int>(arena);
                    parent = static_cast<int>(candidateParent);
                    selectedDistance = candidateDistance;
                }
            }
        }
        if (selected < 0) {
            break;
        }
        addArenaEdge(edges, parent, selected);
        connected[static_cast<std::size_t>(selected)] = true;
    }
    return edges;
}

std::vector<std::pair<int, int>> planRingAndBranches(
    const RoomGrid& grid, const std::vector<CellIndex>& arenaSeeds)
{
    const std::size_t arenaCount = arenaSeeds.size();
    CellPoint center {};
    for (const CellIndex seed : arenaSeeds) {
        center.x += grid.cells[seed].position.x;
        center.y += grid.cells[seed].position.y;
    }
    center.x /= static_cast<float>(arenaCount);
    center.y /= static_cast<float>(arenaCount);

    std::vector<int> outerCandidates;
    for (std::size_t arena = 2; arena < arenaCount; ++arena) {
        outerCandidates.push_back(static_cast<int>(arena));
    }
    std::ranges::sort(outerCandidates, [&](int lhs, int rhs) {
        const float left = distance(center,
            grid.cells[arenaSeeds[static_cast<std::size_t>(lhs)]].position);
        const float right = distance(center,
            grid.cells[arenaSeeds[static_cast<std::size_t>(rhs)]].position);
        return left != right ? left > right : lhs < rhs;
    });
    const std::size_t ringTarget = std::clamp<std::size_t>(
        arenaCount / 2, std::min<std::size_t>(6, arenaCount), 8);
    std::vector<int> ring { 0, 1 };
    for (const int candidate : outerCandidates) {
        if (ring.size() >= ringTarget) {
            break;
        }
        ring.push_back(candidate);
    }
    std::ranges::sort(ring, [&](int lhs, int rhs) {
        const CellPoint left = grid.cells[arenaSeeds[static_cast<std::size_t>(lhs)]].position;
        const CellPoint right = grid.cells[arenaSeeds[static_cast<std::size_t>(rhs)]].position;
        const float leftAngle = std::atan2(left.y - center.y, left.x - center.x);
        const float rightAngle = std::atan2(right.y - center.y, right.x - center.x);
        return leftAngle != rightAngle ? leftAngle < rightAngle : lhs < rhs;
    });
    const auto startPosition = std::ranges::find(ring, 0) - ring.begin();
    const auto exitPosition = std::ranges::find(ring, 1) - ring.begin();
    const std::size_t clockwise = static_cast<std::size_t>(
        std::abs(static_cast<std::ptrdiff_t>(startPosition - exitPosition)));
    const std::size_t ringDistance = std::min(clockwise, ring.size() - clockwise);
    if (ringDistance < 3 && ring.size() >= 6) {
        const std::size_t opposite
            = (static_cast<std::size_t>(startPosition) + ring.size() / 2)
            % ring.size();
        std::iter_swap(ring.begin() + exitPosition,
            ring.begin() + static_cast<std::ptrdiff_t>(opposite));
    }

    std::vector<std::pair<int, int>> ringEdges;
    for (std::size_t index = 0; index < ring.size(); ++index) {
        addArenaEdge(ringEdges, ring[index], ring[(index + 1) % ring.size()]);
    }
    const auto optionalLoop = *std::ranges::min_element(ringEdges,
        [&](const auto& lhs, const auto& rhs) {
            const float left = arenaSeedDistance(
                grid, arenaSeeds, lhs.first, lhs.second);
            const float right = arenaSeedDistance(
                grid, arenaSeeds, rhs.first, rhs.second);
            return left != right ? left < right : lhs < rhs;
        });
    std::vector<std::pair<int, int>> edges;
    for (const auto& edge : ringEdges) {
        if (edge != optionalLoop) {
            addArenaEdge(edges, edge.first, edge.second);
        }
    }
    for (std::size_t arena = 2; arena < arenaCount; ++arena) {
        const int branch = static_cast<int>(arena);
        if (std::ranges::find(ring, branch) != ring.end()) {
            continue;
        }
        int branchTarget = ring.front();
        float targetDistance = std::numeric_limits<float>::infinity();
        for (const int room : ring) {
            const float candidateDistance
                = arenaSeedDistance(grid, arenaSeeds, branch, room);
            if (candidateDistance < targetDistance) {
                branchTarget = room;
                targetDistance = candidateDistance;
            }
        }
        addArenaEdge(edges, branch, branchTarget);
    }
    // Optional edges follow the required tree. Use the shortest ring segment so
    // the final closure remains physically routable after the tree is embedded.
    addArenaEdge(edges, optionalLoop.first, optionalLoop.second);
    return edges;
}

std::vector<std::pair<int, int>> planMainSpine(
    const RoomGrid& grid, const std::vector<CellIndex>& arenaSeeds)
{
    const CellPoint start = grid.cells[arenaSeeds[0]].position;
    const CellPoint exit = grid.cells[arenaSeeds[1]].position;
    const float axisX = exit.x - start.x;
    const float axisY = exit.y - start.y;
    const float axisLength = std::max(std::hypot(axisX, axisY), 0.001F);
    std::vector<int> candidates;
    for (std::size_t arena = 2; arena < arenaSeeds.size(); ++arena) {
        candidates.push_back(static_cast<int>(arena));
    }
    std::ranges::sort(candidates, [&](int lhs, int rhs) {
        const CellPoint left = grid.cells[arenaSeeds[static_cast<std::size_t>(lhs)]].position;
        const CellPoint right = grid.cells[arenaSeeds[static_cast<std::size_t>(rhs)]].position;
        const float leftOffset = std::abs(axisX * (left.y - start.y)
            - axisY * (left.x - start.x)) / axisLength;
        const float rightOffset = std::abs(axisX * (right.y - start.y)
            - axisY * (right.x - start.x)) / axisLength;
        if (leftOffset != rightOffset) {
            return leftOffset > rightOffset;
        }
        return lhs < rhs;
    });
    const std::size_t branchCount = std::min<std::size_t>(
        std::max<std::size_t>(1, arenaSeeds.size() / 3),
        candidates.size() > 2 ? candidates.size() - 2 : 1);
    std::vector<int> branches(
        candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(branchCount));
    std::vector<int> spine(candidates.begin() + static_cast<std::ptrdiff_t>(branchCount),
        candidates.end());
    std::ranges::sort(spine, [&](int lhs, int rhs) {
        const CellPoint left = grid.cells[arenaSeeds[static_cast<std::size_t>(lhs)]].position;
        const CellPoint right = grid.cells[arenaSeeds[static_cast<std::size_t>(rhs)]].position;
        const float leftProjection = (left.x - start.x) * axisX
            + (left.y - start.y) * axisY;
        const float rightProjection = (right.x - start.x) * axisX
            + (right.y - start.y) * axisY;
        return leftProjection != rightProjection
            ? leftProjection < rightProjection : lhs < rhs;
    });
    spine.insert(spine.begin(), 0);
    spine.push_back(1);

    std::vector<std::pair<int, int>> edges;
    for (std::size_t index = 1; index < spine.size(); ++index) {
        addArenaEdge(edges, spine[index - 1], spine[index]);
    }
    for (std::size_t index = 0; index < branches.size(); ++index) {
        int target = spine[spine.size() / 2];
        if (index > 0) {
            float targetDistance = std::numeric_limits<float>::infinity();
            for (std::size_t spineIndex = 1;
                 spineIndex + 1 < spine.size(); ++spineIndex) {
                const float candidateDistance = arenaSeedDistance(
                    grid, arenaSeeds, branches[index], spine[spineIndex]);
                if (candidateDistance < targetDistance) {
                    target = spine[spineIndex];
                    targetDistance = candidateDistance;
                }
            }
        }
        addArenaEdge(edges, branches[index], target);
    }
    return edges;
}

std::vector<std::pair<int, int>> planTwinDistricts(
    const RoomGrid& grid, const std::vector<CellIndex>& arenaSeeds)
{
    const CellPoint start = grid.cells[arenaSeeds[0]].position;
    const CellPoint exit = grid.cells[arenaSeeds[1]].position;
    const float axisX = exit.x - start.x;
    const float axisY = exit.y - start.y;
    std::vector<int> ordered;
    for (std::size_t arena = 2; arena < arenaSeeds.size(); ++arena) {
        ordered.push_back(static_cast<int>(arena));
    }
    std::ranges::sort(ordered, [&](int lhs, int rhs) {
        const CellPoint left = grid.cells[arenaSeeds[static_cast<std::size_t>(lhs)]].position;
        const CellPoint right = grid.cells[arenaSeeds[static_cast<std::size_t>(rhs)]].position;
        const float leftProjection = (left.x - start.x) * axisX
            + (left.y - start.y) * axisY;
        const float rightProjection = (right.x - start.x) * axisX
            + (right.y - start.y) * axisY;
        return leftProjection != rightProjection
            ? leftProjection < rightProjection : lhs < rhs;
    });
    const std::size_t split = (ordered.size() + 1) / 2;
    std::vector<int> firstDistrict { 0 };
    std::vector<int> secondDistrict { 1 };
    firstDistrict.insert(firstDistrict.end(), ordered.begin(),
        ordered.begin() + static_cast<std::ptrdiff_t>(split));
    secondDistrict.insert(secondDistrict.end(),
        ordered.begin() + static_cast<std::ptrdiff_t>(split), ordered.end());
    const int firstHub = arenaMedoid(grid, arenaSeeds, firstDistrict, 0);
    const int secondHub = arenaMedoid(grid, arenaSeeds, secondDistrict, 1);

    std::vector<std::pair<int, int>> edges;
    for (const int room : firstDistrict) {
        if (room != firstHub) {
            addArenaEdge(edges, firstHub, room);
        }
    }
    for (const int room : secondDistrict) {
        if (room != secondHub) {
            addArenaEdge(edges, secondHub, room);
        }
    }
    addArenaEdge(edges, firstHub, secondHub);
    return edges;
}

std::vector<std::pair<int, int>> planDenseCoreWithSparseBranch(
    const RoomGrid& grid, const std::vector<CellIndex>& arenaSeeds)
{
    std::vector<std::pair<int, int>> edges
        = planMainSpine(grid, arenaSeeds);
    std::vector<std::vector<int>> graph(arenaSeeds.size());
    for (const auto& [first, second] : edges) {
        graph[static_cast<std::size_t>(first)].push_back(second);
        graph[static_cast<std::size_t>(second)].push_back(first);
    }
    std::pair<int, int> loop { -1, -1 };
    float loopDistance = std::numeric_limits<float>::infinity();
    for (std::size_t first = 2; first < arenaSeeds.size(); ++first) {
        const std::vector<int> distances
            = breadthFirstDistances(graph, static_cast<int>(first));
        for (std::size_t second = first + 1;
             second < arenaSeeds.size(); ++second) {
            if (distances[second] < 2
                || std::ranges::find(edges,
                       canonicalArenaEdge(
                           static_cast<int>(first), static_cast<int>(second)))
                    != edges.end()) {
                continue;
            }
            const float candidateDistance = arenaSeedDistance(grid,
                arenaSeeds,
                static_cast<int>(first),
                static_cast<int>(second));
            if (candidateDistance < loopDistance) {
                loop = { static_cast<int>(first), static_cast<int>(second) };
                loopDistance = candidateDistance;
            }
        }
    }
    if (loop.first >= 0) {
        addArenaEdge(edges, loop.first, loop.second);
    }
    return edges;
}

std::vector<std::pair<int, int>> planSpatialTreeConnections(
    const RoomGrid& grid,
    const std::vector<CellIndex>& arenaSeeds,
    std::uint32_t generationSeed)
{
    std::vector<std::pair<int, int>> edges;
    if (arenaSeeds.size() < 2) {
        return edges;
    }
    std::vector<bool> connected(arenaSeeds.size(), false);
    connected[0] = true;
    while (edges.size() + 1 < arenaSeeds.size()) {
        std::size_t selectedFirst = arenaSeeds.size();
        std::size_t selectedSecond = arenaSeeds.size();
        float selectedCost = std::numeric_limits<float>::infinity();
        for (std::size_t first = 0; first < arenaSeeds.size(); ++first) {
            if (!connected[first]) {
                continue;
            }
            for (std::size_t second = 0; second < arenaSeeds.size(); ++second) {
                if (connected[second]
                    || (arenaSeeds.size() > 2
                        && ((first == 0 && second == 1)
                            || (first == 1 && second == 0)))) {
                    continue;
                }
                const float length = arenaSeedDistance(grid,
                    arenaSeeds,
                    static_cast<int>(first),
                    static_cast<int>(second));
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
        if (selectedSecond == arenaSeeds.size()) {
            return {};
        }
        addArenaEdge(edges,
            static_cast<int>(selectedFirst),
            static_cast<int>(selectedSecond));
        connected[selectedSecond] = true;
    }
    return edges;
}

void addRequiredCycleEars(const RoomGrid& grid,
    const std::vector<CellIndex>& arenaSeeds,
    std::vector<std::pair<int, int>>& edges)
{
    const std::size_t targetEdgeCount = arenaSeeds.size() + 1;
    while (edges.size() < targetEdgeCount) {
        std::vector<std::vector<int>> graph(arenaSeeds.size());
        for (const auto& [first, second] : edges) {
            graph[static_cast<std::size_t>(first)].push_back(second);
            graph[static_cast<std::size_t>(second)].push_back(first);
        }
        std::pair<int, int> selected { -1, -1 };
        float selectedScore = -std::numeric_limits<float>::infinity();
        for (std::size_t first = 0; first < arenaSeeds.size(); ++first) {
            const std::vector<int> distances
                = breadthFirstDistances(graph, static_cast<int>(first));
            for (std::size_t second = first + 1;
                 second < arenaSeeds.size(); ++second) {
                const auto edge = canonicalArenaEdge(
                    static_cast<int>(first), static_cast<int>(second));
                if (distances[second] < 3
                    || std::ranges::find(edges, edge) != edges.end()) {
                    continue;
                }
                const float physicalDistance = std::max(
                    arenaSeedDistance(grid, arenaSeeds,
                        static_cast<int>(first), static_cast<int>(second)),
                    0.001F);
                const float score = static_cast<float>(distances[second])
                    / physicalDistance;
                if (score > selectedScore
                    || (score == selectedScore && edge < selected)) {
                    selected = edge;
                    selectedScore = score;
                }
            }
        }
        if (selected.first < 0) {
            break;
        }
        addArenaEdge(edges, selected.first, selected.second);
    }
}

std::vector<std::pair<int, int>> planArenaConnections(
    const RoomGrid& grid,
    const std::vector<CellIndex>& arenaSeeds,
    LargeMapArchetype archetype)
{
    if (arenaSeeds.size() < 5) {
        return {};
    }
    std::vector<std::pair<int, int>> edges;
    switch (archetype) {
    case LargeMapArchetype::HubAndSpokes:
        edges = planHubAndSpokes(grid, arenaSeeds);
        break;
    case LargeMapArchetype::RingAndBranches:
        edges = planRingAndBranches(grid, arenaSeeds);
        break;
    case LargeMapArchetype::MainSpine:
        edges = planMainSpine(grid, arenaSeeds);
        break;
    case LargeMapArchetype::TwinDistricts:
        edges = planTwinDistricts(grid, arenaSeeds);
        break;
    case LargeMapArchetype::DenseCoreWithSparseBranch:
        edges = planDenseCoreWithSparseBranch(grid, arenaSeeds);
        break;
    }
    const std::size_t requiredEdgeCount = arenaSeeds.size() - 1;
    if (edges.size() < requiredEdgeCount) {
        return {};
    }
    if (grid.getCellCount()
        >= detail::MINIMUM_PRODUCTION_CYCLE_GRID_CELLS) {
        addRequiredCycleEars(grid, arenaSeeds, edges);
        if (edges.size() < arenaSeeds.size() + 1) {
            return {};
        }
    }
    std::ranges::sort(edges.begin(),
        edges.begin() + static_cast<std::ptrdiff_t>(requiredEdgeCount),
        [&](const auto& lhs, const auto& rhs) {
            const float left = arenaSeedDistance(
                grid, arenaSeeds, lhs.first, lhs.second);
            const float right = arenaSeedDistance(
                grid, arenaSeeds, rhs.first, rhs.second);
            return left != right ? left < right : lhs < rhs;
        });
    return edges;
}

