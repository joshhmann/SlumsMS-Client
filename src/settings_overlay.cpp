// ============================================================
// settings_overlay.cpp: the in-game settings window (1.6.0, ticket 73 on the D3D9 host).
//
// Drawn by d3d9host.cpp while open (Ctrl+F10). The options, their defaults, the legacy moveattack fallback and the
// save-with-rollback are settings_values.h (ticket 73); this file is only the ImGui face of it. Save writes just
// those keys to config.ini; Close drops unsaved changes. Changes apply after a restart, as before.
// ============================================================

#include "pch.h"
#include "constants.h"
#include "d3d9host.h"
#include "settings_values.h"

#include "imgui.h"

namespace {

constexpr const char* kIniPath = ".\\" CONSTANTS_CONFIG_NAME;
#ifdef MAPLEROOT_WEATHER
constexpr std::size_t kCount = 9;   // the weather sway options exist only in a weather build
#else
constexpr std::size_t kCount = 7;
#endif

std::optional<std::string> ReadValue(const char* section, const char* key) {
    char value[128] = {};
    GetPrivateProfileStringA(section, key, "", value, sizeof(value), kIniPath);
    return value[0] ? std::optional<std::string>(value) : std::nullopt;
}

bool WriteValue(const char* section, const char* key, const char* value) {
    return WritePrivateProfileStringA(section, key, value, kIniPath) != FALSE;
}

}  // namespace

void SettingsOverlay_Draw(bool* pOpen) {
    static bool s_bLoaded = false;
    static slums_settings::Values s_values{};
    static std::string s_status;
    if (!s_bLoaded) {
        s_values = slums_settings::load(ReadValue);
        s_status.clear();
        s_bLoaded = true;
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("SlumsMS settings", pOpen,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("Changes apply after restarting the game.");
        ImGui::TextDisabled("Resolution and sound are in the game's Options.");
        ImGui::Separator();
        for (std::size_t i = 0; i < kCount; ++i) {
            ImGui::Checkbox(slums_settings::preferences[i].label, &s_values[i]);
        }
        ImGui::Separator();
        if (ImGui::Button("Save")) {
            s_status = slums_settings::save(s_values, ReadValue, WriteValue, kCount)
                ? "Saved. Restart the game to apply."
                : "Could not write config.ini; nothing was changed.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) *pOpen = false;
        ImGui::TextDisabled("Ctrl+F10 or Esc closes this window.");
        if (!s_status.empty()) ImGui::TextUnformatted(s_status.c_str());
    }
    ImGui::End();
    if (!*pOpen) s_bLoaded = false;   // reopening shows what is saved, not unsaved edits
}
