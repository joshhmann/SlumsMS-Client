#pragma once
// ============================================================
// overlay_policy.h -- layout maths for the ImGui HUD (1.6.0): no Windows, no ImGui, so tests can check it.
//
// Widget positions and sizes are stored as fractions of the screen (x 10000), so a widget keeps its place
// across the client's resolutions (800x600 up to 1920x1080) and moves with them instead of falling off-screen.
// ============================================================

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace OverlayPolicy {

constexpr int kFrac = 10000;   // stored fraction scale

struct Rect { float x, y, w, h; };

// Stored fractions -> pixels on a screen of sw x sh, kept fully on screen (at least minW x minH).
inline Rect FromFractions(int fx, int fy, int fw, int fh, float sw, float sh, float minW, float minH) {
    Rect r{fx * sw / kFrac, fy * sh / kFrac, fw * sw / kFrac, fh * sh / kFrac};
    r.w = (std::min)((std::max)(r.w, minW), sw);
    r.h = (std::min)((std::max)(r.h, minH), sh);
    r.x = (std::min)((std::max)(r.x, 0.0f), sw - r.w);
    r.y = (std::min)((std::max)(r.y, 0.0f), sh - r.h);
    return r;
}

inline int ToFraction(float v, float extent) {
    if (extent <= 0) return 0;
    const int f = static_cast<int>(v * kFrac / extent + 0.5f);
    return (std::min)((std::max)(f, 0), kFrac);
}

// Font pixel size for a back buffer height and the player's scale (percent): 15px at 600 lines, growing with height.
inline float FontPixels(float screenH, int scalePct) {
    const float base = 15.0f * (std::max)(screenH, 480.0f) / 600.0f;
    const float s = (std::min)((std::max)(scalePct, 50), 200) / 100.0f;
    return (std::min)((std::max)(base * s, 11.0f), 40.0f);
}

// Edit UI's snap grid: a stored fraction rounded to the nearest grid line (grid in fractions, e.g. 125 = 1.25%).
constexpr int kGridFrac = 125;
inline int Snap(int frac, int grid = kGridFrac) {
    if (grid <= 0) return frac;
    const int half = grid / 2;
    const int snapped = frac >= 0 ? (frac + half) / grid * grid : -((-frac + half) / grid * grid);
    return (std::min)((std::max)(snapped, -kFrac), kFrac);
}

// 0..1 for a bar; a zero or negative max shows an empty bar rather than dividing by zero.
inline float Ratio(long long value, long long max) {
    if (max <= 0) return 0.0f;
    return static_cast<float>((std::min)((std::max)(value, 0LL), max)) / static_cast<float>(max);
}

// "1234 / 5678 (21.73%)"; EXP shows two decimals like the client, HP/MP none.
inline void BarLabel(char* out, std::size_t n, long long value, long long max, bool twoDecimals) {
    const double pct = max > 0 ? 100.0 * static_cast<double>((std::min)((std::max)(value, 0LL), max)) / max : 0.0;
    if (twoDecimals) std::snprintf(out, n, "%lld / %lld (%.2f%%)", value, max, pct);
    else std::snprintf(out, n, "%lld / %lld (%d%%)", value, max, static_cast<int>(pct));
}

}  // namespace OverlayPolicy
