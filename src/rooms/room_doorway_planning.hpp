#pragma once

#include "rooms/room_generation_random.hpp"
#include "rooms/room_graph_operations.hpp"
#include "rooms/room_layout.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <ranges>
#include <utility>
#include <vector>

namespace stalberg::rooms::detail {

bool generatePlannedDoorways(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    std::uint32_t generationSeed,
    const std::vector<int>& assignments,
    const std::vector<std::pair<int, int>>& requiredConnections,
    std::vector<Doorway>& doorways)
{
    using CellPair = std::pair<CellIndex, CellIndex>;
    doorways.clear();
    float averageWidth = 0.0F;
    std::size_t widthCount = 0;
    for (CellIndex cell = 0; cell < adjacency.size(); ++cell) {
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor > cell) {
                averageWidth += sharedBoundaryLength(grid, cell, neighbor);
                ++widthCount;
            }
        }
    }
    averageWidth = widthCount == 0
        ? 1.0F
        : averageWidth / static_cast<float>(widthCount);
    for (const auto& [firstRegion, secondRegion] : requiredConnections) {
        std::vector<CellPair> candidates;
        for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
            if (assignments[cell] != firstRegion
                && assignments[cell] != secondRegion) {
                continue;
            }
            for (const CellIndex neighbor : adjacency[cell]) {
                if (neighbor <= cell) {
                    continue;
                }
                const int neighborRegion = assignments[neighbor];
                if (assignments[cell] == firstRegion
                    && neighborRegion == secondRegion) {
                    candidates.emplace_back(cell, neighbor);
                } else if (assignments[cell] == secondRegion
                    && neighborRegion == firstRegion) {
                    candidates.emplace_back(neighbor, cell);
                }
            }
        }
        if (candidates.empty()) {
            doorways.clear();
            return false;
        }

        const auto best = std::ranges::max_element(
            candidates, {}, [&](const CellPair& candidate) {
                const float width
                    = sharedBoundaryLength(grid, candidate.first, candidate.second);
                const float clearance = std::min(
                    grid.cells[candidate.first].clearance,
                    grid.cells[candidate.second].clearance);
                const float noise = unitNoise(generationSeed,
                    mix(candidate.first) ^ (mix(candidate.second) << 1U));
                return width + clearance * 0.35F + noise * 0.05F;
            });
        const float width = sharedBoundaryLength(grid, best->first, best->second);
        const float clearance = std::min(
            grid.cells[best->first].clearance,
            grid.cells[best->second].clearance);
        const float quality = std::clamp(
            (width + clearance * 0.35F) / std::max(averageWidth * 1.25F, 0.001F),
            0.0F,
            1.0F);
        doorways.push_back(Doorway {
            firstRegion,
            best->first,
            secondRegion,
            best->second,
            width,
            quality
        });
    }
    return true;
}

