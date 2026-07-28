#pragma once

#include "grid/dual_grid.hpp"

#include <cstddef>
#include <memory>
#include <optional>

namespace stalberg {

class StalbergGrid;
namespace rooms {
class RoomLayout;
}
struct Point;

class GridRendererCache {
public:
    GridRendererCache();
    ~GridRendererCache();
    GridRendererCache(GridRendererCache&&) noexcept;
    GridRendererCache& operator=(GridRendererCache&&) noexcept;
    GridRendererCache(const GridRendererCache&) = delete;
    GridRendererCache& operator=(const GridRendererCache&) = delete;

    void rebuildGrid(const StalbergGrid& grid);
    void rebuildRooms(const rooms::RoomLayout& rooms);

    const DualGrid& dualGrid() const;
    std::optional<std::size_t> findCellAtPoint(Point point) const;
    void draw(const StalbergGrid& grid, const rooms::RoomLayout& rooms,
        bool drawCenters, float cameraZoom,
        std::optional<std::size_t> hoveredCell) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace stalberg
