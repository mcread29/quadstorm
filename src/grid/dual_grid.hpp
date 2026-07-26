#pragma once

#include "grid/stalberg_grid.hpp"

#include <optional>
#include <vector>

namespace stalberg {

struct DualCell {
    Point center;
    std::vector<Point> polygon;
    float area = 0.0F;
    float clearance = 0.0F;
};

struct DualConnection {
    Edge cells;
    Point first;
    Point second;
    float length = 0.0F;
};

struct DualGrid {
    std::vector<Point> quadCenters;
    std::vector<DualCell> cells;
    std::vector<DualConnection> connections;
};

DualGrid buildDualGrid(const StalbergGrid& grid);
std::optional<VertexIndex> findDualCellAtPoint(const DualGrid& dual, Point point);

} // namespace stalberg
