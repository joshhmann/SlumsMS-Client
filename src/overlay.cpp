// ============================================================
// overlay.cpp: the ImGui HUD framework on the D3D9 host (1.6.0): theme, font scale, widget layout, freeze.
//
// Widgets are always on (each can be hidden in the HUD window) and keep their place: positions and sizes are
// stored as screen fractions (overlay_policy.h) in HKCU\Software\SlumsMS\Overlay, so they follow resolution
// changes. One freeze toggle, "Edit UI" (Ctrl+F11 or the HUD window): unfrozen, widgets have a title bar and can be
// dragged and resized, and the stock status bar and quick slot get grips that move them (uilayout.cpp); frozen,
// everything is fixed and click-through, so the game gets every click. The theme is the client's
// blue and cream (or plain ImGui); the font is one large Tahoma atlas scaled to the screen height.
// ============================================================

#include "pch.h"
#include "debug.h"
#include "d3d9host.h"
#include "overlay_policy.h"

#include "imgui.h"
#include "misc/freetype/imgui_freetype.h"

#include <cmath>

namespace {

constexpr const char* kKey = "Software\\SlumsMS\\Overlay";
// The font is baked (FreeType, light hinting) at the size it is drawn at: the HUD size times the largest module text
// size, rebuilt when either changes (Overlay_BeforeFrame), so text is never a scaled-up bitmap.
float g_fAtlasPx = 0.0f;
float g_fWantAtlasPx = 32.0f;
char g_szFontPath[MAX_PATH] = {};
constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;   // CUserLocal*: set only while a character is in the field

DWORD ReadPref(const char* name, DWORD fallback) {
    DWORD v = 0, cb = sizeof(v);
    return RegGetValueA(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS ? v : fallback;
}

void WritePref(const char* name, DWORD v) {
    RegSetKeyValueA(HKEY_CURRENT_USER, kKey, name, REG_DWORD, &v, sizeof(v));
}

struct Widget {
    const char* id;      // registry prefix and ImGui window id
    const char* title;   // shown while unfrozen and in the HUD window
    void (*draw)();
    int rows;            // content rows: the window is exactly as tall as its content (+ title bar when unfrozen)
    int defX, defY, defW, defH;   // default layout, fractions x 10000 (height is derived from rows)
    bool on;
    int fx, fy, fw, fh;
    bool loaded;
    bool place;          // apply the stored layout this frame (first frame, freeze change, resolution change)
    int textPct;         // the module's text size, percent of the HUD size
    int alpha;           // background opacity, percent
};

constexpr int kTextDefault = 125;   // module text starts a size up (readability, owner 2026-10-26)

// The modular HUD's modules (owner, 2026-10-26: FFXIV-style, each movable and sized on its own). HP, MP and EXP are
// the 1.6.0 set (ticket 91); quick slots, buffs/cooldowns and chat follow in 1.7.0.
Widget g_widgets[] = {
    {"hp", "HP", &HudVitals_DrawHP, 1, 125, 1750, 2500, 600, true, 0, 0, 0, 0, false, true, kTextDefault, 94},
    {"mp", "MP", &HudVitals_DrawMP, 1, 125, 2375, 2500, 600, true, 0, 0, 0, 0, false, true, kTextDefault, 94},
    {"exp", "EXP", &HudVitals_DrawEXP, 1, 125, 3000, 2500, 600, true, 0, 0, 0, 0, false, true, kTextDefault, 94},
};
constexpr int kTextMin = 60, kTextMax = 200, kAlphaMin = 0, kAlphaMax = 100;

int MaxTextPct() {
    int m = 100;
    for (const auto& w : g_widgets) {
        if (w.on) m = (std::max)(m, w.textPct);
    }
    return m;
}

bool g_bLoaded = false;
bool g_bFrozen = true;
bool g_bSnap = true;   // Edit UI snaps modules (and the stock bar and quick slot) to a 1.25% grid
int g_nTheme = 0;      // 0 SlumsMS, 1 plain ImGui
int g_nScale = 100;    // percent
float g_fLastW = 0, g_fLastH = 0;

void LoadPrefs() {
    if (g_bLoaded) return;
    g_bLoaded = true;
    g_bFrozen = true;   // Edit UI is a temporary mode: every session starts with the layout fixed
    g_nTheme = static_cast<int>(ReadPref("theme", 0));
    g_nScale = static_cast<int>(ReadPref("scale", 100));
    g_bSnap = ReadPref("snap", 1) != 0;
    for (auto& w : g_widgets) {
        char name[64];
        sprintf_s(name, "%s_on", w.id);  w.on = ReadPref(name, 1) != 0;
        sprintf_s(name, "%s_x", w.id);   w.fx = static_cast<int>(ReadPref(name, w.defX));
        sprintf_s(name, "%s_y", w.id);   w.fy = static_cast<int>(ReadPref(name, w.defY));
        sprintf_s(name, "%s_w", w.id);   w.fw = static_cast<int>(ReadPref(name, w.defW));
        sprintf_s(name, "%s_h", w.id);   w.fh = static_cast<int>(ReadPref(name, w.defH));
        sprintf_s(name, "%s_text", w.id);
        w.textPct = (std::min)((std::max)(static_cast<int>(ReadPref(name, kTextDefault)), kTextMin), kTextMax);
        sprintf_s(name, "%s_alpha", w.id);
        w.alpha = (std::min)((std::max)(static_cast<int>(ReadPref(name, 94)), kAlphaMin), kAlphaMax);
        w.loaded = true;
    }
}

void SaveLook(Widget& w) {
    char name[64];
    sprintf_s(name, "%s_text", w.id); WritePref(name, static_cast<DWORD>(w.textPct));
    sprintf_s(name, "%s_alpha", w.id); WritePref(name, static_cast<DWORD>(w.alpha));
}

void SaveLayout(Widget& w) {
    char name[64];
    sprintf_s(name, "%s_x", w.id); WritePref(name, w.fx);
    sprintf_s(name, "%s_y", w.id); WritePref(name, w.fy);
    sprintf_s(name, "%s_w", w.id); WritePref(name, w.fw);
    sprintf_s(name, "%s_h", w.id); WritePref(name, w.fh);
}

void ApplyTheme() {
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    if (g_nTheme == 1) {
        ImGui::StyleColorsDark(&st);
        return;
    }
    ImGui::StyleColorsLight(&st);
    st.WindowRounding = 5.0f;
    st.FrameRounding = 3.0f;
    st.GrabRounding = 3.0f;
    st.WindowBorderSize = 1.0f;
    ImVec4* c = st.Colors;
    // The stock windows' own colours, sampled from UI/UIWindow.img (Item/FullBackgrnd): cream body #EEEEDD, white
    // panels, light blue frame #99BBCC, steel blue border #557799, separators #CCCCBB.
    auto rgb = [](int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); };
    const ImVec4 cream = rgb(0xEE, 0xEE, 0xDD, 0.96f), white = rgb(0xFF, 0xFF, 0xFF), frame = rgb(0x99, 0xBB, 0xCC);
    const ImVec4 frameLight = rgb(0xAA, 0xCC, 0xDD), steel = rgb(0x55, 0x77, 0x99), sep = rgb(0xCC, 0xCC, 0xBB);
    c[ImGuiCol_Text] = rgb(0x33, 0x33, 0x33);
    c[ImGuiCol_TextDisabled] = rgb(0x88, 0x88, 0x77);
    c[ImGuiCol_WindowBg] = cream;
    c[ImGuiCol_PopupBg] = cream;
    c[ImGuiCol_Border] = steel;
    c[ImGuiCol_FrameBg] = white;
    c[ImGuiCol_FrameBgHovered] = rgb(0xEE, 0xF4, 0xF8);
    c[ImGuiCol_FrameBgActive] = rgb(0xDD, 0xEA, 0xF2);
    c[ImGuiCol_TitleBg] = frame;
    c[ImGuiCol_TitleBgActive] = frame;
    c[ImGuiCol_TitleBgCollapsed] = frame;
    c[ImGuiCol_Button] = frame;
    c[ImGuiCol_ButtonHovered] = frameLight;
    c[ImGuiCol_ButtonActive] = steel;
    c[ImGuiCol_CheckMark] = steel;
    c[ImGuiCol_SliderGrab] = frame;
    c[ImGuiCol_SliderGrabActive] = steel;
    c[ImGuiCol_Header] = frameLight;
    c[ImGuiCol_HeaderHovered] = frameLight;
    c[ImGuiCol_HeaderActive] = frame;
    c[ImGuiCol_Separator] = sep;
    c[ImGuiCol_TableBorderLight] = sep;
    c[ImGuiCol_TableBorderStrong] = sep;
    c[ImGuiCol_ResizeGrip] = frameLight;
    c[ImGuiCol_ResizeGripHovered] = frame;
    c[ImGuiCol_ResizeGripActive] = steel;
}

int MaxTextPct();

// The base text size for this screen; the atlas is baked at base x the largest module text size and modules scale
// down from it (SetWindowFontScale), which stays sharp.
void ApplyScale(float screenH) {
    const float base = OverlayPolicy::FontPixels(screenH, g_nScale);
    g_fWantAtlasPx = (std::min)((std::max)(base * MaxTextPct() / 100.0f, 11.0f), 96.0f);
    if (g_fAtlasPx > 0) ImGui::GetIO().FontGlobalScale = base / g_fAtlasPx;
}

void BuildFont(float px) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.SizePixels = px;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
    if (g_szFontPath[0]) {
        io.Fonts->AddFontFromFileTTF(g_szFontPath, px, &cfg);
    } else {
        io.Fonts->AddFontDefault(&cfg);
    }
    g_fAtlasPx = px;
}

}  // namespace

