// ============================================================
// chatlog.cpp: chat tabs, colours and blocklists (ticket 123, 1.6.1).
//
// Every chat-log line reaches CUIStatusBar::ChatLogAdd (0x008DB070) with its text and one of 23 types (392 callers go
// through CHATLOG_ADD, 21 call it directly). The detour copies each line into a 500-line history and asks
// chat_policy.h whether it survives the player's filters; a dropped line is passed on as "" so the stock function
// still runs and releases the item reference it owns (an item megaphone's ZRef). So blocking works on the stock chat
// log as well as in the module.
//
// The "Chat" module is an ImGui window like the HP/MP/EXP modules: tabs (All, General, Whisper, Party, Guild, System,
// Megaphone) with an unread count, the stock colours (or the player's per tab), its own text size and opacity, and a
// right-click on a line to block its speaker. Unlike the bars it takes the mouse while frozen (tabs, scrolling); Edit
// UI moves and resizes it. Off by default: the HUD window (Ctrl+F10) switches it on. Settings are per PC in
// HKCU\Software\SlumsMS\Chat. config.ini [clienthax] chattabs=false leaves ChatLogAdd unhooked.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESS (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "constants.h"
#include "attackstop_patch.h"
#include "d3d9host.h"
#include "overlay_policy.h"
#include "chat_policy.h"

#include "imgui.h"

#include <cstring>

