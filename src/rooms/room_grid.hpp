#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace stalberg::rooms {

using CellIndex = std::size_t;

struct CellPoint {
    float x;
    float y;
};

struct Cell {
    CellPoint position;
    float area = 0.0F;
    float clearance = 0.0F;
    bool buildable = false;
};

struct CellConnection {
    CellIndex cell;
    float distance = 0.0F;
    float sharedBoundaryLength = 0.0F;
};

struct RoomGrid {
    std::vector<Cell> cells;
    // The sorted connection lists are the only stored topology. Index-only
    // callers use neighbors(), which is a non-owning projection of this data.
    std::vector<std::vector<CellConnection>> connections;
    std::vector<CellIndex> entranceCandidates;

    void setConnections(std::vector<std::vector<CellConnection>> value)
    {
        for (auto& cellConnections : value) {
            std::ranges::sort(cellConnections, {}, &CellConnection::cell);
        }
        connections = std::move(value);
    }

    auto neighbors(CellIndex cell) const
    {
        return connections[cell]
            | std::views::transform(
                [](const CellConnection& connection) { return connection.cell; });
    }

    std::size_t getCellCount() const { return cells.size(); }
    std::span<const Cell> getCells() const { return cells; }
    std::span<const std::vector<CellConnection>> getConnections() const
    {
        return connections;
    }
};

// Includes geometry and entrance policy because both affect seeded generation.
// Equivalent connection and entrance ordering produces the same value.
std::uint64_t canonicalTopologyFingerprint(const RoomGrid& grid);

} // namespace stalberg::rooms