void Overlay_Setup() {
    LoadPrefs();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    char path[MAX_PATH];
    const UINT n = GetWindowsDirectoryA(path, MAX_PATH);
    if (n && n < MAX_PATH - 24) strcat_s(path, "\\Fonts\\tahoma.ttf");
    if (n && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        strcpy_s(g_szFontPath, path);
    } else {
        DebugMessage("Overlay: tahoma.ttf not found, using ImGui's default font");
    }
    BuildFont(g_fWantAtlasPx);
    ApplyTheme();
}

bool Overlay_HasHud() {
    LoadPrefs();
    for (const auto& w : g_widgets) {
        if (w.on) return true;
    }
    return false;
}

bool Overlay_IsFrozen() { return g_bFrozen; }

// Before ImGui's frame (d3d9host.cpp): rebake the font when the size it is drawn at changed by a pixel or more.
// The DX9 backend re-uploads the font texture on its next NewFrame once its device objects are invalidated.
bool Overlay_BeforeFrame() {
    if (g_fAtlasPx > 0 && std::fabs(g_fWantAtlasPx - g_fAtlasPx) < 1.0f) return false;
    BuildFont(std::round(g_fWantAtlasPx));
    ApplyScale(ImGui::GetIO().DisplaySize.y > 0 ? ImGui::GetIO().DisplaySize.y : 600.0f);
    return true;
}
bool Overlay_SnapOn() { return g_bSnap; }

