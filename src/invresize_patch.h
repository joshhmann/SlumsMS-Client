#pragma once
// ============================================================
// invresize_patch.h -- addresses, stock bytes and grid maths for the resizable item inventory (1.6.0).
//
// Header-only so tests/invresize_patch_test.cpp can check the geometry and the stock bytes without the
// client; invresize.cpp installs it. Ported from the community "invresize" mod (see docs/PROVENANCE.md);
// the analysis below is the mod author's, re-verified against our exe.
//
// THE SLOT MODEL. The whole (slot <-> pixel) model of CUIItem lives in two members:
//   CUIItem::GetSlotRect              0x0081E2C8  ret 8  (nPos, RECT*) -> BOOL
//   CUIItem::GetSlotPositionFromPoint 0x0081DB7E  ret 8  (x, y) -> 1-based slot, 0 = miss
// A rel32 scan finds 5 callers of the first (0x0081DBF7, 0x0081DE52, 0x0081DF89, 0x0081E061, 0x0081EC32) and
// 4 of the second (0x004F1260, 0x004F30A6, 0x0081D48F, 0x0081D8C8), none doing slot arithmetic of its own.
// Replacing the two moves icons, tooltips, drag pick-up, drop targets, double-click and right-click together.
// itemicon.cpp's jmp at 0x0081DEE9 (inside Draw) draws at the x/y Draw got from GetSlotRect, so it follows.
//
// STOCK GEOMETRY (GetSlotRect 0x0081E37B-0x0081E397): icon 32x32, column pitch 36, row pitch 34, origin (8,50).
//   W(cols) = 36*cols + 31, H(rows) = 34*rows + 85  ->  175 x 289 at 4x6, the constructor's own immediates.
//
// NARROW MODE IS FORCED. The 603px 4-tab view (m_bFull, +0x604) has its own rect maths, hit bound and layout
// copy. The constructor's only CConfig read of it (call 0x0049F5DA at 0x0081C4BE, that getter's only caller)
// becomes xor eax,eax, and ToggleFull (0x0081E541, plain ret, its setter 0x0049F62C has one caller inside it)
// becomes a 4 <-> 8 column preset. The only writers of +0x604 are 0x0081C4C3 and 0x0081E555.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>

