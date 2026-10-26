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

namespace {

constexpr const char* kKey = "Software\\SlumsMS\\Overlay";
constexpr float kAtlasPx = 32.0f;   // fonts are baked once at this size and scaled down
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
};

// The HUD's widgets. The pilot is HP/MP/EXP (ticket 91); buff timers, the damage/EXP meter and the bot panel
// join this list.
Widget g_widgets[] = {
    {"vitals", "HP / MP / EXP", &HudVitals_Draw, 3, 100, 1800, 3000, 1300, true, 0, 0, 0, 0, false, true},
};

bool g_bLoaded = false;
bool g_bFrozen = true;
int g_nTheme = 0;      // 0 SlumsMS, 1 plain ImGui
int g_nScale = 100;    // percent
float g_fLastW = 0, g_fLastH = 0;

void LoadPrefs() {
    if (g_bLoaded) return;
    g_bLoaded = true;
    g_bFrozen = true;   // Edit UI is a temporary mode: every session starts with the layout fixed
    g_nTheme = static_cast<int>(ReadPref("theme", 0));
    g_nScale = static_cast<int>(ReadPref("scale", 100));
    for (auto& w : g_widgets) {
        char name[64];
        sprintf_s(name, "%s_on", w.id);  w.on = ReadPref(name, 1) != 0;
        sprintf_s(name, "%s_x", w.id);   w.fx = static_cast<int>(ReadPref(name, w.defX));
        sprintf_s(name, "%s_y", w.id);   w.fy = static_cast<int>(ReadPref(name, w.defY));
        sprintf_s(name, "%s_w", w.id);   w.fw = static_cast<int>(ReadPref(name, w.defW));
        sprintf_s(name, "%s_h", w.id);   w.fh = static_cast<int>(ReadPref(name, w.defH));
        w.loaded = true;
    }
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
    const ImVec4 cream(0.97f, 0.95f, 0.87f, 0.94f), creamDark(0.90f, 0.87f, 0.77f, 1.00f);
    const ImVec4 blue(0.20f, 0.42f, 0.66f, 1.00f), blueLight(0.33f, 0.56f, 0.80f, 1.00f), blueDark(0.13f, 0.29f, 0.49f, 1.00f);
    c[ImGuiCol_Text] = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    c[ImGuiCol_WindowBg] = cream;
    c[ImGuiCol_PopupBg] = cream;
    c[ImGuiCol_Border] = blueDark;
    c[ImGuiCol_FrameBg] = creamDark;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.86f, 0.90f, 0.96f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.85f, 0.95f, 1.00f);
    c[ImGuiCol_TitleBg] = blue;
    c[ImGuiCol_TitleBgActive] = blueLight;
    c[ImGuiCol_TitleBgCollapsed] = blueDark;
    c[ImGuiCol_Button] = blue;
    c[ImGuiCol_ButtonHovered] = blueLight;
    c[ImGuiCol_ButtonActive] = blueDark;
    c[ImGuiCol_CheckMark] = blueDark;
    c[ImGuiCol_SliderGrab] = blue;
    c[ImGuiCol_SliderGrabActive] = blueDark;
    c[ImGuiCol_Header] = blueLight;
    c[ImGuiCol_HeaderHovered] = blueLight;
    c[ImGuiCol_Separator] = blueDark;
    c[ImGuiCol_ResizeGrip] = blueLight;
    c[ImGuiCol_ResizeGripHovered] = blue;
    c[ImGuiCol_ResizeGripActive] = blueDark;
}

void ApplyScale(float screenH) {
    ImGui::GetIO().FontGlobalScale = OverlayPolicy::FontPixels(screenH, g_nScale) / kAtlasPx;
}

}  // namespace

void Overlay_Setup() {
    LoadPrefs();
    ImGuiIO& io = ImGui::GetIO();
    char path[MAX_PATH];
    const UINT n = GetWindowsDirectoryA(path, MAX_PATH);
    if (n && n < MAX_PATH - 24) strcat_s(path, "\\Fonts\\tahoma.ttf");
    if (n && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        io.Fonts->AddFontFromFileTTF(path, kAtlasPx);
    } else {
        ImFontConfig cfg;
        cfg.SizePixels = kAtlasPx;
        io.Fonts->AddFontDefault(&cfg);
        DebugMessage("Overlay: tahoma.ttf not found, using ImGui's default font");
    }
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
    const float minW = 120.0f * ImGui::GetIO().FontGlobalScale * kAtlasPx / 15.0f;
    const float minH = 40.0f * ImGui::GetIO().FontGlobalScale * kAtlasPx / 15.0f;

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
        const float fontPx = ImGui::GetIO().FontGlobalScale * kAtlasPx;
        const float rowH = fontPx + st.FramePadding.y * 2;
        const float h = w.rows * rowH + (w.rows - 1) * st.ItemSpacing.y + st.WindowPadding.y * 2 +
            (g_bFrozen ? 0.0f : fontPx + st.FramePadding.y * 2);
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
        if (ImGui::Begin(title, nullptr, flags)) w.draw();
        if (!g_bFrozen && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // A drag or resize has finished: store where the player left it.
            const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
            const int fx = OverlayPolicy::ToFraction(p.x, screen.x), fy = OverlayPolicy::ToFraction(p.y, screen.y);
            const int fw = OverlayPolicy::ToFraction(s.x, screen.x), fh = OverlayPolicy::ToFraction(s.y, screen.y);
            if (fx != w.fx || fy != w.fy || fw != w.fw || fh != w.fh) {
                w.fx = fx; w.fy = fy; w.fw = fw; w.fh = fh;
                SaveLayout(w);
            }
        }
        ImGui::End();
    }
    if (!g_bFrozen) UiLayout_DrawGrips();   // the stock bar and quick slot move in the same edit mode
}

// The HUD window's controls: they apply at once, no restart.
void Overlay_DrawSettings() {
    LoadPrefs();
    for (auto& w : g_widgets) {
        char label[96];
        sprintf_s(label, "Show %s", w.title);
        if (ImGui::Checkbox(label, &w.on)) {
            char name[64];
            sprintf_s(name, "%s_on", w.id);
            WritePref(name, w.on ? 1 : 0);
            w.place = true;
        }
    }
    bool editing = !g_bFrozen;
    if (ImGui::Checkbox("Edit UI (Ctrl+F11): move and resize widgets, move the status bar and quick slots", &editing)) {
        Overlay_ToggleFreeze();
    }
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
            w.place = true;
            SaveLayout(w);
        }
        UiLayout_Reset();
#ifdef MAPLEROOT_INV_RESIZE
        InvResize_ResetSize();
#endif
    }
}
