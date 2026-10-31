// ============================================================
// slidemenu.cpp: the Dimensional Mirror's slide menu no longer reads past its text and closes the client (b105).
// slidemenu_patch.h has the analysis.
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "slidemenu_patch.h"
#include "ztl/ztl.h"

namespace {

// ZXString<char> by value: the stock parser releases it (0x0076B95B), like every by-value ZXString.
auto CSlideMenu__Parse = reinterpret_cast<void(__thiscall*)(void*, ZXString<char>)>(
    static_cast<uintptr_t>(SlideMenu::kAddr_Parse));

int g_logged = 0;
constexpr int kMaxLogLines = 5;

void __fastcall Parse_hook(void* pThis, void* /*edx*/, ZXString<char> sText) {
    const char* text = sText;
    const int need = text ? SlideMenu::ClosingHashesNeeded(text, static_cast<size_t>(sText.GetLength())) : 0;
    if (need == 0) {
        CSlideMenu__Parse(pThis, sText);
        return;
    }
    ZXString<char> sClosed(text, sText.GetLength());
    for (int i = 0; i < need; ++i) sClosed.Cat("#");
    if (g_logged < kMaxLogLines) {
        ++g_logged;
        DebugMessage("Slide menu: text of %d chars has no closing '#'; %d appended so the menu stays inside it",
                     sText.GetLength(), need);
    }
    CSlideMenu__Parse(pThis, sClosed);
}

}  // namespace

void AttachSlideMenuMod() {
    using namespace SlideMenu;
    const auto* site = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(kAddr_Parse));
    switch (Classify(site)) {
    case SiteState::Detoured:
        DebugMessage("Slide menu: 0x%08X is already detoured; stock behaviour kept", kAddr_Parse);
        return;
    case SiteState::Unknown:
        DebugMessage("Slide menu: unexpected client bytes at 0x%08X; stock behaviour kept", kAddr_Parse);
        return;
    default:
        break;
    }
    ATTACH_HOOK(CSlideMenu__Parse, Parse_hook);
    DebugMessage("Slide menu: a menu text without its closing '#' no longer reads past its end");
}
