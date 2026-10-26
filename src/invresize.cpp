// ============================================================
// invresize.cpp: drag the item inventory's right edge, bottom edge or corner to 4-16 columns x 3-14 rows (1.6.0).
//
// Ported from the community "invresize" mod (docs/PROVENANCE.md). invresize_patch.h has the addresses, the stock
// bytes and the analysis. On by default; config.ini [clienthax] invresize=false patches nothing, which keeps the
// stock 4x6 window and its 4-tab "full" view. The size is saved per character in HKCU\Software\SlumsMS\Inventory
// (value w<world>_c<id>). Client only: no server half, no WZ edit; the frame is composited at runtime from
// UI/UIWindow.img/Item/FullBackgrnd.
//
// Drivers: InvResize_HandleMouseMessage from CWndMan::TranslateMessage_hook (grip press/drag/release; records a
// pending size only) and InvResize_Tick from CWvsApp::CallUpdate_hook (commits it: CWnd::Destroy + CreateWnd,
// exactly what the client's own expand button does, so child layout stays "whatever OnCreate does").
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "attackstop_patch.h"
#include "invresize_patch.h"
#include "wvs/util.h"
#include "wvs/wnd.h"
#include "ztl/ztl.h"

#include <windows.h>

namespace InvResize {
namespace {

// ---- live state -----------------------------------------------------------------------------------
bool g_bInstalled = false;
int  g_nCols = kStockCols;          // what the patched immediates currently say
int  g_nRows = kStockRows;
bool g_bLoaded = false;
bool g_bCharacterLoaded = false;   // set by CConfig::LoadCharacter; the size is per character
int g_nWorldID = 0;
unsigned g_dwCharacterID = 0;

int  g_pendCols = 0, g_pendRows = 0;   // requested from the WndProc, applied in Tick
bool g_bDragging = false;
int  g_nDragMode = 0;                  // 1 = right edge, 2 = bottom edge, 3 = corner
void* g_pDragWnd = nullptr;            // latched at grip-down; a mismatch aborts
int  g_nDragStartCols = 0, g_nDragStartRows = 0;
bool g_bSaveAfterDrag = false;         // a drag ended: save once its final size is applied
int  g_nCursorState = 0;

constexpr int kCursor_Arrow  = 0;
constexpr int kCursor_Resize = 9;      // static up/down arrows; the client has no diagonal or left/right sprite

template <typename T>
T At(std::uint32_t addr) { return reinterpret_cast<T>(static_cast<uintptr_t>(addr)); }

// ---------------------------------------------------------------------------------------------------
// SEH leaves. /EHsc catch(...) does not catch access violations, and C2712 forbids __try next to objects
// that unwind, so every guarded read is its own leaf with no smart pointers in scope.
// ---------------------------------------------------------------------------------------------------
void* SehInstance() {
    __try { return *At<void**>(kAddr_Instance); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

int SehReadInt(void* p, int off) {
    __try { return *reinterpret_cast<int*>(reinterpret_cast<char*>(p) + off); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

void SehWriteInt(void* p, int off, int v) {
    __try { *reinterpret_cast<int*>(reinterpret_cast<char*>(p) + off) = v; }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Screen-space top-left of a CWnd. Both getters take the IUIMsgHandler sub-object at base+4.
void SehWndAbs(void* pBase, int& l, int& t) {
    l = 0; t = 0;
    __try {
        void* h = reinterpret_cast<char*>(pBase) + 4;
        l = At<int(__thiscall*)(void*)>(kAddr_CWnd_GetAbsLeft)(h);
        t = At<int(__thiscall*)(void*)>(kAddr_CWnd_GetAbsTop)(h);
    } __except (EXCEPTION_EXECUTE_HANDLER) { l = 0; t = 0; }
}

// Slot count of the selected tab, the client's own idiom from GetSlotRect 0x0081E2D4-0x0081E30D:
// GetCharacterData fills an 8-byte ZRef whose payload is at +4; the tab's ZArray is at characterData +
// 0x447 + 4*tab and its count is the int just before the data. CWvsContext holds the master reference, so the
// local one is released straight away.
int SehTabSlotCount(void* pThis) {
    __try {
        void* ctx = *At<void**>(kAddr_CWvsContext);
        if (!ctx) return 0;
        void* out[2] = { nullptr, nullptr };
        void* ret = At<void*(__thiscall*)(void*, void**)>(kAddr_GetCharacterData)(ctx, out);
        void* cd = *reinterpret_cast<void**>(reinterpret_cast<char*>(ret) + 4);
        if (out[1]) {
            At<void(__thiscall*)(void**, int)>(kAddr_ZRefRelease)(out, 0);
            out[1] = nullptr;
        }
        if (!cd) return 0;
        const int nTab = *reinterpret_cast<int*>(reinterpret_cast<char*>(pThis) + kOff_TabIndex);
        if (nTab < 0 || nTab > 4) return 0;
        void* pArr = *reinterpret_cast<void**>(reinterpret_cast<char*>(cd) + kOff_TabArray + 4 * nTab);
        if (!pArr) return 0;
        const int n = *(reinterpret_cast<int*>(pArr) - 1);
        return (n < 0 || n > 100000) ? 0 : n;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool GetAbsCursor(POINT& sp) {
    sp.x = 0; sp.y = 0;
    void* pInput = *At<void**>(kAddr_InputSystem);
    if (!pInput) return false;
    At<void(__thiscall*)(void*, POINT*)>(kAddr_GetCursorPos)(pInput, &sp);
    return true;
}

void SetCursorState(int state) {
    void* pInput = *At<void**>(kAddr_InputSystem);
    if (pInput) At<void(__thiscall*)(void*, int)>(kAddr_SetCursorState)(pInput, state);
}

void Invalidate(void* pThis) {
    __try { At<void(__thiscall*)(void*, const RECT*)>(kAddr_CWnd_Invalidate)(pThis, nullptr); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ===================================================================================================
// The two replacements keep the stock signature, 1-based slot space and `ret 8`, so callers are unaffected.
// ===================================================================================================

// The visible half-open range [first, min(first + cells, tabSlotCount)). The second clamp is stock
// (0x0081E322-0x0081E32E): it stops past-capacity cells being drawn into or clicked.
void VisibleRange(void* pThis, int& nFirst, int& nLimit) {
    nFirst = SehReadInt(pThis, kOff_FirstSlot);
    const int nCount = SehTabSlotCount(pThis);
    nLimit = nFirst + g_nCols * g_nRows;
    if (nLimit > nCount) nLimit = nCount;
}

bool SlotRectCore(int nPos, int nFirst, int nLimit, RECT* pOut) {
    SlotRect r;
    if (!SlotRectFor(nPos, nFirst, nLimit, g_nCols, r)) {
        SetRectEmpty(pOut);
        return false;
    }
    SetRect(pOut, r.left, r.top, r.right, r.bottom);
    return true;
}

int __fastcall GetSlotRect_hook(void* pThis, void* /*edx*/, int nPos, RECT* pOut) {
    if (!pThis || !pOut) return 0;
    int nFirst = 0, nLimit = 0;
    VisibleRange(pThis, nFirst, nLimit);
    return SlotRectCore(nPos, nFirst, nLimit, pOut) ? 1 : 0;
}

// 1-based slot, 0 = miss. Same walk as stock (0x0081DBE6-0x0081DC11).
int __fastcall GetSlotPosFromPoint_hook(void* pThis, void* /*edx*/, int x, int y) {
    if (!pThis) return 0;
    int nFirst = 0, nLimit = 0;
    VisibleRange(pThis, nFirst, nLimit);
    const POINT pt = { static_cast<LONG>(x), static_cast<LONG>(y) };
    for (int i = nFirst; i < nLimit; ++i) {
        RECT rc;
        if (SlotRectCore(i, nFirst, nLimit, &rc) && PtInRect(&rc, pt)) return i;
    }
    return 0;
}

// ===================================================================================================
// Background. Every band comes from Item/FullBackgrnd (603x289): Item/backgrnd is aperiodic dither that cannot
// be tiled, while FullBackgrnd has a true 32px period and byte-identical cells. Do not tile from its cell columns
// 4..15 (later tab groups, offset 5px each by the full-mode maths). The 4x6 window keeps stock geometry but takes
// FullBackgrnd's paper tone.
// ===================================================================================================
constexpr int kSrcW = 603;
constexpr int kTilePeriod = 32;

constexpr int kA_H = 50, kA_Left = 81, kA_Right = 12, kA_TileX = 81;
constexpr int kB_CellX = 7, kB_RailW = 7, kB_GutterSrc = 596, kB_FrameSrc = 599, kB_FrameW = 4;
constexpr int kC_H = 37, kC_SrcY = 252, kC_Left = 31, kC_TileX = 31;
constexpr int kC_MesoSrc = 142, kC_MesoW = 30;
constexpr int kC_Right = kC_MesoW + kB_FrameW;

IWzCanvas* FullBackgrnd() {
    static bool bTried = false;
    static IWzCanvasPtr pFull;
    if (!bTried) {
        bTried = true;
        try {
            pFull = get_unknown(get_rm()->GetObjectA(L"UI/UIWindow.img/Item/FullBackgrnd"));
        } catch (...) {
        }
        if (!pFull) {
            LOG_ONCE("Inventory resize: UI/UIWindow.img/Item/FullBackgrnd is missing; stock background kept");
        }
    }
    return pFull;
}

// Verbatim rect copy, alpha included (CA_OVERWRITE keeps the frame's soft shadow).
void Blit(IWzCanvasPtr pDst, IWzCanvas* pSrc, int sx, int sy, int w, int h, int dx, int dy) {
    if (w <= 0 || h <= 0) return;
    pDst->CopyEx(dx, dy, pSrc, CANVAS_ALPHATYPE::CA_OVERWRITE, w, h, sx, sy, w, h);
}

void TileH(IWzCanvasPtr pDst, IWzCanvas* pSrc, int sx, int sy, int h, int dx0, int dx1, int dy) {
    for (int x = dx0; x < dx1; x += kTilePeriod) {
        const int w = (dx1 - x < kTilePeriod) ? (dx1 - x) : kTilePeriod;
        Blit(pDst, pSrc, sx, sy, w, h, x, dy);
    }
}

IWzCanvasPtr BuildBackground(int cols, int rows) {
    IWzCanvas* pF = FullBackgrnd();
    if (!pF) return nullptr;

    const int W = WndW(cols), H = WndH(rows);
    IWzCanvasPtr pDst;
    try {
        PcCreateObject<IWzCanvasPtr>(L"Canvas", pDst, nullptr);
        if (!pDst) return nullptr;
        pDst->Create(W, H, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);

        // Band A: title bar and tab plate, source rows 0..49.
        Blit(pDst, pF, 0, 0, kA_Left, kA_H, 0, 0);
        TileH(pDst, pF, kA_TileX, 0, kA_H, kA_Left, W - kA_Right, 0);
        Blit(pDst, pF, kSrcW - kA_Right, 0, kA_Right, kA_H, W - kA_Right, 0);

        // Band B: one 36x34 source cell into every cell, then the scrollbar gutter and the 4-column right frame.
        for (int r = 0; r < rows; ++r) {
            const int y = kGridTop + kRowPitch * r;
            Blit(pDst, pF, 0, kGridTop, kB_RailW, kRowPitch, 0, y);
            for (int c = 0; c < cols; ++c) {
                Blit(pDst, pF, kB_CellX, kGridTop, kColPitch, kRowPitch, kB_CellX + kColPitch * c, y);
            }
            for (int x = kB_CellX + kColPitch * cols; x < W - kB_FrameW; ++x) {
                Blit(pDst, pF, kB_GutterSrc, kGridTop, 1, kRowPitch, x, y);
            }
            Blit(pDst, pF, kB_FrameSrc, kGridTop, kB_FrameW, kRowPitch, W - kB_FrameW, y);
        }

        // Band C: the meso row, drawn last over band B's trailing 2 rows (why H is 34r+85, not 34r+87). The
        // "mesos" label is lifted out as its own cap and butted onto the frame, as stock lays it out at 4x6.
        const int cy = H - kC_H;
        Blit(pDst, pF, 0, kC_SrcY, kC_Left, kC_H, 0, cy);
        TileH(pDst, pF, kC_TileX, kC_SrcY, kC_H, kC_Left, W - kC_Right, cy);
        Blit(pDst, pF, kC_MesoSrc, kC_SrcY, kC_MesoW, kC_H, W - kC_Right, cy);
        Blit(pDst, pF, kB_FrameSrc, kC_SrcY, kB_FrameW, kC_H, W - kB_FrameW, cy);
        return pDst;
    } catch (...) {
        return nullptr;   // InstallBackground keeps the stock canvas
    }
}

// 0x0041E527 is ZRef<IWzCanvas>::operator=: ecx is the holder and the argument is the ADDRESS of a smart
// pointer, which it QIs for the canvas IID and writes back through, then releases the old canvas.
void SehInstallCanvas(void* pThis, IWzCanvas* pCanvas) {
    __try {
        IUnknown* raw = pCanvas;
        At<void(__thiscall*)(void*, IUnknown**)>(kAddr_SetWndCanvas)(reinterpret_cast<char*>(pThis) + kOff_BgCanvas, &raw);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void InstallBackground(void* pThis, int cols, int rows) {
    // Cached on (cols, rows): the window is rebuilt on every snap step of a drag.
    static IWzCanvasPtr s_pCached;
    static int s_nCachedCols = -1, s_nCachedRows = -1;

    if (!s_pCached || s_nCachedCols != cols || s_nCachedRows != rows) {
        IWzCanvasPtr p = BuildBackground(cols, rows);
        if (!p) return;
        s_pCached = p;
        s_nCachedCols = cols;
        s_nCachedRows = rows;
    }
    SehInstallCanvas(pThis, s_pCached);
}

// ===================================================================================================
// Layout immediates, rewritten in place before every CreateWnd so OnCreate lays the children out at the new size.
// ===================================================================================================
void ApplyLayoutConstants(int cols, int rows) {
    for (int id = 0; id < kImmCount; ++id) {
        const Imm& imm = kImms[id];
        const int v = ImmValue(id, cols, rows);
        if (imm.nBytes == 4) Patch4(imm.uInstr + imm.uOperandOffset, static_cast<unsigned int>(v));
        else Patch1(imm.uInstr + imm.uOperandOffset, static_cast<unsigned char>(v));
    }
    for (const Stride& s : kStrides) {
        auto code = StrideBytes(s.modrm, cols);
        PatchMemory(TO_PVOID(s.uAddr), code.data(), code.size());
    }
    g_nCols = cols;
    g_nRows = rows;
}

// Resize commit, modelled on the client's own ToggleFull: write the close-button x, Destroy, CreateWnd.
void ApplySize(int cols, int rows) {
    void* pThis = SehInstance();
    if (!pThis) return;

    cols = Clamp(cols, kMinCols, kMaxCols);
    rows = Clamp(rows, kMinRows, kMaxRows);
    if (!ValidSize(cols, rows)) return;

    // Keep the window where it is: growing from a fixed top-left is what makes an edge drag feel anchored.
    int absL = 0, absT = 0;
    SehWndAbs(pThis, absL, absT);

    ApplyLayoutConstants(cols, rows);

    // Per-tab scroll positions are ROW indices for the old column count; reset rather than rescale.
    __try {
        int* pScroll = At<int*>(kAddr_ScrollPosArray);
        for (int i = 0; i < 5; ++i) pScroll[i] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    SehWriteInt(pThis, kOff_FirstSlot, 1);
    SehWriteInt(pThis, kOff_CloseX, WndW(cols) - 20);

    __try {
        At<void(__thiscall*)(void*)>(kAddr_CWnd_Destroy)(pThis);
        At<void(__thiscall*)(void*, int, int, int, int, int, int, void*, int)>(kAddr_CWnd_CreateWnd)(
            pThis, absL, absT, WndW(cols), WndH(rows), 10, 1, nullptr, 1);
        // Re-apply the selected tab to the rebuilt CCtrlTab, as CWvsContext::OnInventoryGrow does at 0x00A1F8DA;
        // without it the strip shows tab 0 while m_nTabIndex still says another.
        At<void(__thiscall*)(void*)>(kAddr_ApplySavedTab)(pThis);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    Invalidate(pThis);
}

// The size lives in our own key, one DWORD per character. Not in CConfig's CHARACTER_OPT: the client deletes and
// rewrites that registry key with its fixed option list when the character enters the field (TEST, 2026-10-25:
// invCols survived exit and the login screen, then was gone in the field), so anything added there is lost.
constexpr const char* kStoreKey = "Software\\SlumsMS\\Inventory";

void StoreValueName(char (&name)[32]) {
    sprintf_s(name, "w%d_c%u", g_nWorldID, g_dwCharacterID);
}

void SaveSize() {
    if (!g_bCharacterLoaded) return;
    char name[32];
    StoreValueName(name);
    const DWORD v = PackSize(g_nCols, g_nRows);
    const LSTATUS st = RegSetKeyValueA(HKEY_CURRENT_USER, kStoreKey, name, REG_DWORD, &v, sizeof(v));
    DebugMessage("Inventory resize: saved %dx%d for %s (%ld)", g_nCols, g_nRows, name, st);
}

// Once per character load (InvResize_OnLoadCharacter); a missing or invalid value means the stock 4x6.
void LoadSizeOnce() {
    if (g_bLoaded || !g_bCharacterLoaded) return;
    g_bLoaded = true;
    char name[32];
    StoreValueName(name);
    DWORD v = 0, cb = sizeof(v);
    int c = kStockCols, r = kStockRows;
    if (RegGetValueA(HKEY_CURRENT_USER, kStoreKey, name, RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS) {
        UnpackSize(v, c, r);
    }
    if (c != g_nCols || r != g_nRows) ApplyLayoutConstants(c, r);
    DebugMessage("Inventory resize: %dx%d for %s", g_nCols, g_nRows, name);
}

// ===================================================================================================
// Locked slots: cells past the tab's capacity get the client's own Item/disabled art (or a grey wash).
// ===================================================================================================
IWzCanvas* DisabledSlotArt() {
    static bool bTried = false;
    static IWzCanvasPtr pArt;
    if (!bTried) {
        bTried = true;
        try {
            pArt = get_unknown(get_rm()->GetObjectA(L"UI/UIWindow.img/Item/disabled"));
        } catch (...) {
        }
        if (!pArt) LOG_ONCE("Inventory resize: Item/disabled is missing; locked slots get a grey wash");
    }
    return pArt;
}

// +0x14 is the client's own "created" test: CreateWnd sets it from a counter (0x009DE4DF), Destroy early-outs on
// 0 (0x009E00C2) and clears it (0x009E013D). The singleton outlives a close and CreateWnd would REOPEN a closed
// window, so without this the catch-up resize would pop the inventory open every frame.
bool IsWindowLive(void* pThis) {
    return pThis && SehReadInt(pThis, kOff_CreateId) != 0;
}

void DrawLockedCells(void* pThis) {
    const int cells = g_nCols * g_nRows;
    const int nFirst = SehReadInt(pThis, kOff_FirstSlot);
    const int v0 = FirstLockedCell(nFirst, SehTabSlotCount(pThis), cells);
    if (v0 >= cells) return;
    try {
        IWzCanvasPtr pCanvas = reinterpret_cast<CWnd*>(pThis)->GetCanvas();
        if (!pCanvas) return;
        IWzCanvas* pArt = DisabledSlotArt();
        for (int v = v0; v < cells; ++v) {
            SlotRect r;
            if (!SlotRectFor(nFirst + v, nFirst, nFirst + cells, g_nCols, r)) continue;
            if (pArt) {
                pCanvas->CopyEx(r.left, r.top, pArt, CANVAS_ALPHATYPE::CA_OVERWRITE, kIcon, kIcon, 0, 0, kIcon, kIcon);
            } else {
                pCanvas->raw_DrawRectangle(r.left, r.top, kIcon, kIcon, 0x80808080);   // blends: greys the cell
            }
        }
    } catch (...) {
        LOG_ONCE("Inventory resize: drawing locked slots failed; they stay plain");
    }
}

// ===================================================================================================
// New-item tab flash, centred on the stretched tab (see invresize_patch.h).
// ===================================================================================================
bool SehTabEdges(void* pThis, int nTab, int& left, int& right) {
    __try {
        char* pCtrl = *reinterpret_cast<char**>(reinterpret_cast<char*>(pThis) + kOff_TabCtrl);
        if (!pCtrl) return false;
        const int idx = nTab - 1;
        if (idx < 0 || idx >= *reinterpret_cast<int*>(pCtrl + kOff_TabCount)) return false;
        At<void(__thiscall*)(void*)>(kAddr_TabLayout)(pCtrl);   // the edges are only set by a layout pass
        char* pTab = At<char*(__thiscall*)(void*, int)>(kAddr_TabGetAt)(pCtrl + kOff_TabList, idx);
        if (!pTab) return false;
        left = *reinterpret_cast<int*>(pTab + kOff_TabLeft);
        right = *reinterpret_cast<int*>(pTab + kOff_TabRight);
        return right > left;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

}  // namespace

int __stdcall NewTabBase(void* pThis, int nTab) {
    const int stock = nTab * kStockTabPitch;
    if (g_nCols == kStockCols) return stock;
    int left = 0, right = 0;
    if (!SehTabEdges(pThis, nTab, left, right)) return stock;
    return FlashBase(left, right);
}

// Replaces `mov ecx,eax / imul ecx,ecx,0x22` (5 bytes): in eax = tab, esi = CUIItem; out ecx = base.
__declspec(naked) void NewTabX_thunk() {
    __asm {
        push edx
        push eax
        push eax
        push esi
        call NewTabBase
        mov ecx, eax
        pop eax
        pop edx
        ret
    }
}

namespace {

// ===================================================================================================
// Hooks
// ===================================================================================================
typedef void(__thiscall* t_Draw)(void*, const RECT*);
auto CUIItem_Draw = reinterpret_cast<t_Draw>(static_cast<uintptr_t>(kAddr_Draw));

void __fastcall Draw_hook(void* pThis, void* /*edx*/, const RECT* pRect) {
    CUIItem_Draw(pThis, pRect);
    if (IsWindowLive(pThis)) DrawLockedCells(pThis);
}

typedef void(__thiscall* t_OnCreate)(void*, void*);
auto CUIItem_OnCreate = reinterpret_cast<t_OnCreate>(static_cast<uintptr_t>(kAddr_OnCreate));

// Does not load the config: the constructor reads the size immediates at its CreateWnd call site, before
// OnCreate runs, so a change here would lay children out at a new size inside a window built at the old one.
void __fastcall OnCreate_hook(void* pThis, void* /*edx*/, void* pData) {
    CUIItem_OnCreate(pThis, pData);
    // Stock OnCreate has installed Item/backgrnd; only replace it when the built size is the grid we expect.
    const int W = SehReadInt(pThis, kOff_Width);
    const int H = SehReadInt(pThis, kOff_Height);
    if (W == WndW(g_nCols) && H == WndH(g_nRows)) {
        InstallBackground(pThis, g_nCols, g_nRows);
    }
}

// The expand button (0x7D5) becomes a 4 <-> 8 column toggle; m_bFull is forced to 0.
void __fastcall ToggleFull_hook(void* /*pThis*/, void* /*edx*/) {
    g_pendCols = (g_nCols >= 8) ? kMinCols : 8;
    g_pendRows = g_nRows;
    g_nDragStartCols = g_nCols;   // saved by the tick once applied, like a drag
    g_nDragStartRows = g_nRows;
    g_bSaveAfterDrag = true;
}

// ===================================================================================================
// Grip input. The client has no Win32 mouse capture, so the drag starts in the WndProc and continues from the
// per-frame tick, which also owns the rebuild (CWnd construction stays on the main loop).
// ===================================================================================================
int GripUnderCursor(void*& pOutWnd) {
    pOutWnd = nullptr;
    void* pThis = SehInstance();
    if (!pThis) return 0;
    const int W = SehReadInt(pThis, kOff_Width);
    const int H = SehReadInt(pThis, kOff_Height);
    if (W < 80 || W > 2000 || H < 80 || H > 2000) return 0;
    POINT sp;
    if (!GetAbsCursor(sp)) return 0;
    int absL = 0, absT = 0;
    SehWndAbs(pThis, absL, absT);
    if (absL <= 0 && absT <= 0) return 0;
    pOutWnd = pThis;
    return GripAt(W, H, sp.x - absL, sp.y - absT);
}

// The first window of a session can be built before the per-character size is known; fix it up once live.
void ReconcileLiveWindow() {
    if (g_bDragging || g_pendCols) return;
    void* pThis = SehInstance();
    if (!IsWindowLive(pThis)) return;
    const int W = SehReadInt(pThis, kOff_Width);
    const int H = SehReadInt(pThis, kOff_Height);
    if (W <= 0 || H <= 0) return;
    if (W == WndW(g_nCols) && H == WndH(g_nRows)) return;
    ApplySize(g_nCols, g_nRows);
}

// Every way a drag ends comes here: the WM_LBUTTONUP, the tick seeing the button already up (it usually polls
// GetAsyncKeyState before the message arrives, which used to end the drag without saving), or the window going
// away. The last pending size is kept unless the window is gone, and the tick saves once it is applied.
void EndDrag(bool bKeepPending) {
    if (!g_bDragging) return;
    g_bDragging = false;
    g_nDragMode = 0;
    g_pDragWnd = nullptr;
    if (!bKeepPending) g_pendCols = g_pendRows = 0;
    if (g_nCursorState) { SetCursorState(kCursor_Arrow); g_nCursorState = 0; }
    g_bSaveAfterDrag = true;
}

bool TargetsMatch() {
    for (const Guard& g : kGuards) {
        if (memcmp(At<const void*>(g.uAddr), g.pBytes, g.uSize) != 0) {
            DebugMessage("Inventory resize: unexpected client bytes at 0x%08X (%s); stock 4x6 window kept",
                g.uAddr, g.sName);
            return false;
        }
    }
    return true;
}

bool EnabledInConfig() {
    char value[32] = {};
    GetPrivateProfileStringA("clienthax", "invresize", "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    return integration::ParseIniBool(value).value_or(true);
}

}  // namespace
}  // namespace InvResize

// Returns true when it consumed the message (and rewrites msg to WM_NULL, so passing it on is harmless).
bool InvResize_HandleMouseMessage(UINT& msg) {
    using namespace InvResize;
    if (!g_bInstalled) return false;

    if (msg == WM_LBUTTONDOWN && !g_bDragging) {
        void* pWnd = nullptr;
        const int nGrip = GripUnderCursor(pWnd);
        if (!nGrip) return false;
        g_bDragging = true;
        g_nDragMode = nGrip;
        g_pDragWnd = pWnd;
        g_nDragStartCols = g_nCols;
        g_nDragStartRows = g_nRows;
        msg = WM_NULL;   // no window move, no click into the grid underneath
        return true;
    }

    if (msg == WM_LBUTTONUP && g_bDragging) {
        EndDrag(true);
        msg = WM_NULL;
        return true;
    }

    if (msg == WM_MOUSEMOVE && g_bDragging) {
        void* pThis = SehInstance();
        if (!pThis || pThis != g_pDragWnd) { EndDrag(false); return false; }
        POINT sp;
        if (!GetAbsCursor(sp)) return false;
        int absL = 0, absT = 0;
        SehWndAbs(pThis, absL, absT);

        int cols = g_nCols, rows = g_nRows;
        if (g_nDragMode == 1 || g_nDragMode == 3) cols = SnapCols(sp.x - absL + 1);
        if (g_nDragMode == 2 || g_nDragMode == 3) rows = SnapRows(sp.y - absT + 1);
        cols = Clamp(cols, kMinCols, kMaxCols);
        rows = Clamp(rows, kMinRows, kMaxRows);

        // Stay under the imm8 ceiling, giving way on the axis the player is NOT dragging.
        while (cols * rows > kMaxCells) {
            if (g_nDragMode == 1 && rows > kMinRows)      --rows;
            else if (g_nDragMode == 2 && cols > kMinCols) --cols;
            else if (cols >= rows && cols > kMinCols)     --cols;
            else if (rows > kMinRows)                     --rows;
            else break;
        }
        // Keep the whole window on screen.
        const int sw = get_screen_width(), sh = get_screen_height();
        while (cols > kMinCols && absL + WndW(cols) > sw - 8) --cols;
        while (rows > kMinRows && absT + WndH(rows) > sh - 8) --rows;

        if (cols != g_nCols || rows != g_nRows) { g_pendCols = cols; g_pendRows = rows; }
        // Not consumed: the move flows on so the cursor keeps tracking.
    }
    return false;
}

// From CConfig::LoadCharacter_hook (resolution.cpp): the character whose size the tick loads next.
void InvResize_OnLoadCharacter(int nWorldID, unsigned int dwCharacterId) {
    using namespace InvResize;
    g_nWorldID = nWorldID;
    g_dwCharacterID = dwCharacterId;
    g_bCharacterLoaded = true;
    g_bLoaded = false;
}

// From the HUD window's "Reset UI layout": this character's inventory back to the stock 4x6.
void InvResize_ResetSize() {
    using namespace InvResize;
    if (!g_bInstalled || !g_bCharacterLoaded) return;
    char name[32];
    StoreValueName(name);
    RegDeleteKeyValueA(HKEY_CURRENT_USER, kStoreKey, name);
    g_pendCols = kStockCols;
    g_pendRows = kStockRows;
    DebugMessage("Inventory resize: reset to %dx%d for %s", kStockCols, kStockRows, name);
}

void InvResize_Tick() {
    using namespace InvResize;
    if (!g_bInstalled) return;

    LoadSizeOnce();

    // Lifetime safety: closing the inventory, a channel change or a shop entry mid-drag must not leave the next
    // tick calling CWnd::Destroy on freed memory.
    if (g_bDragging) {
        void* pNow = SehInstance();
        if (!pNow || pNow != g_pDragWnd) {
            EndDrag(false);
        } else if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            EndDrag(true);
        }
    }

    if (g_pendCols && (g_pendCols != g_nCols || g_pendRows != g_nRows)) {
        ApplySize(g_pendCols, g_pendRows);
    }
    g_pendCols = g_pendRows = 0;

    if (g_bSaveAfterDrag && !g_bDragging) {
        g_bSaveAfterDrag = false;
        if (g_nCols != g_nDragStartCols || g_nRows != g_nDragStartRows) SaveSize();
    }

    ReconcileLiveWindow();

    // Cursor feedback runs here: setting it from WM_MOUSEMOVE freezes it. Only touched over a grip or mid-drag.
    void* pWnd = nullptr;
    if (g_bDragging || GripUnderCursor(pWnd)) {
        SetCursorState(kCursor_Resize);
        g_nCursorState = kCursor_Resize;
    } else if (g_nCursorState) {
        SetCursorState(kCursor_Arrow);
        g_nCursorState = 0;
    }
}

void AttachInvResizeMod() {
    using namespace InvResize;
    if (!EnabledInConfig()) {
        DebugMessage("Inventory resize: off in config.ini ([clienthax] invresize=false); stock window kept");
        return;
    }
    if (!TargetsMatch()) {
        return;
    }

    // Force narrow mode before anything can construct a CUIItem.
    PatchMemory(TO_PVOID(kAddr_NarrowGetterCall), const_cast<unsigned char*>(kXorEaxNops.data()), kXorEaxNops.size());

    // Seed the immediates at stock 4x6 so the first window is right even before the config read.
    ApplyLayoutConstants(kStockCols, kStockRows);

    PatchJmp(kAddr_GetSlotRect, CastHook(&GetSlotRect_hook));
    PatchJmp(kAddr_GetSlotPosPoint, CastHook(&GetSlotPosFromPoint_hook));
    PatchJmp(kAddr_ToggleFull, CastHook(&ToggleFull_hook));
    if (!(ATTACH_HOOK(CUIItem_OnCreate, OnCreate_hook))) {
        DebugMessage("Inventory resize: OnCreate detour failed; background stays stock");
    }
    if (!(ATTACH_HOOK(CUIItem_Draw, Draw_hook))) {
        DebugMessage("Inventory resize: Draw detour failed; slots past the tab's size are not marked");
    }
    PatchCall(kAddr_NewTabX, &NewTabX_thunk);

    g_bInstalled = true;
    DebugMessage("Inventory resize: ready (grid %d-%d cols x %d-%d rows, max %d cells; narrow mode forced at 0x%08X)",
        kMinCols, kMaxCols, kMinRows, kMaxRows, kMaxCells, kAddr_NarrowGetterCall);
}