namespace InvResize {

// ---- client functions and globals -------------------------------------------------------------
constexpr std::uint32_t kAddr_GetSlotRect      = 0x0081E2C8;
constexpr std::uint32_t kAddr_GetSlotPosPoint  = 0x0081DB7E;
constexpr std::uint32_t kAddr_OnCreate         = 0x0081C6C9;   // CUIItem vtable entry at 0x00B39114
constexpr std::uint32_t kAddr_ToggleFull       = 0x0081E541;   // called only from 0x0081D409 (button 0x7D5)
constexpr std::uint32_t kAddr_NarrowGetterCall = 0x0081C4BE;   // call 0x0049F5DA -> m_bFull
constexpr std::uint32_t kAddr_Instance         = 0x00BED654;   // CUIItem* (set 0x0081C455, cleared 0x0081C698)
constexpr std::uint32_t kAddr_ScrollPosArray   = 0x00BF0EB4;   // int[5], per-tab scroll ROW
constexpr std::uint32_t kAddr_CWvsContext      = 0x00BE7918;
constexpr std::uint32_t kAddr_GetCharacterData = 0x00425D0B;
constexpr std::uint32_t kAddr_ZRefRelease      = 0x00428C44;
constexpr std::uint32_t kAddr_SetWndCanvas     = 0x0041E527;   // ZRef<IWzCanvas>::operator=, QI for IID 0x00BD82F8
constexpr std::uint32_t kAddr_CWnd_Destroy     = 0x009E00AF;
constexpr std::uint32_t kAddr_CWnd_CreateWnd   = 0x009DE4D2;
constexpr std::uint32_t kAddr_CWnd_GetAbsLeft  = 0x009E03C5;
constexpr std::uint32_t kAddr_CWnd_GetAbsTop   = 0x009E0447;
constexpr std::uint32_t kAddr_CWnd_Invalidate  = 0x009E04C9;
constexpr std::uint32_t kAddr_ApplySavedTab    = 0x0081D38D;   // push [ecx+0x5A4] into the CCtrlTab at [ecx+0x5B4]
constexpr std::uint32_t kAddr_InputSystem      = 0x00BEC33C;
constexpr std::uint32_t kAddr_GetCursorPos     = 0x0059A388;
constexpr std::uint32_t kAddr_SetCursorState   = 0x0059A6D9;

// Locked slots. CUIItem::Draw (0x0081DC20, ret 4, vtable entry 0x00B39134) draws slots [first, min(first + cells,
// count)), where count is the tab's ZArray size and slot 0 is unused, so a 24-slot tab has count 25. Cells past it
// are drawn after the stock Draw with the client's own locked-slot art, Item/disabled (32x32, sits on the icon).
constexpr std::uint32_t kAddr_Draw = 0x0081DC20;

// New-item tab flash. 0x0081E5DA shows the effect for CWvsContext's new item (+0x38C4 id, +0x38C8 slot): the tab is
// id / 1000000 and its Item/New/Tab0|Tab1 plate (34px) goes to x = tab*34 - 31 (label tab*34 - 25), the stock
// 34px tab positions. With the strip stretched to W - 5 that lands on the wrong tab, so `mov ecx,eax / imul
// ecx,ecx,0x22` at 0x0081E658 becomes a call that returns the same tab*34 at stock width and otherwise a base that
// centres the plate on the real tab. esi is the CUIItem there; eax (the tab) and edx are preserved.
constexpr std::uint32_t kAddr_NewTabX       = 0x0081E658;
constexpr std::uint32_t kAddr_TabLayout     = 0x004DE15F;   // CCtrlTab: recompute each tab's left/right (plain ret)
constexpr std::uint32_t kAddr_TabGetAt      = 0x004DE8CB;   // ZList<TAB>::GetAt(index), ret 4
constexpr int kOff_TabCtrl   = 0x5B4;   // CUIItem -> CCtrlTab*
constexpr int kOff_TabList   = 0x40;    // CCtrlTab ZList of tabs; its count at +0x48
constexpr int kOff_TabCount  = 0x48;
constexpr int kOff_TabLeft   = 0x4;     // TAB left / right, relative to the control
constexpr int kOff_TabRight  = 0x8;
constexpr int kTabCtrlLeft   = 3;       // CreateCtrl(parent, 0x7D0, type 1, l 3, t 0x17, w, 0) at 0x0081C964
constexpr int kStockTabPitch = 34;
constexpr int kFlashW        = 34;      // Item/New/Tab0|Tab1 width
constexpr int kFlashPlateOff = 31;      // plate x = base - 31, label x = base - 25

// The base for tab `tab` whose stretched edges are [left, right) inside the control: plate centred on the tab.
constexpr int FlashBase(int left, int right) { return kTabCtrlLeft + (left + right) / 2 - kFlashW / 2 + kFlashPlateOff; }

// ---- CUIItem field offsets ----------------------------------------------------------------------
constexpr int kOff_CreateId  = 0x14;    // CreateWnd's serial; Destroy zeroes it: the "window is built" test
constexpr int kOff_Width     = 0x24;
constexpr int kOff_Height    = 0x28;
constexpr int kOff_BgCanvas  = 0x68;    // ZRef<IWzCanvas> that CWnd::Draw blits
constexpr int kOff_CloseX    = 0x590;   // close-button x, W - 20
constexpr int kOff_FirstSlot = 0x5E0;   // 1-based index of the top-left visible slot
constexpr int kOff_TabIndex  = 0x5E4;   // 0..4
constexpr int kOff_TabArray  = 0x447;   // GW_CharacterData + 0x447 + 4*tab -> ZArray

// ---- grid ---------------------------------------------------------------------------------------
constexpr int kIcon     = 32;
constexpr int kColPitch = 36;
constexpr int kRowPitch = 34;
constexpr int kGridLeft = 8;     // icon origin; the art's cell border is at 7
constexpr int kGridTop  = 50;

constexpr int kMinCols = 4, kMaxCols = 16;
constexpr int kMinRows = 3, kMaxRows = 14;
constexpr int kStockCols = 4, kStockRows = 6;

// Two patched operands are signed imm8: the Draw loop's limit (cells) and the scroll bias (-(cells-2)).
constexpr int kMaxCells = 120;
static_assert(kMaxCells <= 127, "cells must fit the Draw loop's imm8");
static_assert(kMaxCells - 2 <= 128, "cells-2 must fit the scroll bias imm8");
static_assert(kMaxCols <= 127, "cols must fit the scroll divisor and stride imm8");

constexpr int kGripThickness = 7;

constexpr int WndW(int cols) { return kColPitch * cols + 31; }
constexpr int WndH(int rows) { return kRowPitch * rows + 85; }
static_assert(WndW(kStockCols) == 175 && WndH(kStockRows) == 289, "stock 4x6 window is 175x289");

// Half-pitch snapping to the cell borders: round((W - 31) / 36), round((H - 85) / 34).
constexpr int SnapCols(int wantW) { return (wantW - 13) / kColPitch; }
constexpr int SnapRows(int wantH) { return (wantH - 68) / kRowPitch; }

constexpr int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// First visible cell index (0-based) that is past the tab's capacity; cells from here on are locked.
// count <= 0 means the tab could not be read: lock nothing.
constexpr int FirstLockedCell(int nFirst, int nCount, int cells) {
    if (nCount <= 0) return cells;
    const int v = nCount - nFirst;
    return v < 0 ? 0 : (v > cells ? cells : v);
}

constexpr bool ValidSize(int cols, int rows) {
    return cols >= kMinCols && cols <= kMaxCols && rows >= kMinRows && rows <= kMaxRows && cols * rows <= kMaxCells;
}

// Saved size: one DWORD per character, (cols << 8) | rows. Anything invalid reads as the stock 4x6.
constexpr std::uint32_t PackSize(int cols, int rows) {
    return (static_cast<std::uint32_t>(cols) << 8) | static_cast<std::uint32_t>(rows);
}
constexpr bool UnpackSize(std::uint32_t v, int& cols, int& rows) {
    const int c = static_cast<int>((v >> 8) & 0xFF), r = static_cast<int>(v & 0xFF);
    if ((v >> 16) != 0 || !ValidSize(c, r)) {
        cols = kStockCols;
        rows = kStockRows;
        return false;
    }
    cols = c;
    rows = r;
    return true;
}

// 0 = none, 1 = right edge, 2 = bottom edge, 3 = corner; window-relative coordinates. Grips start at
// y >= kGridTop, below the 25px title drag area, so a grip press never starts a window move.
constexpr int GripAt(int W, int H, int rx, int ry) {
    const bool bRight  = rx >= W - kGripThickness && rx < W && ry >= kGridTop && ry < H;
    const bool bBottom = ry >= H - kGripThickness && ry < H && rx >= 7 && rx < W;
    return (bRight && bBottom) ? 3 : bRight ? 1 : bBottom ? 2 : 0;
}

// The slot rect of nPos in [nFirst, nLimit), or false (stock: SetRectEmpty) outside it.
struct SlotRect { int left, top, right, bottom; };
constexpr bool SlotRectFor(int nPos, int nFirst, int nLimit, int cols, SlotRect& out) {
    if (nPos < nFirst || nPos >= nLimit || cols <= 0) {
        out = {0, 0, 0, 0};
        return false;
    }
    const int v = nPos - nFirst;
    const int x = kGridLeft + kColPitch * (v % cols);
    const int y = kGridTop + kRowPitch * (v / cols);
    out = {x, y, x + kIcon, y + kIcon};
    return true;
}

// ---- layout immediates, rewritten before every CreateWnd ---------------------------------------
// Addresses are of the INSTRUCTION; the guard checks the whole instruction, the write hits the operand.
struct Imm {
    std::uint32_t uInstr;
    std::uint32_t uOperandOffset;
    int nBytes;             // 4 = imm32, 1 = imm8
    const char* sName;
};

enum ImmId { CloseX, WndHeight, WndWidth, TabW, ScrollLen, ScrollX, CoinY, MesoX, MesoY, DrawLimit, RangeBias,
    RangeDiv, kImmCount };

constexpr std::array<Imm, kImmCount> kImms = {{
    {0x0081C4D4, 1, 4, "close button x"},      // add eax, 0x9B    -> W - 20
    {0x0081C512, 1, 4, "window height"},       // push 0x121       -> H
    {0x0081C517, 1, 4, "window width"},        // add eax, 0xAF    -> W
    {0x0081C967, 1, 4, "tab strip width"},     // push 0xAA        -> W - 5
    {0x0081CCB9, 1, 4, "scrollbar length"},    // push 0xC8        -> 34r - 4
    {0x0081CCC0, 1, 4, "scrollbar x"},         // push 0x98        -> 8 + 36c
    {0x0081CD67, 1, 4, "coin button y"},       // push 0x10A       -> H - 23
    {0x0081DCD8, 1, 4, "meso right-align x"},  // mov esi, 0x8A    -> W - 37
    {0x0081DD5F, 1, 4, "meso y"},              // push 0x10A       -> H - 23
    {0x0081DDF9, 2, 1, "draw loop limit"},     // add eax, 0x18    -> cells
    {0x0081D293, 2, 1, "scroll range bias"},   // add eax, -0x16   -> -(cells-2)
    {0x0081D296, 1, 1, "scroll range div"},    // push 4           -> cols
}};

// The value each immediate takes at a grid size. At 4x6 every one equals the stock operand.
constexpr int ImmValue(int id, int cols, int rows) {
    const int W = WndW(cols), H = WndH(rows), cells = cols * rows;
    switch (id) {
    case CloseX: return W - 20;
    case WndHeight: return H;
    case WndWidth: return W;
    case TabW: return W - 5;
    case ScrollLen: return kRowPitch * rows - 4;
    case ScrollX: return kGridLeft + kColPitch * cols;
    case CoinY: return H - 23;
    case MesoX: return W - 37;
    case MesoY: return H - 23;
    case DrawLimit: return cells;
    case RangeBias: return -(cells - 2);
    case RangeDiv: return cols;
    default: return 0;
    }
}

// The column count is also an SIB scale (x4) in two scroll-stride leas, which can only be 1/2/4/8, so the
// 7-byte lea becomes imul r32,r/m32,imm8 + inc eax + 3 nops. Neither next instruction reads flags
// (0x0081D08A push ebx, 0x0081D2CB mov [esi+0x5E0],eax), which matters because imul/inc set them.
struct Stride {
    std::uint32_t uAddr;
    unsigned char modrm;    // imul eax, <src>, imm8
    const char* sName;
};
constexpr std::array<Stride, 2> kStrides = {{
    {0x0081D083, 0xC7, "scroll stride (SetTab)"},     // lea eax,[edi*4+1] -> imul eax,edi,cols / inc eax
    {0x0081D2C4, 0xC0, "scroll stride (OnScroll)"},   // lea eax,[eax*4+1] -> imul eax,eax,cols / inc eax
}};
constexpr std::size_t kStrideLength = 7;

inline std::array<unsigned char, kStrideLength> StrideBytes(unsigned char modrm, int cols) {
    return {{0x6B, modrm, static_cast<unsigned char>(cols), 0x40, 0x90, 0x90, 0x90}};
}

constexpr std::array<unsigned char, 5> kXorEaxNops = {{0x33, 0xC0, 0x90, 0x90, 0x90}};

// ---- stock bytes: every site is checked before anything is written -------------------------------
struct Guard {
    std::uint32_t uAddr;
    const unsigned char* pBytes;
    std::size_t uSize;
    const char* sName;
};

namespace detail {
inline constexpr unsigned char kGetSlotRect[] = {0x55, 0x8B, 0xEC, 0x51, 0x51, 0x56, 0x57};
inline constexpr unsigned char kHitTest[]     = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x53, 0x56, 0x57};
inline constexpr unsigned char kOnCreate[]    = {0xB8, 0xEA, 0x1F, 0xAC, 0x00, 0xE8, 0xC5, 0x44, 0x24, 0x00};
inline constexpr unsigned char kToggleFull[]  = {0x55, 0x8B, 0xEC, 0x51, 0x51, 0x56, 0x33, 0xC0, 0x8B, 0xF1};
inline constexpr unsigned char kNarrowCall[]  = {0xE8, 0x17, 0x31, 0xC8, 0xFF};      // call 0x0049F5DA
inline constexpr unsigned char kCloseX[]      = {0x05, 0x9B, 0x00, 0x00, 0x00};
inline constexpr unsigned char kWndH[]        = {0x68, 0x21, 0x01, 0x00, 0x00};
inline constexpr unsigned char kWndW[]        = {0x05, 0xAF, 0x00, 0x00, 0x00};
inline constexpr unsigned char kTabW[]        = {0x68, 0xAA, 0x00, 0x00, 0x00};
inline constexpr unsigned char kScrollLen[]   = {0x68, 0xC8, 0x00, 0x00, 0x00};
inline constexpr unsigned char kScrollX[]     = {0x68, 0x98, 0x00, 0x00, 0x00};
inline constexpr unsigned char kCoinY[]       = {0x68, 0x0A, 0x01, 0x00, 0x00};
inline constexpr unsigned char kMesoX[]       = {0xBE, 0x8A, 0x00, 0x00, 0x00};
inline constexpr unsigned char kMesoY[]       = {0x68, 0x0A, 0x01, 0x00, 0x00};
inline constexpr unsigned char kDrawLimit[]   = {0x83, 0xC0, 0x18, 0x3B, 0xC1};      // add eax,0x18 / cmp eax,ecx
inline constexpr unsigned char kRangeBias[]   = {0x83, 0xC0, 0xEA, 0x6A, 0x04};      // add eax,-0x16 / push 4
inline constexpr unsigned char kStrideA[]     = {0x8D, 0x04, 0xBD, 0x01, 0x00, 0x00, 0x00};
inline constexpr unsigned char kStrideB[]     = {0x8D, 0x04, 0x85, 0x01, 0x00, 0x00, 0x00};
inline constexpr unsigned char kDraw[]        = {0xB8, 0x7C, 0x21, 0xAC, 0x00, 0xE8, 0x6E, 0x2F, 0x24, 0x00};
inline constexpr unsigned char kNewTabX[]     = {0x8B, 0xC8, 0x6B, 0xC9, 0x22, 0x89, 0x4D, 0x90};  // mov ecx,eax / imul ecx,ecx,0x22 / mov [ebp-0x70],ecx
inline constexpr unsigned char kTabCreate[]   = {0x6A, 0x17, 0x6A, 0x03, 0x6A, 0x01, 0x68, 0xD0, 0x07, 0x00, 0x00};  // push 0x17 / 3 / 1 / 0x7D0
inline constexpr unsigned char kTabLayout[]   = {0xB8, 0x42, 0x67, 0xA8, 0x00, 0xE8, 0x2F, 0x2A, 0x58, 0x00};
inline constexpr unsigned char kTabGetAt[]    = {0x8B, 0x51, 0x08, 0x8B, 0xC2, 0x56, 0x8B, 0x74, 0x24, 0x08};
}  // namespace detail

