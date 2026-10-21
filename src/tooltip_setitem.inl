// ============================================================
// tooltip_setitem.inl — SetEquip addon (ticket 18): the GMS-style second tooltip beside the equip tooltip.
// Included by tooltip.cpp only (MAPLEROOT_SET_EQUIP), after GW_ItemSlotEquip and the Equip_Basic hook.
//
// Merged from the addon's tooltip_set_item_snippet.cpp (kaentakedev; same client.exe, so the same addresses):
//   * a second real CUIToolTip, built in a raw buffer with the native constructor (0x008E49B5);
//   * SetToolTip_String2 (0x008E7150) is fed blank lines only to size and place that box, then the content is
//     drawn straight onto its canvas: piece names green when worn / grey when not, tier headers purple, tier
//     stats green once reached;
//   * it is shown from DrawToolTip_Equip (0x008ED0D2) whenever the main equip tooltip draws a set item, and hidden
//     from ClearToolTip (0x008E6E23) when that main tooltip clears, with a 20 s orphan timeout as a safety net.
// The main tooltip only gets the two fixed rows added in SetToolTip_Equip_Basic_hook.
// ============================================================

namespace {

constexpr uintptr_t kAddr_ToolTipCtor     = 0x008E49B5;   // CUIToolTip::CUIToolTip (same as wvs/tooltip.h)
constexpr uintptr_t kAddr_SetFontSE       = 0x0046341A;   // IWzFont::Create(name, size, color, style)
constexpr uintptr_t kAddr_InputSystemSE   = 0x00BEC33C;   // CInputSystem*; m_hWnd at +0
constexpr size_t kToolTipBytes = 0x600;                   // native object is 0x518 (MapleRoot BossHP uses 1304)

alignas(8) unsigned char g_seBuffer[kToolTipBytes];
bool g_seConstructed = false;
CUIToolTip* g_mainEquipTip = nullptr;   // the engine's equip tooltip, seen in SetToolTip_Equip_Basic_hook

int g_sePendingItem = 0;                // set item the main tooltip is showing (0 = none)
int g_seShownItem = 0;
DWORD g_seLastSeen = 0;
constexpr DWORD kSeOrphanMs = 20000;

int g_seBuiltItem = 0, g_seBuiltX = INT_MIN, g_seBuiltY = INT_MIN, g_seBuiltPieces = -1;
int g_seAnchorItem = 0, g_seAnchorX = 0, g_seAnchorY = 0;   // cursor, sampled once per hover

constexpr unsigned long kSeFontSize   = 12;
constexpr unsigned long kSeTitle      = 0xFFFFD45A;
constexpr unsigned long kSeGreen      = 0xFF6EDB7E;
constexpr unsigned long kSeGrey       = 0xFF8A8A8A;
constexpr unsigned long kSePurple     = 0xFFC79CFF;
constexpr unsigned long kSeShadow     = 0xFF000000;
constexpr unsigned long kSeFill       = 0xFF000000;
constexpr unsigned long kSeTitleFill  = 0xFF1E1E1E;
constexpr unsigned long kSeSeparator  = 0xFF6B4FA0;
constexpr unsigned long kSeBorder     = 0xFFB8A0E0;
constexpr int kSeMinWidth = 220, kSeMaxWidth = 320, kSeWidthPad = 26;
constexpr int kSeLeft = 12, kSeTop = 34, kSeLineH = 15, kSeSepH = 10, kSeTitleBarH = 20, kSeTitleTop = 9;
constexpr int kSeMaxLines = 60;         // a long set is cut here (Visitor/Frozen list many alternatives)

enum class LineKind { Owned, Missing, Separator, Header, Active, Inactive };
struct Line {
    std::string text;
    LineKind kind;
};

struct Fonts {
    IWzFontPtr title, owned, missing, header, active, inactive, shadow;
    bool tried = false;
} g_seFonts;

bool MakeSeFont(IWzFontPtr& out, unsigned long color, bool bold) {
    if (out) return true;
    try {
        PcCreateObject<IWzFontPtr>(L"Canvas#Font", out, nullptr);
        if (!out) return false;
        auto create = reinterpret_cast<HRESULT(__thiscall*)(IWzFont*, Ztl_bstr_t, unsigned long, unsigned long, const Ztl_variant_t&)>(kAddr_SetFontSE);
        return SUCCEEDED(create(out, Ztl_bstr_t(L"Dotum"), kSeFontSize, color, bold ? Ztl_variant_t(L"B") : Ztl_variant_t(L"")));
    } catch (...) {
        out = nullptr;
        return false;
    }
}

bool SeFontsReady() {
    if (!g_seFonts.tried) {
        g_seFonts.tried = true;
        MakeSeFont(g_seFonts.title, kSeTitle, true);
        MakeSeFont(g_seFonts.owned, kSeGreen, false);
        MakeSeFont(g_seFonts.missing, kSeGrey, false);
        MakeSeFont(g_seFonts.header, kSePurple, true);
        MakeSeFont(g_seFonts.active, kSeGreen, false);
        MakeSeFont(g_seFonts.inactive, kSeGrey, false);
        MakeSeFont(g_seFonts.shadow, kSeShadow, false);
    }
    return g_seFonts.title && g_seFonts.owned && g_seFonts.missing && g_seFonts.header && g_seFonts.active
        && g_seFonts.inactive && g_seFonts.shadow;
}

IWzFontPtr FontFor(LineKind k) {
    switch (k) {
    case LineKind::Owned: return g_seFonts.owned;
    case LineKind::Missing: return g_seFonts.missing;
    case LineKind::Header: return g_seFonts.header;
    case LineKind::Active: return g_seFonts.active;
    case LineKind::Inactive: return g_seFonts.inactive;
    default: return nullptr;
    }
}

int TextWidth(IWzFontPtr f, const std::string& s) {
    if (!f || s.empty()) return 0;
    try {
        return static_cast<int>(f->CalcTextWidth(Ztl_bstr_t(s.c_str()), Ztl_variant_t()));
    } catch (...) {
        return static_cast<int>(s.size()) * 7;
    }
}

void ShadowText(IWzCanvasPtr c, int x, int y, const std::string& s, IWzFontPtr f) {
    if (!c || !f || s.empty()) return;
    try {
        c->DrawTextA(x + 1, y + 1, s.c_str(), g_seFonts.shadow);
        c->DrawTextA(x, y, s.c_str(), f);
    } catch (...) {
    }
}

CUIToolTip* SetEffectTip() {
    if (!g_seConstructed) {
        reinterpret_cast<void(__thiscall*)(void*)>(kAddr_ToolTipCtor)(g_seBuffer);
        g_seConstructed = true;
    }
    return reinterpret_cast<CUIToolTip*>(g_seBuffer);
}

std::vector<Line> BuildLines(const setitem::Def& def, const std::vector<int>& worn) {
    std::vector<Line> lines;
    const int pieces = static_cast<int>(worn.size());
    for (const auto& item : def.items) {
        const bool on = std::find(worn.begin(), worn.end(), item.itemId) != worn.end();
        lines.push_back({item.name, on ? LineKind::Owned : LineKind::Missing});
    }
    lines.push_back({std::string(), LineKind::Separator});
    for (const auto& tier : def.tiers) {
        const bool reached = pieces >= tier.pieces;
        lines.push_back({std::to_string(tier.pieces) + " Set Items Equipped", LineKind::Header});
        for (const auto& [key, value] : tier.stats) {
            lines.push_back({setitem::StatLabel(key) + (value < 0 ? ": " : ": +") + std::to_string(value),
                             reached ? LineKind::Active : LineKind::Inactive});
        }
    }
    if (lines.size() > static_cast<size_t>(kSeMaxLines)) {
        lines.resize(kSeMaxLines - 1);
        lines.push_back({"...", LineKind::Inactive});
    }
    return lines;
}

int ContentHeight(const std::vector<Line>& lines) {
    int h = 0;
    for (const auto& l : lines) h += l.kind == LineKind::Separator ? kSeSepH : kSeLineH;
    return h;
}

void DrawSetEffect(CUIToolTip* tip, const std::string& title, const std::vector<Line>& lines) {
    if (!tip || !tip->m_pLayer || !SeFontsReady()) return;
    IWzCanvasPtr c;
    try {
        c = tip->m_pLayer->canvas[0];
    } catch (...) {
        return;
    }
    const int w = tip->m_nWidth, h = tip->m_nHeight;
    if (!c || w <= 0 || h <= 0) return;
    try {
        c->DrawRectangle(0, 0, w, h, kSeFill);
        c->DrawRectangle(2, 2, (std::max)(0, w - 4), kSeTitleBarH, kSeTitleFill);
        c->DrawRectangle(6, kSeTitleBarH + 4, (std::max)(0, w - 12), 1, kSeSeparator);
        c->DrawRectangle(0, 0, w, 1, kSeBorder);
        c->DrawRectangle(0, (std::max)(0, h - 1), w, 1, kSeBorder);
        c->DrawRectangle(0, 0, 1, h, kSeBorder);
        c->DrawRectangle((std::max)(0, w - 1), 0, 1, h, kSeBorder);
    } catch (...) {
        return;
    }
    const int tw = TextWidth(g_seFonts.title, title);
    ShadowText(c, (std::max)(kSeLeft, (w - tw) / 2), kSeTitleTop, title, g_seFonts.title);
    int y = kSeTop;
    for (const auto& l : lines) {
        if (l.kind == LineKind::Separator) {
            try {
                c->DrawRectangle(kSeLeft, y + kSeSepH / 2, (std::max)(0, w - 2 * kSeLeft), 1, kSeSeparator);
            } catch (...) {
            }
            y += kSeSepH;
            continue;
        }
        ShadowText(c, kSeLeft, y, l.text, FontFor(l.kind));
        y += kSeLineH;
    }
}

void HideSetEffect() {
    if (!g_seShownItem) return;
    SetEffectTip()->ClearToolTip();
    g_seShownItem = 0;
    g_seAnchorItem = 0;
    g_seBuiltItem = 0;
    g_seBuiltPieces = -1;
}

void ShowSetEffect(int itemId, int x, int y) {
    setitem::Def def;
    std::vector<int> worn;
    if (!SetItem_Find(itemId, def, worn)) return;
    const int pieces = static_cast<int>(worn.size());
    if (itemId == g_seBuiltItem && x == g_seBuiltX && y == g_seBuiltY && pieces == g_seBuiltPieces) {
        g_seShownItem = itemId;
        g_seLastSeen = GetTickCount();
        return;   // unchanged: redrawing every frame reads as flicker
    }
    const std::vector<Line> lines = BuildLines(def, worn);
    const std::string title = def.name + " " + std::to_string(pieces) + "/" + std::to_string(def.complete);
    int width = kSeMinWidth;
    if (SeFontsReady()) {
        width = TextWidth(g_seFonts.title, title);
        for (const auto& l : lines) width = (std::max)(width, TextWidth(FontFor(l.kind), l.text));
        width = (std::min)((std::max)(width + kSeWidthPad, kSeMinWidth), kSeMaxWidth);
    }
    const int target = kSeTop + ContentHeight(lines) + 6;

    // Blank placeholder lines size the native box; add more until it is tall enough for the drawn content.
    std::string blank;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) blank += '\n';
        if (lines[i].kind != LineKind::Separator) blank += ' ';
    }
    CUIToolTip* tip = SetEffectTip();
    for (int extra = 0; extra <= 24; ++extra) {
        tip->ClearToolTip();
        tip->SetToolTip_String2(x, y, ZXString<char>(" "), ZXString<char>(blank.c_str()), 0, 0, 0, width, 1, 0);
        if (tip->m_nHeight >= target) break;
        blank += "\n ";
    }
    DrawSetEffect(tip, title, lines);

    g_seBuiltItem = itemId;
    g_seBuiltX = x;
    g_seBuiltY = y;
    g_seBuiltPieces = pieces;
    g_seShownItem = itemId;
    g_seLastSeen = GetTickCount();
}

