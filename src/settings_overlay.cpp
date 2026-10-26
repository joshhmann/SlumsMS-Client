// ============================================================
// settings_overlay.cpp: the in-game HUD settings window (1.6.0).
//
// Drawn by d3d9host.cpp while open (Ctrl+F10). HUD only: which widgets show, freeze, size, look and layout reset,
// all applied at once. Game options (movement, lightbulbs, Discord, ...) stay in the launcher, which owns
// config.ini's [clienthax]/[discord] switches and only changes them while the game is closed (owner, 2026-10-25).
// ============================================================

#include "pch.h"
#include "d3d9host.h"

#include "imgui.h"

void SettingsOverlay_Draw(bool* pOpen) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("SlumsMS HUD", pOpen,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        Overlay_DrawSettings();
        ImGui::Separator();
        ImGui::TextDisabled("Game options are in the launcher (Game options).");
        if (ImGui::Button("Close")) *pOpen = false;
        ImGui::SameLine();
        ImGui::TextDisabled("Ctrl+F10 or Esc also closes this window.");
    }
    ImGui::End();
}
