#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace stalberg {

class StalbergGrid;
struct Point;

std::optional<std::size_t> findDualCellAtPoint(const StalbergGrid& grid, Point point);

void drawGrid(
    const StalbergGrid& grid,
    bool drawCenters,
    float cameraZoom,
    const std::vector<bool>& generatedCells,
    std::optional<std::size_t> hoveredCell);

} // namespace stalberg