namespace {

constexpr const char* kKey = "Software\\SlumsMS\\Chat";
constexpr uintptr_t kAddr_ChatLogAdd = 0x008DB070;   // CUIStatusBar::ChatLogAdd, thiscall, ret 0x18
constexpr unsigned char kPrologue[] = {0xB8, 0x4A, 0x2A, 0xAD, 0x00, 0xE8, 0x1E, 0x5B, 0x18, 0x00, 0x83, 0xEC};
constexpr std::size_t kHistory = 500;

// ZRef<GW_ItemSlotBase> by value: 8 bytes on the stack. The callee releases it, so the original is always called once.
struct ZRefRaw {
    void* pad;
    void* p;
};
using ChatLogAdd_t = void(__fastcall*)(void* self, void* edx, const char* text, long type, long channel, int flag,
    ZRefRaw item);
ChatLogAdd_t g_ChatLogAdd = reinterpret_cast<ChatLogAdd_t>(kAddr_ChatLogAdd);
bool g_bHooked = false;

struct Line {
    DWORD tick = 0;
    int type = 0;
    std::string text;     // UTF-8 for ImGui
    std::string sender;   // as in the line (ANSI), for "block this player"
};
Chat::Ring<Line, kHistory> g_lines;
unsigned g_unread[Chat::kTabs] = {};
std::uint64_t g_seenSerial = 0;

// ---- preferences ----------------------------------------------------------------------------------------------------
bool g_bLoaded = false;
bool g_bOn = false;
bool g_bFilters = true;
bool g_bTimes = false;
int g_nTab = 0;
int g_nText = 100;
int g_nAlpha = 70;
int g_fx = 125, g_fy = 6000, g_fw = 3500, g_fh = 2400;   // fractions x 10000 (overlay_policy.h)
constexpr int kDefX = 125, kDefY = 6000, kDefW = 3500, kDefH = 2400;
std::uint32_t g_colour[Chat::kTabs] = {};   // 0 = the stock colour of each type
Chat::Rules g_rules;
bool g_bPlace = true;
bool g_bLastFrozen = true;
float g_fLastW = 0, g_fLastH = 0;

DWORD ReadDword(const char* name, DWORD fallback) {
    DWORD v = 0, cb = sizeof(v);
    return RegGetValueA(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_DWORD, nullptr, &v, &cb) == ERROR_SUCCESS ? v : fallback;
}

void WriteDword(const char* name, DWORD v) { RegSetKeyValueA(HKEY_CURRENT_USER, kKey, name, REG_DWORD, &v, sizeof(v)); }

std::string ReadString(const char* name) {
    DWORD cb = 0;
    if (RegGetValueA(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_SZ, nullptr, nullptr, &cb) != ERROR_SUCCESS || cb < 2) {
        return "";
    }
    std::string s(cb, '\0');
    if (RegGetValueA(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_SZ, nullptr, &s[0], &cb) != ERROR_SUCCESS) return "";
    s.resize(std::strlen(s.c_str()));
    return s;
}

void WriteString(const char* name, const std::string& s) {
    RegSetKeyValueA(HKEY_CURRENT_USER, kKey, name, REG_SZ, s.c_str(), static_cast<DWORD>(s.size() + 1));
}

int Clamp(int v, int lo, int hi) { return (std::min)((std::max)(v, lo), hi); }

void LoadPrefs() {
    if (g_bLoaded) return;
    g_bLoaded = true;
    g_bOn = ReadDword("on", 0) != 0;
    g_bFilters = ReadDword("filters", 1) != 0;
    g_bTimes = ReadDword("times", 0) != 0;
    g_nTab = Clamp(static_cast<int>(ReadDword("tab", 0)), 0, Chat::kTabs - 1);
    g_nText = Clamp(static_cast<int>(ReadDword("text", 100)), 60, 200);
    g_nAlpha = Clamp(static_cast<int>(ReadDword("alpha", 70)), 0, 100);
    g_fx = static_cast<int>(ReadDword("x", kDefX));
    g_fy = static_cast<int>(ReadDword("y", kDefY));
    g_fw = static_cast<int>(ReadDword("w", kDefW));
    g_fh = static_cast<int>(ReadDword("h", kDefH));
    for (int t = 1; t < Chat::kTabs; ++t) {
        char name[16];
        sprintf_s(name, "colour%d", t);
        g_colour[t] = ReadDword(name, 0);
    }
    g_rules.hideSystemTips = ReadDword("hidetips", 0) != 0;
    g_rules.players = Chat::SplitList(ReadString("players"));
    g_rules.keywords = Chat::SplitList(ReadString("words"));
}

void SaveLayout() {
    WriteDword("x", g_fx);
    WriteDword("y", g_fy);
    WriteDword("w", g_fw);
    WriteDword("h", g_fh);
}

void SaveLists() {
    WriteString("players", Chat::JoinList(g_rules.players));
    WriteString("words", Chat::JoinList(g_rules.keywords));
}

std::string AnsiToUtf8(const char* s) {
    const int wn = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (wn <= 1) return "";
    std::wstring w(static_cast<std::size_t>(wn), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, -1, &w[0], wn);
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return "";
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
    out.resize(static_cast<std::size_t>(n - 1));
    return out;
}

// ---- the hook (game thread, like the overlay's EndScene) ------------------------------------------------------------
void __fastcall ChatLogAdd_hook(void* self, void* edx, const char* text, long type, long channel, int flag,
    ZRefRaw item) {
    bool keep = true;
    if (text && *text) {
        try {
            LoadPrefs();
            const std::string line(text);
            keep = !g_bFilters || Chat::Keep(static_cast<int>(type), line, g_rules);
            if (keep) {
                Line l;
                l.tick = GetTickCount();
                l.type = static_cast<int>(type);
                l.text = AnsiToUtf8(text);
                l.sender = Chat::SenderOf(l.type, line);
                g_lines.Push(std::move(l));
                // Unread counts only while another tab is open: a line already on screen in All is read.
                const int tab = static_cast<int>(Chat::TabOf(static_cast<int>(type)));
                if (g_nTab != 0 && tab != g_nTab) {
                    if (g_unread[tab] < 999) ++g_unread[tab];
                    if (g_unread[0] < 999) ++g_unread[0];
                }
            }
        } catch (...) {
            keep = true;
            LOG_ONCE("Chat: a line could not be filtered; it was shown unchanged");
        }
    }
    g_ChatLogAdd(self, edx, keep ? text : "", type, channel, flag, item);
}

ImU32 ColourOf(int type) {
    const int tab = static_cast<int>(Chat::TabOf(type));
    const std::uint32_t argb = g_colour[tab] ? g_colour[tab] : Chat::StockColour(type);
    return IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 0xFF);
}

void BlockPlayer(const std::string& name) {
    if (Chat::AddUnique(g_rules.players, name)) SaveLists();
}

// A list editor: one text box with Add, then each entry with a remove button.
void EditList(const char* id, const char* hint, std::vector<std::string>& list, char* buf, std::size_t cb) {
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
    const bool enter = ImGui::InputTextWithHint("##add", hint, buf, cb, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Add") || enter) && Chat::AddUnique(list, buf)) {
        buf[0] = '\0';
        SaveLists();
    }
    for (std::size_t i = 0; i < list.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("x")) {
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
            SaveLists();
            ImGui::PopID();
            break;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(list[i].c_str());
        ImGui::PopID();
    }
    ImGui::PopID();
}

}  // namespace

bool ChatModule_On() {
    LoadPrefs();
    return g_bOn && g_bHooked;
}

int ChatModule_TextPct() {
    LoadPrefs();
    return g_nText;
}

