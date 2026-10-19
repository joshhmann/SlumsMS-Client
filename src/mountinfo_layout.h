#pragma once
#include <algorithm>
struct MountBounds { double left, top, right, bottom; };
struct MountLayout { double scale, originX, originY; };
inline MountLayout FitMountPreview(MountBounds b) {
    double w = b.right - b.left, h = b.bottom - b.top;
    if (w <= 0 || h <= 0) return {0, 0, 0};
    double scale = (std::min)({87.0 / w, 77.0 / h, 1.0});
    return {scale, 52.5 - w * scale / 2 - b.left * scale, 259.0 - h * scale - b.top * scale};
}
