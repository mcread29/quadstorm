#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace stalberg::rooms {

enum class MissionEdgePurpose : std::uint8_t {
    Primary,
    Cycle,
    Shortcut,
    Quest,
    Exterior
};

struct MissionNodeBrief {
    int arena = 0;
    std::size_t minimumDegree = 1;
};

struct MissionEdgeBrief {
    std::size_t id = 0;
    int firstArena = 0;
    int secondArena = 0;
    MissionEdgePurpose purpose = MissionEdgePurpose::Primary;
    bool required = true;
    std::uint8_t unlockStage = 0;

    bool operator==(const MissionEdgeBrief&) const = default;
};

struct MissionGraphBrief {
    std::vector<MissionNodeBrief> nodes;
    std::vector<MissionEdgeBrief> edges;
};

} // namespace stalberg::rooms
