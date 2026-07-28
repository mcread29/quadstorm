// Internal implementation fragment included from room_generator.cpp inside its
// anonymous namespace. It owns entrance selection and the organic, branching,
// and shooter generation strategies while keeping their helpers private.

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
    const CellConnection* connection = findConnection(grid, first, second);
    const Cell& destination = grid.cells[second];
    const float directDistance = distance(grid.cells[first].position, destination.position);
    const float edgeDistance = connection == nullptr
        ? directDistance
        : std::max(connection->distance, 0.001F);
    const float preferredClearance = edgeDistance * 0.32F;
    const float clearancePenalty = preferredClearance <= 0.0F
        ? 0.0F
        : std::max(0.0F, preferredClearance - destination.clearance)
            / preferredClearance;
    const float portalWidth = connection == nullptr
        ? edgeDistance
        : connection->sharedBoundaryLength;
    const float preferredWidth = edgeDistance * 0.55F;
    const float widthPenalty = preferredWidth <= 0.0F
        ? 0.0F
        : std::max(0.0F, preferredWidth - portalWidth) / preferredWidth;
    return edgeDistance
        * (1.0F + clearancePenalty * 2.5F + widthPenalty * widthPenalty * 1.8F);
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
            const CellConnection* connection
                = findConnection(grid, current, neighbor);
            const float width = connection == nullptr
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
    float cellScale,
    const EntranceSelection& entranceSelection,
    std::uint32_t generationSeed,
    std::mt19937& random,
    std::vector<int>& assignments,
    std::vector<GeneratedRoom>& rooms,
    std::vector<CellIndex>& connectedEntrances)
{
    const auto cells = grid.getCells();
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
        * 0.36F / static_cast<float>(std::max<std::size_t>(desiredArenaCount, 1));
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
    const auto plannedConnections = usesSmallMapRecipe
        ? planRecipeArenaConnections(requestedSeeds.size(), smallMapRecipe)
        : planArenaConnections(grid, requestedSeeds, generationSeed);
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
                ? 2.0F
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
    bool loopAdded = false;
    for (std::size_t connectionIndex = 0;
         connectionIndex < plannedConnections.size(); ++connectionIndex) {
        const bool requiredTreeEdge = connectionIndex + 1 < arenaSeeds.size();
        if (!requiredTreeEdge && loopAdded) {
            continue;
        }
        const auto [first, second] = plannedConnections[connectionIndex];
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
        if (!connected && requiredTreeEdge) {
            result.complete = false;
        } else if (connected && !requiredTreeEdge) {
            loopAdded = true;
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



