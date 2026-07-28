#pragma once

#include "rooms/room_generation_validation.hpp"

#include <cstdint>

namespace stalberg::rooms::detail {

inline float unitNoise(std::uint32_t seed, std::uint64_t salt)
{
    const std::uint64_t value = mix(
        (static_cast<std::uint64_t>(seed) << 32U) ^ mix(salt));
    return static_cast<float>(value & 0xffffU) / 65535.0F;
}

} // namespace stalberg::rooms::detail
