#pragma once
// ============================================================
// chat_policy.h -- chat tabs, filters and blocklists (ticket 123).
//
// No Windows, no client: tests/chat_policy_test.cpp checks it. Every chat line reaches CUIStatusBar::ChatLogAdd
// (0x008DB070, thiscall, ret 0x18) with one of 23 types; chatlog.cpp's detour asks Keep() and drops a line by calling
// the original with "" (so the item argument it owns is still released), and copies every kept line into a Ring for
// the ImGui chat module. Type meanings come from their callers (some inferred, see docs/research-123-124.md).
// ============================================================

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Chat {

constexpr int kTypes = 23;   // CUIStatusBar's chat fonts, one per type (0x8D01B2)

enum class Tab { All, General, Whisper, Party, Guild, System, Megaphone };
constexpr int kTabs = 7;
constexpr const char* kTabNames[kTabs] = {"All", "General", "Whisper", "Party", "Guild", "System", "Megaphone"};

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

// The stock text colour of each type (ARGB, from CUIStatusBar::OnCreate). Type 22's slot is unverified.
constexpr std::uint32_t kStockColour[kTypes] = {
    0xFFFFFFFF, 0xFF00FF00, 0xFFFF99CC, 0xFFFF9900, 0xFFE1ACFE, 0xFFA6FF7F, 0xFFFF28A7, 0xFFBBBBBB,
    0xFFFFFF00, 0xFFFFF080, 0xFF60CEFF, 0xFFFFFFFF, 0xFFFFAFAF, 0xFFCAE7FF, 0xFFFFBFDD, 0xFFFFC600,
    0xFF6C4CE3, 0xFFFC8BE5, 0xFF99CC33, 0xFFFFFFFF, 0xFF99CC33, 0xFFBBBBBB, 0xFFFFFF00,
};
// Types 11, 13-15, 18 and 20 are dark text on a coloured band in the stock log; on the module's dark background
// they use the band colour instead (the table above), so every line stays readable.

inline std::uint32_t StockColour(int type) { return type >= 0 && type < kTypes ? kStockColour[type] : 0xFFFFFFFF; }

namespace detail {
// Where the speaker's name ends and the separator starts, or npos. Outgoing whispers ("To<< text") have no speaker.
inline std::string::size_type NameEnd(int type, const std::string& line, std::string::size_type* sepLen) {
    *sepLen = 0;
    std::string::size_type p = std::string::npos;
    if (type == 1) {
        p = line.find(">>");
        *sepLen = 2;
    } else if (type == 0 || type == 11 || (type >= 2 && type <= 6)) {
        p = line.find(" : ");
        *sepLen = 3;
    }
    return p;
}
}  // namespace detail

// "Name : text" (map, GM, party, buddy, guild, alliance, spouse) -> "Name"; incoming whispers "Name>> text" and
// "Name[Ch 2]>> text" -> "Name"; outgoing whispers, system lines and anything unparsed -> "".
inline std::string SenderOf(int type, const std::string& line) {
    std::string::size_type sep = 0;
    const auto p = detail::NameEnd(type, line, &sep);
    if (p == std::string::npos || p == 0) return "";
    std::string name = line.substr(0, p);
    if (auto b = name.find('['); b != std::string::npos) name = name.substr(0, b);
    if (name.find(' ') != std::string::npos) return "";   // names have no spaces: this was not a "Name : " prefix
    return name;
}

// The words after the speaker (what a keyword is matched against), or the whole line when there is no speaker.
inline std::string MessageOf(int type, const std::string& line) {
    if (SenderOf(type, line).empty()) return line;
    std::string::size_type sep = 0;
    const auto p = detail::NameEnd(type, line, &sep);
    return line.substr(p + sep);
}

inline std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

struct Rules {
    std::vector<std::string> keywords;    // case-insensitive substrings of the message
    std::vector<std::string> players;     // exact names, case-insensitive
    bool hideSystemTips = false;          // grey EXP/effect lines (7) and the unnamed tip types (9, 21)
};

// Whether a line survives the filters. Notices (10) and GM chat (11) always show; a player rule only matches a line
// it could parse a speaker from, so your own outgoing whispers are never dropped by it.
inline bool Keep(int type, const std::string& line, const Rules& r) {
    if (type == 10 || type == 11) return true;
    if (r.hideSystemTips && (type == 7 || type == 9 || type == 21)) return false;
    const std::string who = Lower(SenderOf(type, line));
    if (!who.empty()) {
        for (const auto& p : r.players) {
            if (who == Lower(Trim(p))) return false;
        }
    }
    const std::string msg = Lower(MessageOf(type, line));
    for (const auto& k : r.keywords) {
        const std::string key = Lower(Trim(k));
        if (!key.empty() && msg.find(key) != std::string::npos) return false;
    }
    return true;
}

// Adds a trimmed entry unless an equal one (ignoring case) is there; false when nothing was added.
inline bool AddUnique(std::vector<std::string>& list, const std::string& entry) {
    const std::string e = Trim(entry);
    if (e.empty()) return false;
    for (const auto& x : list) {
        if (Lower(x) == Lower(e)) return false;
    }
    list.push_back(e);
    return true;
}

// Lists are stored as one string, entries separated by '\n' (HKCU\Software\SlumsMS\Chat, REG_SZ).
inline std::vector<std::string> SplitList(const std::string& s) {
    std::vector<std::string> out;
    std::string::size_type a = 0;
    while (a <= s.size()) {
        const auto b = s.find('\n', a);
        const std::string e = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
        AddUnique(out, e);
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

inline std::string JoinList(const std::vector<std::string>& list) {
    std::string s;
    for (const auto& e : list) {
        if (!s.empty()) s += '\n';
        s += e;
    }
    return s;
}

// A fixed-size history: the newest N lines, oldest first by index. The stock log keeps 64; tabs keep more.
template <class T, std::size_t N>
class Ring {
public:
    void Push(T v) {
        items_[(start_ + count_) % N] = std::move(v);
        if (count_ < N) ++count_;
        else start_ = (start_ + 1) % N;
        ++serial_;
    }
    std::size_t Size() const { return count_; }
    const T& operator[](std::size_t i) const { return items_[(start_ + i) % N]; }
    std::uint64_t Serial() const { return serial_; }   // grows on every push: "has anything new arrived"
    void Clear() { start_ = count_ = 0; ++serial_; }

private:
    T items_[N] = {};
    std::size_t start_ = 0, count_ = 0;
    std::uint64_t serial_ = 0;
};

}  // namespace Chat
