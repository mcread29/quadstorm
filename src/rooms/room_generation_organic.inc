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

