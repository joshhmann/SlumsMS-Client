#pragma once
// ============================================================
// display_policy.h -- display and performance toggles (ticket 120). SCAFFOLD.
//
// No Windows, no client: tests/display_policy_test.cpp checks it. Every map layer is an IWzGr2DLayer with a live
// `visible` and `color` (ARGB, alpha in the top byte). The plan: capture layers when they are created, tag each with
// a category and an owner (mine or someone else's), then write visible/alpha from these rules whenever a toggle
// changes and on each new capture, so nothing waits for a map change. This header is the rules; capture hooks come
// next (research note: docs/research-119-120.md).
// ============================================================

#include <cstdint>

namespace Display {

enum class Category { Back, Tile, Object, SkillEffect, Pet, Summon, CashEffect, Other };
enum class Owner { Mine, Others, Map };

struct Prefs {
    bool hideBack = false, hideTiles = false, hideObjects = false;
    int myEffectPct = 100, othersEffectPct = 100;   // skill effect opacity
    int petPct = 100, summonPct = 100;              // others' pets and summons (mine stay solid)
    bool hideOthersCash = false;                    // other players' cash effects
    int maxAnimatedOthers = 0;                      // 0 = no cap
};

inline int ClampPct(int p) { return p < 0 ? 0 : p > 100 ? 100 : p; }

struct LayerState { bool visible; uint8_t alpha; };

inline uint8_t AlphaOf(int pct) { return static_cast<uint8_t>(ClampPct(pct) * 255 / 100); }

inline LayerState Decide(Category c, Owner o, const Prefs& p) {
    switch (c) {
    case Category::Back: return {!p.hideBack, 255};
    case Category::Tile: return {!p.hideTiles, 255};
    case Category::Object: return {!p.hideObjects, 255};
    case Category::SkillEffect: {
        const int pct = o == Owner::Mine ? p.myEffectPct : p.othersEffectPct;
        return {pct > 0, AlphaOf(pct)};
    }
    case Category::Pet: return o == Owner::Mine ? LayerState{true, 255} : LayerState{p.petPct > 0, AlphaOf(p.petPct)};
    case Category::Summon: return o == Owner::Mine ? LayerState{true, 255} : LayerState{p.summonPct > 0, AlphaOf(p.summonPct)};
    case Category::CashEffect: return {!(o == Owner::Others && p.hideOthersCash), 255};
    default: return {true, 255};
    }
}

// Combine with the alpha a layer already has (the stock animation may set its own): a = stock * ours / 255.
inline uint32_t ApplyAlpha(uint32_t argb, uint8_t alpha) {
    const uint32_t a = ((argb >> 24) * alpha) / 255;
    return (argb & 0x00FFFFFFu) | (a << 24);
}

// Others' animated items: the first `cap` characters (by distance, nearest first) animate, the rest hold frame 0.
inline bool MayAnimate(int rankByDistance, const Prefs& p) { return p.maxAnimatedOthers <= 0 || rankByDistance < p.maxAnimatedOthers; }

}  // namespace Display
