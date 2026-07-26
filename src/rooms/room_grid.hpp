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
    bool buildable = false;
};

struct RoomGrid {
    std::vector<Cell> cells;
    std::vector<std::vector<CellIndex>> neighbors;
    std::vector<CellIndex> entranceCandidates;

    std::size_t getCellCount() const { return cells.size(); }
    std::span<const Cell> getCells() const { return cells; }
    std::span<const std::vector<CellIndex>> getNeighbors() const { return neighbors; }
};

} // namespace stalberg::rooms
