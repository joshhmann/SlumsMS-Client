#include "../src/quickslot_patch.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

void require(bool value) { if (!value) throw std::runtime_error("quickslot instruction contract failed"); }
int main() {
    const std::array<unsigned char, 6> original = {{0x81, 0xFE, 0xF4, 0x2D, 0xBE, 0x00}};
    require(integration::QuickslotTableCompare(0x00BE2DB0, 8 * 8) == original);
    for (std::uint32_t base : {0x10000000u, 0x70000000u, 0x90000000u}) {
        const auto patch = integration::QuickslotTableCompare(base, 26 * 8);
        require(patch[0] == 0x81 && patch[1] == 0xFE); // CMP ESI, imm32, not CMP [ESI], imm32.
        std::uint32_t end = 0;
        std::memcpy(&end, patch.data() + 2, sizeof(end));
        unsigned visited = 0;
        std::uint32_t cursor = base + 4;
        do {
            require(cursor >= base + 4 && cursor + 4 <= base + 26 * 8);
            ++visited; cursor += 8;
        } while (static_cast<std::int32_t>(cursor) < static_cast<std::int32_t>(end));
        require(visited == 26);
    }
    for (const auto invalid : {0u, 7u, 9u}) {
        bool rejected = false;
        try { integration::QuickslotTableCompare(0x10000000, invalid); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected);
    }
    bool rejected = false;
    try { integration::QuickslotTableCompare(0xFFFFFF80, 26 * 8); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected);
    std::cout << "PASS: original encoding, 26 bounded POINT visits, signed addresses and invalid tables\n";
}
