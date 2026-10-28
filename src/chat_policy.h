#pragma once
// ============================================================
// chat_policy.h -- chat tabs, filters and blocklists (ticket 123). SCAFFOLD.
//
// No Windows, no client: tests/chat_policy_test.cpp checks it. Every chat line reaches CUIStatusBar::ChatLogAdd
// (0x008DB070, thiscall, ret 0x18) with one of 23 types; a detour asks Keep() and drops a line by calling the
// original with "" (so the item argument it owns is still released). Tabs and colours live in an ImGui chat module
// that carries over to the 1.7.0 chat. Type meanings come from their callers (some inferred, see the research note
// docs/research-123-124.md).
// ============================================================

#include <cctype>
#include <string>
#include <vector>

namespace Chat {

enum class Tab { All, General, Whisper, Party, Guild, System, Megaphone };

inline Tab TabOf(int type) {
    switch (type) {
    case 0: case 11: return Tab::General;        // map chat, GM chat
    case 1: return Tab::Whisper;
    case 2: case 3: case 6: return Tab::Party;   // party, buddy, spouse
    case 4: case 5: return Tab::Guild;           // guild, alliance
    case 13: case 14: case 18: return Tab::Megaphone;
    default: return Tab::System;                 // EXP, drops, notices, results
    }
}

inline bool InTab(Tab tab, int type) { return tab == Tab::All || TabOf(type) == tab; }

// "Name : text" (map chat) -> "Name"; whispers "Name>> text" / "Name<< text" -> "Name"; else "".
inline std::string SenderOf(int type, const std::string& line) {
    std::string::size_type p = std::string::npos;
    if (type == 0 || type == 11) p = line.find(" : ");
    else if (type == 1) { p = line.find(">>"); if (p == std::string::npos) p = line.find("<<"); }
    else if (type >= 2 && type <= 6) p = line.find(" : ");
    if (p == std::string::npos) return "";
    std::string name = line.substr(0, p);
    if (auto b = name.find('['); b != std::string::npos) name = name.substr(0, b);   // "Name[Ch 2]>> ..."
    return name;
}

struct Rules {
    std::vector<std::string> keywords;    // case-insensitive substrings
    std::vector<std::string> players;     // exact names, case-insensitive
    bool hideSystemTips = false;          // grey EXP/effect lines (7) and the unnamed tip types
};

inline std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Whether a line survives the filters. Never hides whispers to me from people not on the list, or GM/notice lines.
inline bool Keep(int type, const std::string& line, const Rules& r) {
    if (type == 10 || type == 11) return true;                     // notices and GM chat always show
    if (r.hideSystemTips && (type == 7 || type == 9 || type == 21)) return false;
    const std::string low = Lower(line);
    const std::string who = Lower(SenderOf(type, line));
    for (const auto& p : r.players) {
        if (!who.empty() && who == Lower(p)) return false;
    }
    for (const auto& k : r.keywords) {
        if (!k.empty() && low.find(Lower(k)) != std::string::npos) return false;
    }
    return true;
}

}  // namespace Chat
