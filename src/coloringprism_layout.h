#pragma once
// Coloring Prism window layout (MAPLEROOT_COLOR_PRISM), header-only so tests/coloringprism_layout_test.cpp can
// check it without the client.
//
// Every number here is MEASURED off the backdrop the client actually loads, assets/colorprism/backgrnd.png
// (UI/UIWindowEx.img/ColorPrism/backgrnd, 301x406; backgrndLook is the same frame without the drop well). The
// art bakes the frame, the title, the banner, the preview pane with its floor, the HUE/CHROMA/VALUE labels and
// the three slider bars; coloringprism.cpp paints only what moves into it.
//
// 1.4.0 shipped the package's layout, which was measured off the PACKAGE's art (Tools/art/coloring-prism.png),
// not off this one: the sliders were drawn a row below the bars, the banner copy was white on the light banner
// and the left footer button, lettered CANCEL, reset the sliders -- so there was no visible Reset. Redraw the
// art -> re-measure -> update this file and the test together.
//
// Measured (window-local pixels, inclusive):
//   title bar   blue x7..293  y6..21
//   tab strip   cream          y23..47
//   banner      border x8 / x293, y48 / y100; drop well border x18..62, y56..98, interior x22..58, y60..94
//   preview     interior x20..281, y107..221; floor line y222, tan floor y223..239
//   bars        border x86..265, y258..269 / 293..304 / 328..339 (interior x87..264, 10 high)
//   footer      separator line y370, interior y371..401; window inner edge x4..296

namespace ColorPrismLayout {

constexpr int kWndW = 301, kWndH = 406;
constexpr int kTitleH = 23;                                  // drag region
constexpr int kBtCloseX = 278, kBtCloseY = 8, kBtCloseW = 12, kBtCloseH = 12;   // UI/Basic.img/BtClose

// TABS: the Item Inventory's own UI/Basic.img/Tab2 pieces (6px caps, 1px fill, all 21px high), drawn on the
// cream strip between the title bar and the banner. 56px fits "Effects" between the caps.
constexpr int kTabT = 25, kTabH = 21, kTabW = 56;
constexpr int kTabCount = 4;
constexpr int kTabX[kTabCount] = { 8, 64, 120, 176 };

// BANNER and its drop well. The item icon is drawn bottom-left anchored (DrawItemIconForSlot), centred in the
// well's 37x35 interior.
constexpr int kWellL = 22, kWellT = 60, kWellR = 59, kWellB = 95;   // interior, half-open
constexpr int kIconSize = 32;
constexpr int kIconX = kWellL + (kWellR - kWellL - kIconSize) / 2;                  // 24
constexpr int kIconBaseline = kWellT + (kWellB - kWellT - kIconSize) / 2 + kIconSize; // 93
// Copy right of the well (Equip / Effects): three 14px lines centred on the banner's interior y49..99.
constexpr int kBannerTextX = 66, kBannerTextW = 224, kBannerTextT = 54, kBannerLineH = 14;
// Hair / Eyes use the wellless backdrop: two lines across the whole banner interior x9..292.
constexpr int kBannerFullX = 9, kBannerFullW = 284, kBannerLookT = 61;

// PREVIEW pane. The avatar stands on the tan floor and is kept inside the pane's side walls.
constexpr int kPreviewT = 107, kFloorY = 222;
constexpr int kPreviewSepLeft = 20, kPreviewSepRight = 282;
constexpr int kAvatarX = (kPreviewSepLeft + kPreviewSepRight) / 2;   // 151
constexpr int kAvatarY = 230;                                         // feet, mid-floor
constexpr int kPreviewAvatarHalfW = 18;

// SLIDERS. Row r's track (the 176x8 trackTone/Chroma/Bright art) fills bar r's interior with a 1px margin;
// the 22x14 UI/Basic.img/Slider thumb is centred on it; the numeric readout sits right of the bar.
constexpr int kRowsT = 250, kRowStep = 35, kRowH = 28;
constexpr int kTrackX = 88, kTrackW = 176, kTrackH = 8;
constexpr int kThumbW = 22, kThumbH = 14;
constexpr int kTrackTravel = kTrackW - kThumbW;
constexpr int kValueX = 267, kValueW = 29;                   // up to x295; the inner edge is x296
constexpr int RowTop(int row) { return kRowsT + row * kRowStep; }
constexpr int TrackY(int row) { return RowTop(row) + (kRowH - kTrackH) / 2; }
constexpr int ThumbY(int row) { return RowTop(row) + (kRowH - kThumbH) / 2; }
constexpr int BarBorderTop(int row) { return 258 + row * 35; }   // measured, for the test

// FOOTER: RESET on the left; OK and CANCEL on the right, as stock dialogs place them. All three are 47x18
// (UI/Basic.img/BtOK2, BtCancel2, and the prism's own UIWindowEx.img/ColorPrism/BtReset made from BtCancel2),
// centred in the footer's interior y371..401.
constexpr int kFooterT = 371, kFooterB = 402;
constexpr int kBtnW = 47, kBtnH = 18;
constexpr int kBtnT = kFooterT + (kFooterB - kFooterT - kBtnH) / 2;   // 377
enum Button { kBtnReset = 0, kBtnOK = 1, kBtnCancel = 2, kBtnCount = 3 };
constexpr int kBtnGap = 4;
constexpr int kBtnX[kBtnCount] = { 12, kWndW - 12 - 2 * kBtnW - kBtnGap, kWndW - 12 - kBtnW };

}  // namespace ColorPrismLayout
