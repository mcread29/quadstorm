#include "grid/stalberg_grid.hpp"
#include "grid_renderer.hpp"
#include "integration/room_grid_adapter.hpp"
#include "rooms/room_generator.hpp"
#include "rooms/room_layout.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);
    grid.relaxToCompletion();

    stalberg::GridRendererCache cache;
    cache.rebuildGrid(grid);
    bool valid = check(cache.dualGrid().cells.size() == grid.getVertexCount(),
        "renderer cache retains one dual polygon per grid cell");
    for (std::size_t cell = 0; cell < cache.dualGrid().cells.size(); ++cell) {
        valid &= check(cache.findCellAtPoint(
                           cache.dualGrid().cells[cell].center)
                == cell,
            "cached dual geometry is reused for hit-testing");
    }

    const auto roomGrid = stalberg::makeRoomGrid(grid, cache.dualGrid());
    const auto layout = stalberg::rooms::RoomGenerator {}.generate(
        roomGrid, 1, stalberg::rooms::RoomGenerationMethod::ShooterLayout);
    cache.rebuildRooms(layout);

    bool rejectedMisalignedLayout = false;
    try {
        cache.rebuildRooms(stalberg::rooms::RoomLayout {});
    } catch (const std::invalid_argument&) {
        rejectedMisalignedLayout = true;
    }
    valid &= check(rejectedMisalignedLayout,
        "renderer cache rejects a room layout from a different grid");
    return valid ? 0 : 1;
}
