#pragma once
// Coloring Prism wire format helpers (MAPLEROOT_COLOR_PRISM), header-only so the native tests can
// run them without the client. LP 0x372F WEAPON_TINT_SYNC, after the opcode:
//   1 snapshot:     count(1), count x entry                     -- the local player's dyed items
//   2 result:       code(1)                                     -- WeaponTintResult, 1 ok .. 5 failed
//   3 remote table: players(1), per player name(u16 len + bytes) items(1) items x entry
// entry = key(4) hue(2, signed) chroma(1, signed) bright(1, signed), little-endian. key is the equip
// id (body), id + 10000000 (its effect layer), 1 (hair) or 2 (face). The server's
// server.colorprism.ColorPrismPackets writes these bytes.
#include "weapontint.h"
#include <cstddef>
#include <cstdint>

constexpr uint8_t kTintSync_Snapshot    = 1;
constexpr uint8_t kTintSync_Result      = 2;
constexpr uint8_t kTintSync_RemoteTable = 3;
constexpr size_t  kTintEntryBytes       = 8;

// Decodes one 8-byte entry. Returns whether it is a tint to keep: a positive key and an absolute
// hue in -360..-1. Anything else -- in particular the package's old 0..359 relative hues, which
// this decoder used to zero into hue 0 (red) -- is dropped; the caller still advances 8 bytes.
inline bool WeaponTint_DecodeEntry(const unsigned char* b, int& key, WeaponTint& t) {
    key = static_cast<int>(static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8)
                           | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24));
    t.hue    = static_cast<short>(static_cast<uint16_t>(b[4] | (b[5] << 8)));
    t.chroma = static_cast<signed char>(b[6]);
    t.bright = static_cast<signed char>(b[7]);
    return key > 0 && t.IsHueEncodingValid();
}
