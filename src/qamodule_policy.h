#pragma once
// ============================================================
// qamodule_policy.h -- the rules of the QA client's runner channel (ticket 166), header-only so
// tests/qamodule_policy_test.cpp checks them without the client. qamodule.cpp does the Windows side.
//
// The QA client (SLUMS_QA_MODULE builds only, never shipped to players) talks to the auto-QA runner over two
// one-way named pipes, so a blocking read never holds up a write:
//   \\.\pipe\slums-qa-<pid>-events    client -> runner, one JSON object per line
//   \\.\pipe\slums-qa-<pid>-commands  runner -> client, one command per line
// Ready signals replace the runner's fixed timings (the world-select "empty channel list" on 2026-10-30 was a click
// landing before the list was processed): a signal fires once the client has HANDLED the server packet that makes the
// screen ready and drawn kSettleFrames more frames after it. channels.ready answers the world click: no
// channels.ready after a click on a world means the click was lost (click again), not that the server is slow.
// Commands:
//   ping                         -> {"ev":"pong"}
//   burst <fps> <ms> <label>     -> back-buffer captures at <fps> for <ms>, to qa-captures\<label>\NNNN.bmp beside
//                                   the game, plus frames.tsv (index, ms since the burst began); {"ev":"burst.done"}
// ============================================================

#include <cstdint>
#include <cstdio>
#include <string>

