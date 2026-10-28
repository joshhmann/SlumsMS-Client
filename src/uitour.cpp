// ============================================================
// uitour.cpp: the first-run Edit UI tour (1.6.1).
//
// Many players never found Edit UI. The first time a character is in the field on this PC, a small card says how to
// open it (Ctrl+F11), what moves, that changes save themselves, and how to reset. "Show me" turns Edit UI on and
// pulses every grip's outline with its name (uilayout.cpp asks UiTour_Pulse). While Edit UI is on, a strip at the top
// says how to finish and has a "?" that brings the card back; the HUD window (Ctrl+F10) has a replay button too.
//
// Never in the way: the card is not modal, takes no keyboard focus and only the clicks that land on it. Esc, the close
// box, "Got it" and "Show me" all mark it seen (uitour_policy.h).
// ============================================================

#include "pch.h"
#include "debug.h"
#include "d3d9host.h"
#include "uitour_policy.h"

#include "imgui.h"

namespace {

constexpr const char* kKey = "Software\\SlumsMS\\Overlay";
constexpr const char* kSeenValue = "tourSeen";
constexpr uintptr_t kAddr_UserLocal = 0x00BEBF98;   // CUserLocal*: set only while a character is in the field

bool g_bEnabled = true;
bool g_bLoaded = false;
DWORD g_dwSeen = 0;
bool g_bCard = false;
DWORD g_dwFieldSince = 0;   // tick of the first field frame this session
DWORD g_dwPulseSince = 0;   // "Show me" start; 0 when not pulsing

void Load() {
    if (g_bLoaded) return;
    g_bLoaded = true;
    DWORD v = 0, cb = sizeof(v);
    if (RegGetValueA(HKEY_CURRENT_USER, kKey, kSeenValue, RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS) g_dwSeen = v;
}

void MarkSeen() {
    g_bCard = false;
    if (g_dwSeen >= UiTour::kVersion) return;
    g_dwSeen = UiTour::kVersion;
    RegSetKeyValueA(HKEY_CURRENT_USER, kKey, kSeenValue, REG_DWORD, &g_dwSeen, sizeof(g_dwSeen));
    DebugMessage("UI tour: seen (version %lu)", g_dwSeen);
}

bool InField() { return *reinterpret_cast<void* const*>(kAddr_UserLocal) != nullptr; }

void Line(const char* text) {
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextWrapped("%s", text);
}

void DrawCard() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float em = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.48f),
        ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(em * 26.0f, 0.0f), ImGuiCond_Always);
    bool open = true;
    if (ImGui::Begin("Make the screen yours!##uitour", &open,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_AlwaysAutoResize)) {
        Line("Press Ctrl+F11 to turn Edit UI on. Drag anything with a blue outline: status bar, quick slots, "
             "chat box, buttons, HP, MP and EXP.");
        Line("HUD windows resize from their corner. Drag the inventory's edge or corner to resize it any time.");
        Line("Changes save by themselves. Press Ctrl+F11 again to lock everything in place.");
        Line("Moved something too far? Ctrl+F10, then Reset UI layout.");
        ImGui::Spacing();
        if (ImGui::Button("Show me")) {
            MarkSeen();
            if (Overlay_IsFrozen()) Overlay_ToggleFreeze();
            g_dwPulseSince = GetTickCount() | 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Got it")) MarkSeen();
    }
    ImGui::End();
    if (!open) MarkSeen();
}

// While Edit UI is on: how to finish, and the "?" that replays the card.
void DrawStrip() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 4.0f), ImGuiCond_Always,
        ImVec2(0.5f, 0.0f));
    if (ImGui::Begin("##uitourstrip", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Edit UI: drag anything outlined. Ctrl+F11 when you're done.");
        ImGui::SameLine();
        if (ImGui::SmallButton("?")) g_bCard = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How Edit UI works");
    }
    ImGui::End();
}

}  // namespace

void UiTour_Setup(bool enabled) {
    g_bEnabled = enabled;
    if (!enabled) DebugMessage("UI tour: off ([clienthax] uitour=false)");
}

bool UiTour_Wants() {
    if (!g_bEnabled) return false;
    Load();
    if (g_bCard || g_dwPulseSince) return true;
    return g_dwSeen < UiTour::kVersion && InField();   // frames before the card, to time its delay
}

// From Overlay_DrawHud, in the field only.
void UiTour_Draw() {
    if (!g_bEnabled) return;
    Load();
    const DWORD now = GetTickCount();
    if (!g_dwFieldSince) g_dwFieldSince = now | 1;
    if (!g_bCard && UiTour::ShouldOffer(g_bEnabled, g_dwSeen, true, now - g_dwFieldSince)) {
        g_bCard = true;
        DebugMessage("UI tour: first run, showing the card");
    }
    if (g_dwPulseSince && now - g_dwPulseSince >= UiTour::kPulseMs) g_dwPulseSince = 0;
    if (!Overlay_IsFrozen()) DrawStrip();
    if (g_bCard) DrawCard();
}

float UiTour_Pulse() {
    return g_dwPulseSince ? UiTour::Pulse(GetTickCount() - g_dwPulseSince) : 0.0f;
}

void UiTour_Replay() {
    if (g_bEnabled) g_bCard = true;
}

bool UiTour_HandleEscape() {
    if (!g_bCard) return false;
    MarkSeen();
    return true;
}