auto CUIToolTip__ClearToolTip = reinterpret_cast<void(__thiscall*)(CUIToolTip*)>(0x008E6E23);
auto CUIToolTip__DrawToolTip_Equip = reinterpret_cast<void(__thiscall*)(CUIToolTip*, int, GW_ItemSlotEquip*)>(0x008ED0D2);

void __fastcall CUIToolTip__ClearToolTip_hook(CUIToolTip* pThis, void* /*edx*/) {
    CUIToolTip__ClearToolTip(pThis);
    if (pThis == g_mainEquipTip) {
        HideSetEffect();
    }
}

void __fastcall CUIToolTip__DrawToolTip_Equip_hook(CUIToolTip* pThis, void* /*edx*/, int a2, GW_ItemSlotEquip* pe) {
    CUIToolTip__DrawToolTip_Equip(pThis, a2, pe);
    if (!pe || !pThis || !pThis->m_pLayer || !g_sePendingItem) return;
    const int itemId = pe->nItemID;
    if (itemId != g_sePendingItem) return;
    if (itemId != g_seAnchorItem) {
        POINT pt{};
        void* input = *reinterpret_cast<void**>(kAddr_InputSystemSE);
        HWND hWnd = input ? *reinterpret_cast<HWND*>(input) : nullptr;
        if (!hWnd || !::GetCursorPos(&pt) || !::ScreenToClient(hWnd, &pt)) return;
        g_seAnchorItem = itemId;
        g_seAnchorX = pt.x;
        g_seAnchorY = pt.y;
    }
    // Native equip tooltips draw with their top-left at the cursor; dock to the right of it.
    ShowSetEffect(itemId, g_seAnchorX + pThis->m_nWidth + 6, g_seAnchorY);
}

}  // namespace

static void SetEffect_OnEquipToolTip(CUIToolTip* pTip, int nItemID) {
    g_mainEquipTip = pTip;
    setitem::Def def;
    std::vector<int> worn;
    g_sePendingItem = SetItem_Find(nItemID, def, worn) ? nItemID : 0;
}

void SetEffectToolTip_Shutdown() {
    if (g_seConstructed) {
        SetEffectTip()->ClearToolTip();
    }
    g_seShownItem = 0;
    g_sePendingItem = 0;
    g_mainEquipTip = nullptr;
    g_seFonts = Fonts{};
}

void SetEffectToolTip_Tick() {
    if (g_seShownItem && GetTickCount() - g_seLastSeen > kSeOrphanMs) {
        HideSetEffect();
    }
}
