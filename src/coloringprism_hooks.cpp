// ============================================================
// coloringprism_hooks.cpp: the host hooks the Coloring Prism package expects (MAPLEROOT_COLOR_PRISM)
//
// coloringprism.cpp owns no addresses: it exports predicates that the host's cash-item-use and
// item-drop hooks are meant to call. This DLL had no hooks on any of them, so they live here,
// once, and nothing else may Detour these four addresses.
//
// Every address is a function start in the v83 MapleStory.exe (sha256 86e4e6be...aac90, the
// test client's), and every signature was read off its epilogue and call sites:
//
//   0x004863D5  get_consume_cash_item_type   int __cdecl(int nItemID); plain `ret` (0x0048641C,
//               0x00486420, 0x00486432). Answers 0 for group 578, so a prism double-click is
//               dropped before any send path runs. 12 call sites, hence an exact-id test.
//   0x00A0A63F  SendConsumeCashItemUseRequest  __thiscall(this, nPOS, nItemID, int, ZXString<char>);
//               `ret 0x10` at 0x00A0EAD1. Call site 0x004F503C pushes ZXString, [ebp+0xC],
//               nItemID, nPOS; the callee reads nItemID at [ebp+0xC] and passes it to the gate.
//   0x00A1DC5B  SendEtcCashItemUseRequest      __thiscall(this, nPOS, nItemID); `ret 8` at 0x00A1DCE3.
//   0x004EF140  CDraggableItem::OnDropped      int __thiscall(this, pFrom, pTo, rx, ry); `ret 0x10`
//               at 0x004EFD22. [this+0x18] is the source inventory type (compared with 1/2),
//               [this+0x1C] its position (compared with -200/-201), [this+0x24] the source handler.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "ztl/ztl.h"

#include <windows.h>

bool ColorPrism_IsPrismItem(int nItemID);
void ColorPrism_OnUse(int nPOS, int nItemID);
bool ColorPrism_HandleItemDrop(void* pTo, int invType, int invPos);

namespace {

auto get_consume_cash_item_type = reinterpret_cast<int(__cdecl*)(int)>(0x004863D5);
int __cdecl get_consume_cash_item_type_hook(int nItemID) {
    if (ColorPrism_IsPrismItem(nItemID)) return 1;
    return get_consume_cash_item_type(nItemID);
}

auto SendConsumeCashItemUseRequest =
    reinterpret_cast<void(__thiscall*)(void*, int, int, int, ZXString<char>)>(0x00A0A63F);
void __fastcall SendConsumeCashItemUseRequest_hook(void* pThis, void*, int nPOS, int nItemID, int nArg,
                                                   ZXString<char> sArg) {
    if (ColorPrism_IsPrismItem(nItemID)) {
        ColorPrism_OnUse(nPOS, nItemID);
        return;   // swallowed: the server consumes the prism on Confirm; sArg is released here
    }
    SendConsumeCashItemUseRequest(pThis, nPOS, nItemID, nArg, sArg);
}

auto SendEtcCashItemUseRequest = reinterpret_cast<void(__thiscall*)(void*, int, int)>(0x00A1DC5B);
void __fastcall SendEtcCashItemUseRequest_hook(void* pThis, void*, int nPOS, int nItemID) {
    if (ColorPrism_IsPrismItem(nItemID)) {
        ColorPrism_OnUse(nPOS, nItemID);
        return;
    }
    SendEtcCashItemUseRequest(pThis, nPOS, nItemID);
}

// SEH leaf: pThis is not guaranteed to be a live CDraggableItem on every path.
bool ReadDragSource(void* pThis, int& invType, int& invPos) {
    if (!pThis) return false;
    __try {
        invType = *reinterpret_cast<int*>(reinterpret_cast<char*>(pThis) + 0x18);
        invPos  = *reinterpret_cast<int*>(reinterpret_cast<char*>(pThis) + 0x1C);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

auto CDraggableItem_OnDropped = reinterpret_cast<int(__thiscall*)(void*, void*, void*, int, int)>(0x004EF140);
int __fastcall CDraggableItem_OnDropped_hook(void* pThis, void*, void* pFrom, void* pTo, int rx, int ry) {
    int invType = 0, invPos = 0;
    // Only inventory drags (types 1..5) are prism targets. This DLL has no custom drag window that
    // writes its own meaning into +0x18/+0x1C; if one is added, compare [this+0x24] against it here.
    if (ReadDragSource(pThis, invType, invPos) && invType >= 1 && invType <= 5 && invPos != 0
        && ColorPrism_HandleItemDrop(pTo, invType, invPos)) {
        return 1;   // consumed: the item must not also be moved
    }
    return CDraggableItem_OnDropped(pThis, pFrom, pTo, rx, ry);
}

}  // namespace

void AttachColoringPrismHostHooks() {
    ATTACH_HOOK(get_consume_cash_item_type, get_consume_cash_item_type_hook);
    ATTACH_HOOK(SendConsumeCashItemUseRequest, SendConsumeCashItemUseRequest_hook);
    ATTACH_HOOK(SendEtcCashItemUseRequest, SendEtcCashItemUseRequest_hook);
    ATTACH_HOOK(CDraggableItem_OnDropped, CDraggableItem_OnDropped_hook);
}
