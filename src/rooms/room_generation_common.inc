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

