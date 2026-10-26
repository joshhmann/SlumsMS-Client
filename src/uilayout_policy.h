#pragma once
// ============================================================
// uilayout_policy.h -- where the stock bottom bar and quick slot sit when the player moves them (1.6.0).
//
// No Windows, no client: tests/uilayout_policy_test.cpp checks it. uilayout.cpp applies it.
//
// An offset is how far the player moved an element from its stock place, stored as a fraction of the screen (x
// 10000) so a layout survives a resolution change, like the HUD widgets (overlay_policy.h). Applied in pixels, it is
// clamped so the element stays fully on screen. Per character in HKCU\Software\SlumsMS\Layout, one REG_DWORD per
// element: the two signed 16-bit fractions packed as (x << 16) | y.
// ============================================================

#include <algorithm>
#include <cstdint>

namespace UiLayout {

enum Elem { kStatusBar = 0, kQuickSlot, kMainButtons, kIconRow, kChatInput, kNativeHp, kNativeMp, kNativeExp, kElemCount };
constexpr int kFirstGroup = kMainButtons;   // kFirstGroup..kLastGroup: groups of the status bar's own child controls
constexpr int kLastGroup = kChatInput;
constexpr int kFirstNative = kNativeHp;     // from here on: our own stock-art modules (hud_native.cpp)

constexpr int kFrac = 10000;

struct Offset { int fx = 0, fy = 0; };

// An element's place at offset 0, in screen pixels.
struct Rect { int left = 0, top = 0, w = 0, h = 0; };

inline std::uint32_t Pack(Offset o) {
    return (static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(o.fx))) << 16) |
        static_cast<std::uint16_t>(static_cast<std::int16_t>(o.fy));
}

inline Offset Unpack(std::uint32_t v) {
    Offset o;
    o.fx = static_cast<std::int16_t>(static_cast<std::uint16_t>(v >> 16));
    o.fy = static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xFFFF));
    o.fx = (std::max)(-kFrac, (std::min)(o.fx, kFrac));
    o.fy = (std::max)(-kFrac, (std::min)(o.fy, kFrac));
    return o;
}

inline int ToPx(int frac, int extent) {
    const long long v = static_cast<long long>(frac) * extent;
    return static_cast<int>(v >= 0 ? (v + kFrac / 2) / kFrac : -((-v + kFrac / 2) / kFrac));
}

inline int ToFrac(int px, int extent) {
    if (extent <= 0) return 0;
    const long long v = static_cast<long long>(px) * kFrac;
    const int f = static_cast<int>(v >= 0 ? (v + extent / 2) / extent : -((-v + extent / 2) / extent));
    return (std::max)(-kFrac, (std::min)(f, kFrac));
}

// Pixel offset that keeps `home` (the element at offset 0) fully inside `bounds`. Without a known size the offset
// is only kept within one bounds' size.
inline void ClampWithin(const Rect& home, const Rect& bounds, int& dx, int& dy) {
    if (home.w > 0 && home.h > 0) {
        const int minDx = bounds.left - home.left, maxDx = bounds.left + bounds.w - home.w - home.left;
        const int minDy = bounds.top - home.top, maxDy = bounds.top + bounds.h - home.h - home.top;
        dx = minDx <= maxDx ? (std::max)(minDx, (std::min)(dx, maxDx)) : minDx;
        dy = minDy <= maxDy ? (std::max)(minDy, (std::min)(dy, maxDy)) : minDy;
    } else {
        dx = (std::max)(-bounds.w, (std::min)(dx, bounds.w));
        dy = (std::max)(-bounds.h, (std::min)(dy, bounds.h));
    }
}

// The same, on a sw x sh screen.
inline void Clamp(const Rect& home, int sw, int sh, int& dx, int& dy) {
    ClampWithin(home, Rect{0, 0, sw, sh}, dx, dy);
}

// Where a control group may go: on screen and inside its parent window (a control outside its window draws but
// gets no clicks).
inline Rect Intersect(const Rect& a, const Rect& b) {
    const int l = (std::max)(a.left, b.left), t = (std::max)(a.top, b.top);
    const int r = (std::min)(a.left + a.w, b.left + b.w), btm = (std::min)(a.top + a.h, b.top + b.h);
    return Rect{l, t, (std::max)(r - l, 0), (std::max)(btm - t, 0)};
}

// The pixel offset for a stored fraction on this screen, clamped to `bounds`.
inline void PixelOffsetWithin(Offset o, const Rect& home, const Rect& bounds, int sw, int sh, int& dx, int& dy) {
    dx = ToPx(o.fx, sw);
    dy = ToPx(o.fy, sh);
    ClampWithin(home, bounds, dx, dy);
}

inline void PixelOffset(Offset o, const Rect& home, int sw, int sh, int& dx, int& dy) {
    PixelOffsetWithin(o, home, Rect{0, 0, sw, sh}, sw, sh, dx, dy);
}

}  // namespace UiLayout