// kRangeBias also covers the scroll divisor (push 4 at 0x0081D296), so every Imm site is guarded.
constexpr std::array<Guard, 23> kGuards = {{
    {kAddr_GetSlotRect, detail::kGetSlotRect, sizeof(detail::kGetSlotRect), "GetSlotRect"},
    {kAddr_GetSlotPosPoint, detail::kHitTest, sizeof(detail::kHitTest), "GetSlotPositionFromPoint"},
    {kAddr_OnCreate, detail::kOnCreate, sizeof(detail::kOnCreate), "OnCreate"},
    {kAddr_ToggleFull, detail::kToggleFull, sizeof(detail::kToggleFull), "ToggleFull"},
    {kAddr_NarrowGetterCall, detail::kNarrowCall, sizeof(detail::kNarrowCall), "narrow-mode config read"},
    {0x0081C4D4, detail::kCloseX, sizeof(detail::kCloseX), "close button x"},
    {0x0081C512, detail::kWndH, sizeof(detail::kWndH), "window height"},
    {0x0081C517, detail::kWndW, sizeof(detail::kWndW), "window width"},
    {0x0081C967, detail::kTabW, sizeof(detail::kTabW), "tab strip width"},
    {0x0081CCB9, detail::kScrollLen, sizeof(detail::kScrollLen), "scrollbar length"},
    {0x0081CCC0, detail::kScrollX, sizeof(detail::kScrollX), "scrollbar x"},
    {0x0081CD67, detail::kCoinY, sizeof(detail::kCoinY), "coin button y"},
    {0x0081DCD8, detail::kMesoX, sizeof(detail::kMesoX), "meso right-align x"},
    {0x0081DD5F, detail::kMesoY, sizeof(detail::kMesoY), "meso y"},
    {0x0081DDF9, detail::kDrawLimit, sizeof(detail::kDrawLimit), "draw loop limit"},
    {0x0081D293, detail::kRangeBias, sizeof(detail::kRangeBias), "scroll range bias"},
    {0x0081D083, detail::kStrideA, sizeof(detail::kStrideA), "scroll stride (SetTab)"},
    {0x0081D2C4, detail::kStrideB, sizeof(detail::kStrideB), "scroll stride (OnScroll)"},
    {kAddr_Draw, detail::kDraw, sizeof(detail::kDraw), "Draw"},
    {kAddr_NewTabX, detail::kNewTabX, sizeof(detail::kNewTabX), "new-item tab x"},
    {0x0081C96C, detail::kTabCreate, sizeof(detail::kTabCreate), "tab strip position"},
    {kAddr_TabLayout, detail::kTabLayout, sizeof(detail::kTabLayout), "tab layout"},
    {kAddr_TabGetAt, detail::kTabGetAt, sizeof(detail::kTabGetAt), "tab lookup"},
}};

}  // namespace InvResize
