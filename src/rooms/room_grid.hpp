#pragma once

#include <cstddef>
#include <span>
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
    // Kept as a lightweight topology view for graph algorithms and callers that do
    // not need geometry. Connections carry the matching physical edge metadata.
    std::vector<std::vector<CellIndex>> neighbors;
    std::vector<std::vector<CellConnection>> connections;
    std::vector<CellIndex> entranceCandidates;

    std::size_t getCellCount() const { return cells.size(); }
    std::span<const Cell> getCells() const { return cells; }
    std::span<const std::vector<CellIndex>> getNeighbors() const { return neighbors; }
    std::span<const std::vector<CellConnection>> getConnections() const
    {
        return connections;
    }
};

} // namespace stalberg::rooms
