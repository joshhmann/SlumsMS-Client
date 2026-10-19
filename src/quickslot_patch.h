#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace integration {
inline std::array<unsigned char, 6> QuickslotTableCompare(std::uint32_t table, std::uint32_t bytes) {
    // ESI visits each POINT's Y field: start=table+4, stride=8.
    if (!bytes || bytes % 8 != 0 || static_cast<std::uint64_t>(table) + bytes + 4 > (std::numeric_limits<std::uint32_t>::max)())
        throw std::invalid_argument("Invalid quickslot POINT table");
    const std::uint32_t end = table + bytes + 4;
    return {{0x81, 0xFE, static_cast<unsigned char>(end), static_cast<unsigned char>(end >> 8),
        static_cast<unsigned char>(end >> 16), static_cast<unsigned char>(end >> 24)}};
}
}