void ChatModule_ResetLayout() {
    g_fx = kDefX; g_fy = kDefY; g_fw = kDefW; g_fh = kDefH;
    g_bPlace = true;
    SaveLayout();
}

// From Overlay_DrawHud, in the field.
void ChatModule_Draw(bool frozen, bool snap) {
    if (!ChatModule_On()) return;
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    if (screen.x < 1 || screen.y < 1) return;
    if (screen.x != g_fLastW || screen.y != g_fLastH || frozen != g_bLastFrozen) {
        g_fLastW = screen.x;
        g_fLastH = screen.y;
        g_bLastFrozen = frozen;
        g_bPlace = true;
    }
    const float px = ImGui::GetFontSize();
    const float minW = px * 14, minH = px * 5;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar;
    if (frozen) {
        // Fixed in place, but the tabs and the scroll wheel work.
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
    }
    if (g_bPlace || frozen) {
        const OverlayPolicy::Rect r = OverlayPolicy::FromFractions(g_fx, g_fy, g_fw, g_fh, screen.x, screen.y, minW, minH);
        ImGui::SetNextWindowPos(ImVec2(r.x, r.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(r.w, r.h), ImGuiCond_Always);
        g_bPlace = false;
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(minW, minH), ImVec2(screen.x, screen.y));
    ImGui::SetNextWindowBgAlpha(g_nAlpha / 100.0f);
    if (ImGui::Begin("Chat###chatmodule", nullptr, flags)) {
        ImGui::SetWindowFontScale(g_nText / 100.0f);
        if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            for (int t = 0; t < Chat::kTabs; ++t) {
                char label[48];
                if (g_unread[t] && t != g_nTab) sprintf_s(label, "%s (%u)###tab%d", Chat::kTabNames[t], g_unread[t], t);
                else sprintf_s(label, "%s###tab%d", Chat::kTabNames[t], t);
                if (ImGui::BeginTabItem(label)) {
                    if (g_nTab != t) {
                        g_nTab = t;
                        WriteDword("tab", static_cast<DWORD>(t));
                    }
                    g_unread[t] = 0;
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        static int s_menuLine = -1;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        if (ImGui::BeginChild("lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
            const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f;
            const Chat::Tab tab = static_cast<Chat::Tab>(g_nTab);
            ImGui::PushTextWrapPos(0.0f);
            for (std::size_t i = 0; i < g_lines.Size(); ++i) {
                const Line& l = g_lines[i];
                if (!Chat::InTab(tab, l.type)) continue;
                ImGui::PushStyleColor(ImGuiCol_Text, ColourOf(l.type));
                if (g_bTimes) {
                    const DWORD age = (GetTickCount() - l.tick) / 60000;
                    char stamp[16];
                    if (age < 60) sprintf_s(stamp, "[%2lum] ", static_cast<unsigned long>(age));
                    else sprintf_s(stamp, "[%2luh] ", static_cast<unsigned long>(age / 60));
                    ImGui::TextDisabled("%s", stamp);
                    ImGui::SameLine(0, 0);
                }
                ImGui::TextUnformatted(l.text.c_str());
                ImGui::PopStyleColor();
                if (!l.sender.empty() && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    s_menuLine = static_cast<int>(i);
                    ImGui::OpenPopup("line");
                }
            }
            ImGui::PopTextWrapPos();
            if (g_lines.Serial() != g_seenSerial) {
                g_seenSerial = g_lines.Serial();
                if (atBottom) ImGui::SetScrollHereY(1.0f);   // follow new lines unless the player scrolled up
            }
            if (ImGui::BeginPopup("line")) {
                if (s_menuLine >= 0 && static_cast<std::size_t>(s_menuLine) < g_lines.Size()) {
                    const std::string who = g_lines[static_cast<std::size_t>(s_menuLine)].sender;
                    ImGui::TextDisabled("%s", AnsiToUtf8(who.c_str()).c_str());
                    if (ImGui::MenuItem("Block this player")) BlockPlayer(who);
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    if (!frozen && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        // A drag or resize has finished: store where the player left it.
        const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
        int fx = OverlayPolicy::ToFraction(p.x, screen.x), fy = OverlayPolicy::ToFraction(p.y, screen.y);
        if (snap) {
            fx = OverlayPolicy::Snap(fx);
            fy = OverlayPolicy::Snap(fy);
            if (fx != g_fx || fy != g_fy) g_bPlace = true;
        }
        const int fw = OverlayPolicy::ToFraction(s.x, screen.x), fh = OverlayPolicy::ToFraction(s.y, screen.y);
        if (fx != g_fx || fy != g_fy || fw != g_fw || fh != g_fh) {
            g_fx = fx; g_fy = fy; g_fw = fw; g_fh = fh;
            SaveLayout();
        }
    }
    ImGui::End();
}

// The "Chat" section of the HUD window (Ctrl+F10). Everything applies at once.
void ChatModule_DrawSettings() {
    LoadPrefs();
    if (!ImGui::CollapsingHeader("Chat tabs and filters")) return;
    if (!g_bHooked) {
        ImGui::TextDisabled("Off: config.ini [clienthax] chattabs=false, or this client build is not supported.");
        return;
    }
    if (ImGui::Checkbox("Show the chat module (tabs)", &g_bOn)) {
        WriteDword("on", g_bOn ? 1 : 0);
        g_bPlace = true;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    if (ImGui::SliderInt("Text##chat", &g_nText, 60, 200, "%d%%")) WriteDword("text", static_cast<DWORD>(g_nText));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    if (ImGui::SliderInt("Opacity##chat", &g_nAlpha, 0, 100, "%d%%")) WriteDword("alpha", static_cast<DWORD>(g_nAlpha));
    if (ImGui::Checkbox("Minutes since each line", &g_bTimes)) WriteDword("times", g_bTimes ? 1 : 0);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear history")) {
        g_lines.Clear();
        std::fill(std::begin(g_unread), std::end(g_unread), 0u);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset place")) ChatModule_ResetLayout();

    ImGui::SeparatorText("Filters (the stock chat log too)");
    if (ImGui::Checkbox("Filters on", &g_bFilters)) WriteDword("filters", g_bFilters ? 1 : 0);
    ImGui::SameLine();
    if (ImGui::Checkbox("Hide EXP and tip lines", &g_rules.hideSystemTips)) {
        WriteDword("hidetips", g_rules.hideSystemTips ? 1 : 0);
    }
    ImGui::TextDisabled("Notices and GM chat always show. Right-click a line in the module to block its speaker.");
    static char s_player[32] = {}, s_word[64] = {};
    ImGui::TextUnformatted("Blocked players");
    EditList("players", "character name", g_rules.players, s_player, sizeof(s_player));
    ImGui::TextUnformatted("Blocked words");
    EditList("words", "word or phrase", g_rules.keywords, s_word, sizeof(s_word));

    ImGui::SeparatorText("Colours");
    for (int t = 1; t < Chat::kTabs; ++t) {
        ImGui::PushID(t);
        bool custom = g_colour[t] != 0;
        char name[16];
        sprintf_s(name, "colour%d", t);
        if (ImGui::Checkbox("##custom", &custom)) {
            // First use starts from the tab's main stock colour.
            static constexpr int kMainType[Chat::kTabs] = {0, 0, 1, 2, 4, 7, 13};
            g_colour[t] = custom ? Chat::StockColour(kMainType[t]) : 0;
            WriteDword(name, g_colour[t]);
        }
        ImGui::SameLine();
        if (custom) {
            const std::uint32_t c = g_colour[t];
            float rgb[3] = {((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f};
            if (ImGui::ColorEdit3(Chat::kTabNames[t], rgb, ImGuiColorEditFlags_NoInputs)) {
                g_colour[t] = 0xFF000000u | (static_cast<std::uint32_t>(rgb[0] * 255 + 0.5f) << 16) |
                    (static_cast<std::uint32_t>(rgb[1] * 255 + 0.5f) << 8) | static_cast<std::uint32_t>(rgb[2] * 255 + 0.5f);
                WriteDword(name, g_colour[t]);
            }
        } else {
            ImGui::TextDisabled("%s: stock colours", Chat::kTabNames[t]);
        }
        ImGui::PopID();
    }
}

void AttachChatLogMod() {
    char value[32] = {};
    GetPrivateProfileStringA("clienthax", "chattabs", "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    if (!integration::ParseIniBool(value).value_or(true)) {
        DebugMessage("Chat: off ([clienthax] chattabs=false)");
        return;
    }
    if (std::memcmp(reinterpret_cast<const void*>(kAddr_ChatLogAdd), kPrologue, sizeof(kPrologue)) != 0) {
        ErrorMessage("Chat tabs: unsupported binary anchor 008DB070 (CUIStatusBar::ChatLogAdd); chat left unchanged");
        return;
    }
    g_bHooked = ATTACH_HOOK(g_ChatLogAdd, ChatLogAdd_hook);
    DebugMessage("CHAT_TABS_READY hooked=%d", g_bHooked ? 1 : 0);
}