void Overlay_ToggleFreeze() {
    g_bFrozen = !g_bFrozen;
    for (auto& w : g_widgets) w.place = true;
}

void Overlay_DrawHud() {
    LoadPrefs();
    // HUD widgets belong to the field: nothing over the login, world, channel or character screens.
    if (!*reinterpret_cast<void* const*>(kAddr_UserLocal)) return;
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    if (screen.x < 1 || screen.y < 1) return;
    if (screen.x != g_fLastW || screen.y != g_fLastH) {   // first frame or a resolution change
        g_fLastW = screen.x;
        g_fLastH = screen.y;
        ApplyScale(screen.y);
        for (auto& w : g_widgets) w.place = true;
    }
    const float minW = 120.0f * ImGui::GetIO().FontGlobalScale * g_fAtlasPx / 15.0f;
    const float minH = 40.0f * ImGui::GetIO().FontGlobalScale * g_fAtlasPx / 15.0f;

    if (!g_bFrozen && g_bSnap) {   // the grid, faint, every 4th line (5%) drawn
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        for (int f = OverlayPolicy::kGridFrac * 4; f < OverlayPolicy::kFrac; f += OverlayPolicy::kGridFrac * 4) {
            const float x = screen.x * f / OverlayPolicy::kFrac, y = screen.y * f / OverlayPolicy::kFrac;
            bg->AddLine(ImVec2(x, 0), ImVec2(x, screen.y), IM_COL32(255, 255, 255, 28));
            bg->AddLine(ImVec2(0, y), ImVec2(screen.x, y), IM_COL32(255, 255, 255, 28));
        }
    }
    // The stock bar, quick slot and button grips first, so the modules' windows stay above them.
    if (!g_bFrozen) UiLayout_DrawGrips();
    for (auto& w : g_widgets) {
        if (!w.on) continue;
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar;
        if (g_bFrozen) {
            flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;
        }
        // Height follows the content and the font size; only the width is the player's.
        const ImGuiStyle& st = ImGui::GetStyle();
        const float titlePx = ImGui::GetIO().FontGlobalScale * g_fAtlasPx;
        const float fontPx = titlePx * w.textPct / 100.0f;
        const float rowH = fontPx + st.FramePadding.y * 2;
        const float h = w.rows * rowH + (w.rows - 1) * st.ItemSpacing.y + st.WindowPadding.y * 2 +
            (g_bFrozen ? 0.0f : titlePx + st.FramePadding.y * 2);
        const OverlayPolicy::Rect r =
            OverlayPolicy::FromFractions(w.fx, w.fy, w.fw, OverlayPolicy::ToFraction(h, screen.y), screen.x, screen.y,
                minW, (std::min)(minH, h));
        ImGui::SetNextWindowSizeConstraints(ImVec2(minW, h), ImVec2(screen.x, h));
        if (w.place || g_bFrozen) {
            ImGui::SetNextWindowPos(ImVec2(r.x, r.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(r.w, h), ImGuiCond_Always);
            w.place = false;
        }
        char title[96];
        sprintf_s(title, "%s###%s", w.title, w.id);
        ImGui::SetNextWindowBgAlpha(w.alpha / 100.0f);
        if (ImGui::Begin(title, nullptr, flags)) {
            ImGui::SetWindowFontScale(w.textPct / 100.0f);
            w.draw();
        }
        if (!g_bFrozen && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // A drag or resize has finished: store where the player left it.
            const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
            int fx = OverlayPolicy::ToFraction(p.x, screen.x), fy = OverlayPolicy::ToFraction(p.y, screen.y);
            if (g_bSnap) {
                fx = OverlayPolicy::Snap(fx);
                fy = OverlayPolicy::Snap(fy);
                if (fx != w.fx || fy != w.fy) w.place = true;   // move the window onto the grid line next frame
            }
            const int fw = OverlayPolicy::ToFraction(s.x, screen.x), fh = OverlayPolicy::ToFraction(s.y, screen.y);
            if (fx != w.fx || fy != w.fy || fw != w.fw || fh != w.fh) {
                w.fx = fx; w.fy = fy; w.fw = fw; w.fh = fh;
                SaveLayout(w);
            }
        }
        ImGui::End();
    }
}

// The HUD window's controls: they apply at once, no restart.
void Overlay_DrawSettings() {
    LoadPrefs();
    // One row per module: shown, text size, opacity, reset.
    if (ImGui::BeginTable("modules", 4, ImGuiTableFlags_SizingFixedFit)) {
        for (auto& w : g_widgets) {
            ImGui::PushID(w.id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Checkbox(w.title, &w.on)) {
                char name[64];
                sprintf_s(name, "%s_on", w.id);
                WritePref(name, w.on ? 1 : 0);
                w.place = true;
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
            if (ImGui::SliderInt("Text", &w.textPct, kTextMin, kTextMax, "%d%%")) {
                SaveLook(w);
                ApplyScale(ImGui::GetIO().DisplaySize.y);   // the font is rebaked for the new largest size
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
            if (ImGui::SliderInt("Opacity", &w.alpha, kAlphaMin, kAlphaMax, "%d%%")) SaveLook(w);
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Reset")) {
                w.fx = w.defX; w.fy = w.defY; w.fw = w.defW; w.fh = w.defH;
                w.textPct = kTextDefault; w.alpha = 94;
                w.place = true;
                SaveLayout(w);
                SaveLook(w);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    bool editing = !g_bFrozen;
    if (ImGui::Checkbox("Edit UI (Ctrl+F11): move and resize widgets, move the status bar and quick slots", &editing)) {
        Overlay_ToggleFreeze();
    }
    if (ImGui::Checkbox("Snap to grid while editing", &g_bSnap)) WritePref("snap", g_bSnap ? 1 : 0);
    if (ImGui::SliderInt("HUD size", &g_nScale, 60, 160, "%d%%")) {
        WritePref("scale", static_cast<DWORD>(g_nScale));
        ApplyScale(ImGui::GetIO().DisplaySize.y);
    }
    const char* themes[] = {"SlumsMS", "Plain ImGui"};
    if (ImGui::Combo("Look", &g_nTheme, themes, 2)) {
        WritePref("theme", static_cast<DWORD>(g_nTheme));
        ApplyTheme();
    }
    if (ImGui::Button("Reset UI layout")) {
        // Everything Edit UI can change: widget places and sizes, the status bar and quick slot, the inventory size.
        for (auto& w : g_widgets) {
            w.fx = w.defX; w.fy = w.defY; w.fw = w.defW; w.fh = w.defH;
            w.textPct = kTextDefault; w.alpha = 94;
            w.place = true;
            SaveLayout(w);
            SaveLook(w);
        }
        UiLayout_Reset();
#ifdef MAPLEROOT_INV_RESIZE
        InvResize_ResetSize();
#endif
    }
}
