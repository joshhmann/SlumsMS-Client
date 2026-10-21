#pragma once
// Newer pet equips crash the v83 client (1.4.0 QA: scrolling Pet Equip in the Cash Shop). Header-only so
// tests/petequip_flag_test.cpp can check the arithmetic without the client; petequipflag.cpp installs the fix.
//
// ROOT CAUSE. The client remembers which pets a pet equip fits as a set of 128-bit flags, one bit per pet:
// bit (petId % 1000). The flag is a 16-byte stack object (ctor 0x00873A21 zeroes it); its setter
// 0x00873AD5 (__thiscall, (bit, value), ret 8) does
//     dword[bit >> 5] |= 1 << (31 - (bit & 31))
// with NO bounds check -- its getter 0x00873AAD does check `bit > 0x7F`. Stock v83 pets are all below
// 5000128, so it never mattered. This client's data has 845 pets (up to 5002111), and the later pet equips list
// them as children of Character/PetEquip/0180xxxx.img: any pet with id % 1000 >= 128 writes past the flag.
//
//  * CItemInfo's equip loader 0x005CAC3D (via GetEquipItem 0x005CA785), for pet equips with id % 1000 < 100,
//    walks those children and sets bit (child % 1000) in the flag at [ebp-0xA8] (call at 0x005CD26C). Pet
//    5000312 sets 0x80 in the ComPtr at [ebp-0x84]; the epilogue's Release of it (0x005CF54C -> 0x0040262E)
//    reads address 0x80 -- exactly the fault in the 1.4.0 crash.log (0x00402634, read of 0x00000080). Other
//    pets corrupt other locals of that frame (5000561 -> [ebp-0x64], 5000594 -> [ebp-0x60], ...).
//    It is the PET id, not the newer art: 1802086 and 1802094-98 carry the same `_hash` canvases and drew
//    fine; every equip that faulted in integration.log (1802085, 1802087-93, 1802099) lists pet 5000561.
//  * The wear check 0x0046D408 (does this pet equip fit pet N?) builds the same flag for a pet at [ebp-0x10]
//    (call at 0x0046D453); a pet with id % 1000 >= 128 writes into the saved EBP / return address there.
//
// THE FIX: the only two callers of the setter outside the flag class are those two calls, and both are
// redirected to bounds-checked stubs. A pet that does not fit in 128 bits is LEFT OUT of a pet equip's list
// (the loader skips straight to its next child, 0x005CD2C8) and is never found by the wear check (the flag
// stays zero, which the loader never stores). The server still decides what a pet can actually wear.

namespace PetEquipFlag {

constexpr unsigned kBits = 128;                 // the flag is 4 dwords; the getter's own bound is bit <= 0x7F
constexpr int kFlagFrameOffset = -0xA8;          // the loader's flag, [ebp-0xA8]

constexpr bool Fits(int petId) { return petId >= 0 && static_cast<unsigned>(petId % 1000) < kBits; }

// Where the unchecked setter writes for pet `petId`, relative to the loader's EBP, and with what mask.
constexpr int LoaderWriteOffset(int petId) { return kFlagFrameOffset + 4 * ((petId % 1000) >> 5); }
constexpr unsigned SetterMask(int petId) { return 1u << (31 - ((petId % 1000) & 31)); }

// The two call sites and where the loader goes for its next child.
constexpr unsigned long kAddr_SetBit         = 0x00873AD5;
constexpr unsigned long kAddr_LoaderSetBit   = 0x005CD26C;   // E8 64 68 2A 00
constexpr unsigned long kAddr_LoaderNextPet  = 0x005CD2C8;   // lea ecx,[ebp-0x34]; call; jmp loop
constexpr unsigned long kAddr_WearCheckSetBit = 0x0046D453;  // E8 7D 66 40 00

}  // namespace PetEquipFlag
