#pragma once

#include <cstddef>
#include <optional>

namespace stalberg {

class RoomLayout;
class StalbergGrid;
struct Point;

std::optional<std::size_t> findDualCellAtPoint(const StalbergGrid& grid, Point point);

void drawGrid(
    const StalbergGrid& grid,
    const RoomLayout& rooms,
    bool drawCenters,
    float cameraZoom,
    std::optional<std::size_t> hoveredCell);

} // namespace stalberg