void generateDoorways(
    const RoomGrid& grid,
    const std::vector<std::vector<CellIndex>>& adjacency,
    std::uint32_t generationSeed,
    const std::vector<int>& assignments,
    std::size_t roomCount,
    std::vector<Doorway>& doorways)
{
    using RegionPair = std::pair<int, int>;
    using CellPair = std::pair<CellIndex, CellIndex>;
    struct Contact {
        RegionPair regions;
        std::vector<CellPair> candidates;
        float quality = 0.0F;
        bool selected = false;
    };

    std::map<RegionPair, std::vector<CellPair>> sharedBoundaries;
    for (CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int firstRegion = assignments[cell];
        if (firstRegion == EMPTY_CELL) {
            continue;
        }
        for (const CellIndex neighbor : adjacency[cell]) {
            if (neighbor <= cell) {
                continue;
            }
            const int secondRegion = assignments[neighbor];
            if (secondRegion == EMPTY_CELL || secondRegion == firstRegion) {
                continue;
            }
            if (firstRegion < secondRegion) {
                sharedBoundaries[{ firstRegion, secondRegion }].emplace_back(cell, neighbor);
            } else {
                sharedBoundaries[{ secondRegion, firstRegion }].emplace_back(neighbor, cell);
            }
        }
    }

    std::vector<Contact> contacts;
    contacts.reserve(sharedBoundaries.size());
    for (auto& [regions, candidates] : sharedBoundaries) {
        float bestQuality = 0.0F;
        for (const CellPair& candidate : candidates) {
            const float width = sharedBoundaryLength(grid, candidate.first, candidate.second);
            const float clearance = std::min(
                grid.cells[candidate.first].clearance,
                grid.cells[candidate.second].clearance);
            bestQuality = std::max(bestQuality, width + clearance * 0.35F);
        }
        contacts.push_back(Contact { regions, std::move(candidates), bestQuality });
    }

    std::ranges::sort(contacts, [&](const Contact& lhs, const Contact& rhs) {
        if (lhs.quality != rhs.quality) {
            return lhs.quality > rhs.quality;
        }
        const auto tieBreak = [&](const Contact& contact) {
            return mix(static_cast<std::uint64_t>(generationSeed)
                ^ mix(static_cast<std::uint64_t>(contact.regions.first))
                ^ (mix(static_cast<std::uint64_t>(contact.regions.second)) << 1U));
        };
        return tieBreak(lhs) < tieBreak(rhs);
    });

    DisjointSet regions(roomCount);
    std::vector<std::vector<int>> roomGraph(roomCount);
    const auto selectContact = [&](Contact& contact) {
        contact.selected = true;
        roomGraph[static_cast<std::size_t>(contact.regions.first)]
            .push_back(contact.regions.second);
        roomGraph[static_cast<std::size_t>(contact.regions.second)]
            .push_back(contact.regions.first);
    };

    std::size_t treeEdges = 0;
    for (Contact& contact : contacts) {
        const std::size_t first = static_cast<std::size_t>(contact.regions.first);
        const std::size_t second = static_cast<std::size_t>(contact.regions.second);
        if (!regions.unite(first, second)) {
            continue;
        }
        selectContact(contact);
        ++treeEdges;
        if (treeEdges + 1 >= roomCount) {
            break;
        }
    }

    const auto graphDistance = [&](int start, int destination) {
        const std::vector<int> distances
            = breadthFirstDistances(roomGraph, start);
        return distances[static_cast<std::size_t>(destination)];
    };

    const std::size_t desiredLoops = roomCount < 4
        ? 0
        : std::min<std::size_t>(4, std::max<std::size_t>(1, roomCount / 6));
    for (std::size_t loop = 0; loop < desiredLoops; ++loop) {
        Contact* selected = nullptr;
        float bestScore = -std::numeric_limits<float>::infinity();
        for (Contact& contact : contacts) {
            if (contact.selected) {
                continue;
            }
            const int cycleLength = graphDistance(
                contact.regions.first, contact.regions.second);
            if (cycleLength < 2) {
                continue;
            }
            const float noise = unitNoise(generationSeed,
                static_cast<std::uint64_t>(contact.regions.first) * 8191U
                    + static_cast<std::uint64_t>(contact.regions.second));
            const float score = static_cast<float>(cycleLength) * 100.0F
                + contact.quality + noise * 4.0F;
            if (score > bestScore) {
                selected = &contact;
                bestScore = score;
            }
        }
        if (selected == nullptr) {
            break;
        }
        selectContact(*selected);
    }

    for (Contact& contact : contacts) {
        if (!contact.selected || contact.candidates.empty()) {
            continue;
        }
        const auto best = std::ranges::max_element(
            contact.candidates, {}, [&](const CellPair& candidate) {
                const float width
                    = sharedBoundaryLength(grid, candidate.first, candidate.second);
                const float clearance = std::min(
                    grid.cells[candidate.first].clearance,
                    grid.cells[candidate.second].clearance);
                const float noise = unitNoise(generationSeed,
                    mix(candidate.first) ^ (mix(candidate.second) << 1U));
                return width + clearance * 0.35F + noise * 0.05F;
            });
        const float width = sharedBoundaryLength(grid, best->first, best->second);
        const float clearance = std::min(
            grid.cells[best->first].clearance,
            grid.cells[best->second].clearance);
        const float quality = contact.quality <= 0.0F
            ? 0.0F
            : std::clamp((width + clearance * 0.35F) / contact.quality,
                0.0F,
                1.0F);
        doorways.push_back(Doorway {
            contact.regions.first,
            best->first,
            contact.regions.second,
            best->second,
            width,
            quality
        });
    }
}


} // namespace stalberg::rooms::detail
