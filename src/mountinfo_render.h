#pragma once
#include "ztl/ztl.h"
#include "mountinfo_layout.h"
#include <vector>
#include <cmath>
#include <algorithm>
struct MountPreviewPart { IWzCanvasPtr canvas; int w, h, cx, cy, order; };
inline void AlignMountPart(MountPreviewPart& part) {
    try {
        IWzPropertyPtr property = part.canvas->property;
        IWzVector2DPtr anchor = property->item[L"map/navel"].GetUnknown();
        if (anchor) { part.cx += anchor->x; part.cy += anchor->y; }
    } catch (const _com_error&) {}
}
inline IWzCanvasPtr ComposeMountPreview(IWzCanvas* background, const std::vector<MountPreviewPart>& parts, MountLayout layout) {
    IWzCanvasPtr result;
    PcCreateObject<IWzCanvasPtr>(L"Canvas", result, nullptr);
    result->Create(87, 77, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);
    result->CopyEx(0, 0, background, CANVAS_ALPHATYPE::CA_OVERWRITE, 87, 77, 9, 182, 87, 77);
    for (const auto& part : parts) {
        result->CopyEx(lround(layout.originX - part.cx * layout.scale) - 9,
            lround(layout.originY - part.cy * layout.scale) - 182, part.canvas,
            CANVAS_ALPHATYPE::CA_OVERWRITE, (std::max)(1L, lround(part.w * layout.scale)),
            (std::max)(1L, lround(part.h * layout.scale)), 0, 0, part.w, part.h);
    }
    return result;
}
