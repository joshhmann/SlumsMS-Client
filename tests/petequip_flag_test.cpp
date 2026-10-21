// Newer pet equips (petequip_flag.h): which pets overflow the client's 128-bit pet flag, and where the
// unchecked setter would write for them in the equip loader's frame. The offsets are the ones the 1.4.0
// crash proves: pet 5000312 ORs 0x80 into the ComPtr at [ebp-0x84] whose Release read address 0x80.
#include "../src/petequip_flag.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace PetEquipFlag;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        // Stock v83 pets fit; the getter's own bound is 0x7F.
        Check(Fits(5000000) && Fits(5000101) && Fits(5000102) && Fits(5000127), "stock pets fit");
        Check(!Fits(5000128) && !Fits(5000312) && !Fits(5000561) && !Fits(5000999), "newer pets do not");
        Check(Fits(5001000) && Fits(5001100) && !Fits(5002200), "the bit is id % 1000");
        Check(!Fits(-1), "a negative id never fits");

        // In range: the write stays inside the 16-byte flag at [ebp-0xA8].
        Check(LoaderWriteOffset(5000127) == -0xA8 + 12, "bit 127 is the flag's last dword");

        // The 1.4.0 fault: 5000312 -> [ebp-0x84] |= 0x80 (read of 0x00000080 in its Release).
        Check(LoaderWriteOffset(5000312) == -0x84, "5000312 lands on [ebp-0x84]");
        Check(SetterMask(5000312) == 0x80u, "5000312's mask is 0x80");
        // 5000561, listed by every equip that faulted, lands on the ComPtr at [ebp-0x64].
        Check(LoaderWriteOffset(5000561) == -0x64, "5000561 lands on [ebp-0x64]");
        Check(LoaderWriteOffset(5000594) == -0x60, "5000594 lands on [ebp-0x60]");
        // The highest bit reaches [ebp-0x2C]: the whole damage window is [ebp-0x98, ebp-0x28).
        Check(LoaderWriteOffset(5000999) == -0x2C, "bit 999");

        Check(kAddr_LoaderSetBit == 0x005CD26C && kAddr_WearCheckSetBit == 0x0046D453, "call sites");
    } catch (const std::exception& e) {
        std::cerr << "petequip_flag: " << e.what() << '\n';
        return 1;
    }
    std::cout << "petequip_flag: ok\n";
    return 0;
}
