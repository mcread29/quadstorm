#pragma once

#include "rooms/room_grid.hpp"
#include "rooms/room_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace stalberg::rooms {

namespace detail {
struct PreparedGenerationContext;
}

struct RoomGenerationOptions {
    RoomGenerationMethod method = RoomGenerationMethod::ShooterLayout;
    std::size_t candidateCount = 6;
    std::optional<SmallMapRecipe> smallMapRecipe;
};

class RoomGenerator {
public:
    RoomLayout generate(
        const RoomGrid& grid,
        std::uint32_t seed,
        RoomGenerationMethod method = RoomGenerationMethod::ShooterLayout) const;
    RoomLayout generate(
        const RoomGrid& grid,
        std::uint32_t seed,
        const RoomGenerationOptions& options) const;

private:
    RoomLayout generateCandidate(
        const detail::PreparedGenerationContext& prepared,
        std::uint32_t requestedSeed,
        std::uint32_t variantSeed,
        RoomGenerationMethod method,
        SmallMapRecipe smallMapRecipe,
        const std::vector<CellIndex>& entranceOrder,
        std::size_t entranceTargetCount) const;
};

} // namespace stalberg::rooms
