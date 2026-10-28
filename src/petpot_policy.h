#pragma once
// ============================================================
// petpot_policy.h -- the pet auto-potion window's rules (ticket 122). SCAFFOLD.
//
// No Windows, no client: tests/petpot_policy_test.cpp checks it. v83 already has pet HP/MP potions: the item ids
// live in CFuncKeyMappedMan (0x00BED5A0) +0x3C0/+0x3C4 and are set through 0x0058E03C/0x0058E0A8, which also sends
// CP 0x87 (the server keeps them as keymap 91/92 and sends them back at login). The thresholds are the System
// Options HP/MP alert (CConfig+0x80/+0x84, 5% steps, per PC). The window is a friendlier front for both; the server
// disconnects a player who sets a potion they don't hold, so the window must check the inventory first.
// Research note: docs/research-118-122.md.
// ============================================================

namespace PetPot {

// The alert setting moves in 5% steps from 0 (off) to 95 (stock range); snap whatever the slider says.
inline int SnapThreshold(int pct) {
    if (pct <= 0) return 0;
    if (pct >= 95) return 95;
    return (pct + 2) / 5 * 5;
}

// HP potions are Use items (2000000-2029999 cover the stock and cash potions); the window lists only what the
// player holds, and only lets them pick a held one (the server's rule).
inline bool IsPotionLike(int itemId) { return itemId >= 2000000 && itemId < 2030000; }

inline bool MaySet(int itemId, int countHeld) { return itemId == 0 || (IsPotionLike(itemId) && countHeld > 0); }

}  // namespace PetPot
