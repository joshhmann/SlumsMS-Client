#pragma once
// ============================================================
// damagefmt_policy.h -- short damage numbers and their size (ticket 119). SCAFFOLD.
//
// No Windows, no client: tests/damagefmt_policy_test.cpp checks it. CAnimationDisplayer::Effect_HP (0x00437D0F)
// formats the number with "%d" (call at 0x00437DFB) and picks each digit sprite by c - '0'. Swapping that one call
// for ShortDamage + EncodeGlyphs lets '.', 'K', 'M', 'B' reach sprites "10".."13" of each digit set (32 new glyphs
// in Custom.wz) with no other code change. Numbers under 10,000 stay exactly as stock. Truncated, not rounded, so a
// number never reads higher than it is. Research note: docs/research-119-120.md.
// ============================================================

#include <cstdio>
#include <cstring>

namespace DamageFmt {

constexpr int kMinPct = 60, kMaxPct = 150;

inline int ClampPct(int pct) { return pct < kMinPct ? kMinPct : pct > kMaxPct ? kMaxPct : pct; }

// 12345 -> "12.3K", 1234567 -> "1.23M", 2147483647 -> "2.14B": three significant digits, trailing ".0" dropped.
inline void ShortDamage(long long v, char* out, size_t cap) {
    if (v < 10000) {
        std::snprintf(out, cap, "%lld", v);
        return;
    }
    const char* suffix = "K";
    long long unit = 1000;
    if (v >= 1000000000LL) { suffix = "B"; unit = 1000000000LL; }
    else if (v >= 1000000LL) { suffix = "M"; unit = 1000000LL; }
    const long long whole = v / unit;
    char frac[3] = {};
    int decimals = whole >= 100 ? 0 : whole >= 10 ? 1 : 2;
    long long rest = v % unit;
    for (int i = 0; i < decimals; ++i) {
        rest *= 10;
        frac[i] = static_cast<char>('0' + rest / unit);
        rest %= unit;
    }
    while (decimals > 0 && frac[decimals - 1] == '0') frac[--decimals] = 0;
    if (decimals) std::snprintf(out, cap, "%lld.%s%s", whole, frac, suffix);
    else std::snprintf(out, cap, "%lld%s", whole, suffix);
}

// The renderer takes c - '0' as the sprite index: map '.', 'K', 'M', 'B' to ':', ';', '<', '=' (sprites 10-13).
inline void EncodeGlyphs(char* s) {
    for (; *s; ++s) {
        switch (*s) {
        case '.': *s = ':'; break;
        case 'K': *s = ';'; break;
        case 'M': *s = '<'; break;
        case 'B': *s = '='; break;
        default: break;
        }
    }
}

// Sites in MapleStory.exe (86e4e6be...), bytes verified: the "%d" sprintf call inside Effect_HP, and the two canvas
// insert calls a size factor would wrap.
struct Site { unsigned addr; unsigned char stock[5]; const char* what; };
inline constexpr Site kFormatCall{0x00437DFB, {0xE8, 0x4B, 0xDD, 0x00, 0x00}, "Effect_HP sprintf(\"%d\") -> call 0x00445B4B"};

}  // namespace DamageFmt