namespace QaPolicy {

// Server -> client opcodes (v83, the server's SendOpcode) after which a screen is ready.
constexpr uint16_t kOpRecommendedWorldMessage = 0x1B;   // last packet of the world list (list, end, last world, this)
constexpr uint16_t kOpServerStatus = 0x03;              // the reply to clicking a world: its channel panel can draw
constexpr uint16_t kOpCharacterList = 0x0B;             // the character list for the chosen channel
constexpr uint16_t kOpSetField = 0x7D;                  // entering a map (login, warp, channel change)

constexpr int kSettleFrames = 2;   // frames drawn after the packet before the signal fires

// The signal a handled packet arms, or nullptr.
inline const char* SignalForOpcode(uint16_t opcode) {
    switch (opcode) {
    case kOpRecommendedWorldMessage: return "worldselect.ready";
    case kOpServerStatus: return "channels.ready";
    case kOpCharacterList: return "charselect.ready";
    case kOpSetField: return "field.ready";
    default: return nullptr;
    }
}

constexpr int kMinFps = 1, kMaxFps = 30;
constexpr int kMinBurstMs = 100, kMaxBurstMs = 5000;
constexpr size_t kMaxLabel = 40;

// A capture label becomes a folder name: letters, digits, '-' and '_' only, 1-40 long.
inline bool ValidLabel(const std::string& label) {
    if (label.empty() || label.size() > kMaxLabel) return false;
    for (char c : label) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

struct Command {
    enum Kind { Invalid, Ping, Burst } kind = Invalid;
    int fps = 0;
    int ms = 0;
    std::string label;
    std::string error;   // why it is Invalid
};

// One command line (no trailing newline). Unknown or out-of-range commands come back Invalid with a reason.
inline Command ParseCommand(const std::string& line) {
    Command c;
    if (line == "ping") { c.kind = Command::Ping; return c; }
    char label[64] = {};
    int fps = 0, ms = 0;
    char extra = 0;
    if (std::sscanf(line.c_str(), "burst %d %d %63s %c", &fps, &ms, label, &extra) == 3) {
        if (fps < kMinFps || fps > kMaxFps) { c.error = "fps must be 1-30"; return c; }
        if (ms < kMinBurstMs || ms > kMaxBurstMs) { c.error = "ms must be 100-5000"; return c; }
        if (!ValidLabel(label)) { c.error = "label must be 1-40 of A-Z a-z 0-9 - _"; return c; }
        c.kind = Command::Burst;
        c.fps = fps;
        c.ms = ms;
        c.label = label;
        return c;
    }
    c.error = "unknown command";
    return c;
}

// JSON string escaping for the few free-text fields (error reasons, labels are already safe).
inline std::string JsonEscape(const std::string& s) {
    std::string out;
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') { out += '\\'; out += ch; }
        else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
        else out += ch;
    }
    return out;
}

// {"ev":"<name>","t":<ms since the client started>}<extra> -- extra is ",\"key\":value..." or empty.
inline std::string EventLine(const char* name, uint64_t ms, const std::string& extra = std::string()) {
    return std::string("{\"ev\":\"") + name + "\",\"t\":" + std::to_string(ms) + extra + "}\n";
}

// ---- chat lines (ticket 170) ------------------------------------------------------------------------------------
// chat.received is decoded from the server packets that put a line in the player's chat log, not hooked out of the
// chat log itself: the log append sits inside the field's packet handler with no address we could pin (three
// searches, none conclusive), and these layouts are the server's own (PacketCreator), so both sides agree. The
// opcodes below are the client's chat sources: map chat, buddy/party/guild chat, whispers, spouse chat, server
// notices and megaphones, the yellow tip a GM command replies with, and the chat bubble text.
//   kind is the packet's own type/mode byte, so the runner can tell a whisper from a guild line without parsing text:
//   0xA2 the gm byte (0 a normal line, 1 an admin line)  0x86 mode (0 buddy, 1 party, 2 guild, 3 alliance)
//   0x87 the whisper flag (0x12 received)        0x88 mode (4 engaged, 5 spouse)
//   0x44 the notice type                         0x4D the tip type (0xFF from yellowMessage)
constexpr uint16_t kOpServerMessage = 0x44;
constexpr uint16_t kOpYellowTip = 0x4D;    // SET_WEEK_EVENT_MESSAGE: "Spawned 1 BISHOP TrainingBot ... cids [123]"
constexpr uint16_t kOpMultiChat = 0x86;
constexpr uint16_t kOpWhisper = 0x87;
constexpr uint16_t kOpSpouseChat = 0x88;
constexpr uint16_t kOpChatText = 0xA2;

constexpr size_t kMaxChatText = 200;       // bytes of chat text an event carries

struct ChatLine {
    bool ok = false;
    int kind = -1;
    std::string text;                      // what the client shows, cut to kMaxChatText (JsonEscape'd by the caller)
};

// s cut to at most max bytes without splitting a UTF-8 sequence.
inline std::string TruncateUtf8(const std::string& s, size_t max) {
    if (s.size() <= max) return s;
    size_t n = max;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;   // back off continuation bytes
    return s.substr(0, n);
}

namespace detail {
struct Reader {
    const unsigned char* p;
    size_t n;
    size_t at;
    bool U8(int& v) {
        if (at + 1 > n) return false;
        v = p[at++];
        return true;
    }
    bool U16(int& v) {
        if (at + 2 > n) return false;
        v = p[at] | (p[at + 1] << 8);
        at += 2;
        return true;
    }
    bool U32(int& v) {
        if (at + 4 > n) return false;
        v = static_cast<int>(static_cast<unsigned>(p[at]) | (static_cast<unsigned>(p[at + 1]) << 8) |
                             (static_cast<unsigned>(p[at + 2]) << 16) | (static_cast<unsigned>(p[at + 3]) << 24));
        at += 4;
        return true;
    }
    bool Str(std::string& s) {
        int len = 0;
        if (!U16(len) || len < 0 || at + static_cast<size_t>(len) > n) return false;
        s.assign(reinterpret_cast<const char*>(p + at), static_cast<size_t>(len));
        at += static_cast<size_t>(len);
        return true;
    }
};
}  // namespace detail

// One server packet (starting at its 2-byte opcode) as a chat line; ok=false when the opcode is not a chat source
// or the payload does not decode.
inline ChatLine ParseChatPacket(uint16_t opcode, const unsigned char* data, size_t size) {
    ChatLine line;
    if (!data || size < 2) return line;
    detail::Reader r{data, size, 2};
    int v = 0;
    switch (opcode) {
    case kOpChatText: {                       // int cid, byte gm, string text, byte show
        int cid = 0, gm = 0;
        std::string text;
        if (!r.U32(cid) || !r.U8(gm) || !r.Str(text)) return line;
        line.kind = gm;                       // 0 a normal line, 1 an admin (yellow) line
        line.text = text;
        break;
    }
    case kOpMultiChat: {                      // byte mode, string name, string text
        std::string name, text;
        if (!r.U8(v) || !r.Str(name) || !r.Str(text)) return line;
        line.kind = v;
        line.text = name + ": " + text;
        break;
    }
    case kOpWhisper: {                        // byte flag; 0x12 received: string from, byte channel, byte admin, text
        if (!r.U8(v) || v != 0x12) return line;
        std::string from, text;
        int channel = 0, admin = 0;
        if (!r.Str(from) || !r.U8(channel) || !r.U8(admin) || !r.Str(text)) return line;
        line.kind = v;
        line.text = from + ": " + text;
        break;
    }
    case kOpSpouseChat: {                     // byte mode (5 spouse: string from), byte, string text
        int mode = 0, flag = 0;
        std::string from, text;
        if (!r.U8(mode) || (mode != 4 && mode != 5)) return line;
        if (mode == 5 && !r.Str(from)) return line;
        if (!r.U8(flag) || !r.Str(text)) return line;
        line.kind = mode;
        line.text = from.empty() ? text : from + ": " + text;
        break;
    }
    case kOpServerMessage: {                  // byte type, [byte 1 ticker], string text, then the type's tail
        int type = 0;
        if (!r.U8(type)) return line;
        // The scrolling ticker (type 4 with the flag) carries one byte before the text; a plain type-4 notice does
        // not. The server's own decoder tells them apart by the whole packet being 6 bytes longer than the text.
        if (type == 4 && size >= 6 && size == 6 + static_cast<size_t>(data[4] | (data[5] << 8))) ++r.at;
        std::string text;
        if (!r.Str(text)) return line;
        line.kind = type;
        line.text = text;
        break;
    }
    case kOpYellowTip: {                      // byte 0xFF, string tip, short 0
        std::string text;
        if (!r.U8(v) || !r.Str(text)) return line;
        line.kind = v;
        line.text = text;
        break;
    }
    default: return line;
    }
    line.ok = true;
    line.text = TruncateUtf8(line.text, kMaxChatText);
    return line;
}

// ---- windows (ticket 170) ---------------------------------------------------------------------------------------
// The runner needs to know which window is up, by name, before it clicks. A window is named by the class that made
// it, never by where it sits: the client's own window list (CWndMan::ms_lpWindow) is walked once a frame, and a
// UIWindow.img window carries the UOL it was created from (CUIWnd::m_sBackgrndUOL: "UI/UIWindow.img/Item"), whose
// leaf is the name. The two screens that are not UIWindow windows came from their own fills: the cash shop draws
// from UI/CashShop.img and the character-create screen from UI/Login.img/NewChar, so a window made from either is
// that screen.
//
// Dialogs are the exception the UOL cannot name (UtilDlgEx is one class for a party invite, a proposal, an npc
// dialog and a notice), so qamodule.cpp queues the name of the dialog a packet asks for and the next window that
// has no UOL of its own takes it. That pairing is order-based, not class-based: it holds because the client builds
// the dialog inside the packet handler, on the same thread, before the next frame.
constexpr uint16_t kOpPartyOperation = 0x3E;   // code 4 = party invite
constexpr uint16_t kOpMarriageRequest = 0x48;  // the proposal prompt
constexpr uint16_t kOpNpcTalk = 0x130;         // the npc dialog
constexpr uint16_t kOpNoticeType = 1;          // SERVERMESSAGE type 1 = the notice popup

inline std::string LowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline bool StartsWith(const std::string& s, const char* prefix) {
    const size_t n = std::char_traits<char>::length(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

// The runner's name for a window created from this UOL, or nullptr. The two screens that live outside
// UIWindow.img are matched by prefix; everything else by the folders in the path, deepest match first, because the
// string CUIWnd keeps is the window's *background* canvas (UI/UIWindow.img/Item/backgrnd) rather than the window.
inline const char* WindowNameFromUol(const std::string& uol) {
    if (uol.empty()) return nullptr;
    const std::string low = LowerAscii(uol);
    if (StartsWith(low, "ui/cashshop.img")) return "cash_shop";
    if (StartsWith(low, "ui/login.img/newchar")) return "character_create";
    struct Entry {
        const char* part;
        const char* name;
    };
    static const Entry kTable[] = {
        {"item", "item"},
        {"equip", "equip"},
        {"skill", "skill"},
        {"monsterbook", "monster_book"},
        {"worldmap", "world_map"},
        {"family", "family"},
        {"familytree", "family_tree"},
        {"maker", "maker"},
        {"book", "book"},
        {"shop", "shop"},
        {"personalshop", "personal_shop"},
        {"tradingroom", "trade"},
        {"entrustedshop", "storage"},
        {"trunk", "storage"},
        {"inventory", "item"},
    };
    const char* name = nullptr;
    size_t start = 0;
    while (start < low.size()) {
        size_t slash = low.find('/', start);
        if (slash == std::string::npos) slash = low.size();
        std::string part = low.substr(start, slash - start);
        if (part.size() > 4 && part.compare(part.size() - 4, 4, ".img") == 0) part.erase(part.size() - 4);
        for (const Entry& e : kTable) {
            if (part == e.part) name = e.name;   // keep walking: the deepest match wins
        }
        start = slash + 1;
    }
    return name;
}

inline const char* DialogForPacket(uint16_t opcode, const unsigned char* data, size_t size) {
    if (!data || size < 3) return nullptr;
    switch (opcode) {
    case kOpPartyOperation: return data[2] == 4 ? "party_invite" : nullptr;
    case kOpMarriageRequest: return "proposal";
    case kOpNpcTalk: return "npc_dialog";
    case kOpServerMessage: return data[2] == kOpNoticeType ? "notice" : nullptr;
    default: return nullptr;
    }
}

// Classes named by vtable, for windows the UOL table cannot name. Only what the repo has proved; anything else is
// "unknown" with its vtable in the event so a run can name it.
inline const char* WindowNameForVtable(uint32_t vtable) {
    switch (vtable) {
    case 0x00B39FE0: return "monster_book";   // CUIMonsterBook primary vtable
    default: return nullptr;
    }
}

// A bottom-up 32-bit BMP header (BITMAPFILEHEADER + BITMAPINFOHEADER, 54 bytes) for a w x h BGRA frame.
inline void BmpHeader(uint8_t out[54], int w, int h) {
    const uint32_t pixels = static_cast<uint32_t>(w) * static_cast<uint32_t>(h) * 4u;
    const uint32_t size = 54u + pixels;
    auto put16 = [&](int at, uint16_t v) { out[at] = v & 0xFF; out[at + 1] = v >> 8; };
    auto put32 = [&](int at, uint32_t v) { for (int i = 0; i < 4; ++i) out[at + i] = (v >> (8 * i)) & 0xFF; };
    for (int i = 0; i < 54; ++i) out[i] = 0;
    out[0] = 'B'; out[1] = 'M';
    put32(2, size);
    put32(10, 54);
    put32(14, 40);
    put32(18, static_cast<uint32_t>(w));
    put32(22, static_cast<uint32_t>(h));   // positive height: rows bottom-up
    put16(26, 1);
    put16(28, 32);
    put32(34, pixels);
}

}  // namespace QaPolicy
