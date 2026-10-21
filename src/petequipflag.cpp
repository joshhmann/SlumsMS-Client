// ============================================================
// petequipflag.cpp: keep newer pets out of the v83 client's 128-bit pet flag
//
// Pet equips that list a pet with id % 1000 >= 128 overflow a 16-byte stack flag in the equip loader and in
// the pet-equip wear check; petequip_flag.h has the full analysis. This redirects the two calls to the
// unchecked setter (0x00873AD5) to stubs that only let bits 0..127 through:
//
//   loader     0x005CD26C   bit >= 128 -> drop the call's frame and continue with the next child (0x005CD2C8),
//                           which is where the loader also goes after the "info" / "weekly" children; ebx, esi,
//                           edi and the EH state ([ebp-4] = 0xA8) are the same there as on the normal path.
//   wear check 0x0046D453   bit >= 128 -> return without setting it (ret 8, like the setter): the flag stays
//                           zero, which the loader never stores, so that pet is not found.
//
// Every byte is checked first; on any mismatch nothing is patched and the client behaves as 1.4.0 did.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "petequip_flag.h"

#include <cstring>

static uintptr_t g_petFlagSetBit = PetEquipFlag::kAddr_SetBit;
static uintptr_t g_petFlagLoaderNext = PetEquipFlag::kAddr_LoaderNextPet;

// [esp] = return address, [esp+4] = bit, [esp+8] = value; the setter is __thiscall with ret 8.
static void __declspec(naked) PetFlag_LoaderSetBit() {
    __asm {
        cmp     dword ptr [esp + 4], 128
        jae     skip
        jmp     dword ptr [g_petFlagSetBit]
    skip:
        add     esp, 12
        jmp     dword ptr [g_petFlagLoaderNext]
    }
}

static void __declspec(naked) PetFlag_WearCheckSetBit() {
    __asm {
        cmp     dword ptr [esp + 4], 128
        jae     skip
        jmp     dword ptr [g_petFlagSetBit]
    skip:
        ret     8
    }
}

static bool BytesAre(uintptr_t address, const char* bytes, size_t size) {
    return std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0;
}

void AttachPetEquipFlagMod() {
    using namespace PetEquipFlag;
    // The setter itself, the loader's call with the instructions around it (push 1; lea ecx,[ebp-0xA8];
    // push edx; call), the loader's next-child path, and the wear check's call (push 1; lea ecx,[ebp-0x10];
    // push edx; call ctor; mov ecx,eax; call setter).
    static const char kSetter[] = "\x56\x8B\x74\x24\x08\x8B\xC1\x6A\x1F\x59\x8B\xD6\x23\xF1\x2B\xCE";
    static const char kLoader[] = "\x6A\x01\x8D\x8D\x58\xFF\xFF\xFF\x52\xE8\x64\x68\x2A\x00";
    static const char kNext[]   = "\x8D\x4D\xCC\xE8\xEA\x22\x00\x00\xE9\xC6\xFE\xFF\xFF";
    static const char kWear[]   = "\x6A\x01\x8D\x4D\xF0\x52\xE8\xD0\x65\x40\x00\x8B\xC8\xE8\x7D\x66\x40\x00";
    if (!BytesAre(kAddr_SetBit, kSetter, sizeof(kSetter) - 1) ||
        !BytesAre(kAddr_LoaderSetBit - 9, kLoader, sizeof(kLoader) - 1) ||
        !BytesAre(kAddr_LoaderNextPet, kNext, sizeof(kNext) - 1) ||
        !BytesAre(kAddr_WearCheckSetBit - 13, kWear, sizeof(kWear) - 1)) {
        DebugMessage("Pet equip flag: unexpected client bytes; newer pet equips are not guarded");
        return;
    }
    PatchCall(kAddr_LoaderSetBit, &PetFlag_LoaderSetBit);
    PatchCall(kAddr_WearCheckSetBit, &PetFlag_WearCheckSetBit);
    DebugMessage("Pet equip flag: pets with id %% 1000 >= %u are kept out of the 128-bit pet flag", kBits);
}
