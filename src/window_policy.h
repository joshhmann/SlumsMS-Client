#pragma once
// ============================================================
// window_policy.h -- borderless window placement (ticket 137, owner pick 2026-10-28). No Windows: tested natively.
//
// [graphics] borderless=true: the game window has no frame (WS_POPUP) and sits centred on its monitor; at the
// monitor's own resolution it covers the screen ("fake fullscreen": fast alt-tab, no mode switch). Nothing is
// scaled: the game renders at the resolution the player picked. [graphics] dpiaware=true: the process tells Windows
// it handles DPI, so a scaled display (125-200%) shows the game pixel for pixel instead of a blurred upscale.
// ============================================================

#include <cctype>
#include <cstring>

namespace WindowPolicy {

struct Rect { int left, top, right, bottom; };

// Top-left for a client area of w x h on monitor m: centred; a window as large as (or larger than) the monitor
// starts at its corner so the game's top-left is always visible.
inline void Place(const Rect& m, int w, int h, int& x, int& y) {
    const int mw = m.right - m.left, mh = m.bottom - m.top;
    x = w >= mw ? m.left : m.left + (mw - w) / 2;
    y = h >= mh ? m.top : m.top + (mh - h) / 2;
}

// config.ini booleans as the client's other switches read them: 1/true/yes/on, anything else (or empty) is false.
inline bool IniTrue(const char* v) {
    char s[8] = {};
    for (size_t i = 0; v && v[i] && i < sizeof(s) - 1; ++i) s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(v[i])));
    return !std::strcmp(s, "1") || !std::strcmp(s, "true") || !std::strcmp(s, "yes") || !std::strcmp(s, "on");
}

}  // namespace WindowPolicy
