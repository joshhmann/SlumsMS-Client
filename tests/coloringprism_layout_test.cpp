// Coloring Prism (MAPLEROOT_COLOR_PRISM): every control in coloringprism_layout.h lands where the backdrop
// (assets/colorprism/backgrnd.png, measured; see the header) has room for it. 1.4.0 drew the sliders a row
// below the art's bars and had no RESET; this keeps the two from drifting apart again.
#include "../src/coloringprism_layout.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ColorPrismLayout;

namespace {

struct Box { int l, t, r, b; };   // half-open

void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

bool Inside(Box in, Box out) { return in.l >= out.l && in.t >= out.t && in.r <= out.r && in.b <= out.b; }
bool Overlap(Box a, Box b) { return a.l < b.r && b.l < a.r && a.t < b.b && b.t < a.b; }

// Measured off backgrnd.png (inclusive pixel ranges turned half-open).
constexpr Box kTitleBar{ 7, 6, 294, 22 };
constexpr Box kTabStrip{ 4, 23, 297, 48 };
constexpr Box kWellInterior{ 22, 60, 59, 95 };
constexpr Box kBannerRightOfWell{ 63, 49, 293, 100 };
constexpr Box kBannerInterior{ 9, 49, 293, 100 };
constexpr Box kFloor{ 20, 223, 282, 240 };
constexpr Box kFooter{ 4, 371, 297, 402 };
constexpr int kBarInteriorL = 87, kBarInteriorR = 265, kBarInteriorH = 10;
constexpr int kInnerRight = 297;
constexpr int kTextH = 12;   // Dotum 12

}  // namespace

int main() {
    try {
        Check(Inside({ kBtCloseX, kBtCloseY, kBtCloseX + kBtCloseW, kBtCloseY + kBtCloseH }, kTitleBar), "close chip");

        for (int t = 0; t < kTabCount; ++t) {
            const Box tab{ kTabX[t], kTabT, kTabX[t] + kTabW, kTabT + kTabH };
            Check(Inside(tab, kTabStrip), "tab " + std::to_string(t) + " in the strip");
            if (t) Check(!Overlap(tab, { kTabX[t - 1], kTabT, kTabX[t - 1] + kTabW, kTabT + kTabH }), "tabs overlap");
        }

        Check(Inside({ kIconX, kIconBaseline - kIconSize, kIconX + kIconSize, kIconBaseline }, kWellInterior), "icon in well");
        Check(Inside({ kBannerTextX, kBannerTextT, kBannerTextX + kBannerTextW, kBannerTextT + 2 * kBannerLineH + kTextH },
                     kBannerRightOfWell), "drop copy right of the well");
        Check(Inside({ kBannerFullX, kBannerLookT, kBannerFullX + kBannerFullW, kBannerLookT + kBannerLineH + kTextH },
                     kBannerInterior), "look copy in the banner");

        Check(kAvatarY >= kFloor.t && kAvatarY < kFloor.b, "avatar stands on the floor");
        Check(kAvatarX - kPreviewAvatarHalfW >= kPreviewSepLeft && kAvatarX + kPreviewAvatarHalfW <= kPreviewSepRight,
              "avatar between the pane walls");

        for (int row = 0; row < 3; ++row) {
            const std::string r = "row " + std::to_string(row);
            const int barT = BarBorderTop(row) + 1;                     // bar interior top
            const Box bar{ kBarInteriorL, barT, kBarInteriorR, barT + kBarInteriorH };
            Check(Inside({ kTrackX, TrackY(row), kTrackX + kTrackW, TrackY(row) + kTrackH }, bar), r + ": track in its bar");
            Check(TrackY(row) - barT == barT + kBarInteriorH - (TrackY(row) + kTrackH), r + ": track centred in its bar");
            Check(ThumbY(row) + kThumbH / 2 == TrackY(row) + kTrackH / 2, r + ": thumb centred on the track");
            Check(kTrackX + kTrackTravel + kThumbW <= kBarInteriorR, r + ": thumb travel stays on the bar");
            Check(kValueX >= kBarInteriorR + 1 && kValueX + kValueW <= kInnerRight, r + ": readout right of the bar");
            if (row) Check(RowTop(row) >= RowTop(row - 1) + kRowH, r + ": rows overlap");
        }

        Box prev{ 0, 0, 0, 0 };
        for (int i = 0; i < kBtnCount; ++i) {
            const Box b{ kBtnX[i], kBtnT, kBtnX[i] + kBtnW, kBtnT + kBtnH };
            Check(Inside(b, kFooter), "button " + std::to_string(i) + " in the footer");
            Check(!Overlap(b, prev), "buttons overlap");
            prev = b;
        }
        Check(kBtnX[kBtnReset] < kBtnX[kBtnOK] && kBtnX[kBtnOK] < kBtnX[kBtnCancel], "RESET | OK CANCEL order");
        Check(kBtnX[kBtnOK] - (kBtnX[kBtnReset] + kBtnW) > 2 * kBtnW, "RESET stands apart from OK");
    } catch (const std::exception& e) {
        std::cerr << "coloringprism_layout: " << e.what() << '\n';
        return 1;
    }
    std::cout << "coloringprism_layout: ok\n";
    return 0;
}
