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
//   npc_pos <npc id>             -> {"ev":"npc.pos","npc":<id>,"ok":true,"x":..,"y":..} in game-screen pixels (the
//                                   space the worker's screenshots and mouse_click use), or "ok":false with "error":
//                                   npc_not_on_map | out_of_view | field_not_ready. The client never clicks: the
//                                   worker's click_npc clicks the point (P1-Q2, schema agreed with SlumsMS QA).
//   click_option <n>             -> picks row n (1-8) of the open npc menu (a sendSimple) through the dialog's own
//                                   reply path, no mouse: {"ev":"click.done","op":"click_option","ok":true,"value":v}
//                                   or "ok":false with "error": not_a_menu | no_such_option.
// ============================================================

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

// One received packet from its opcode on: CInPacket's buffer starts with the socket header, so the parsers below take
// the bytes from the offset the opcode was peeked at, before the client's handler moved it on.
struct PacketBytes {
    const unsigned char* data = nullptr;
    size_t size = 0;
};
inline PacketBytes PacketFromOpcode(const unsigned char* buffer, size_t length, size_t opcodeAt) {
    PacketBytes b;
    if (!buffer || opcodeAt > length || length - opcodeAt < 2) return b;
    b.data = buffer + opcodeAt;
    b.size = length - opcodeAt;
    return b;
}

constexpr int kMaxMenuOptions = 8;
constexpr int kMaxCharName = 12;      // v83 character names: 4-12 letters and digits
constexpr int kMaxDropCount = 32767;  // ITEM_MOVE carries the count as a short

// The inventory tab an item id lives in: 1 Equip .. 5 Cash, the id's millions digit; 0 for anything else.
inline int InventoryTypeOf(int item) {
    const int type = item / 1000000;
    return (item > 0 && type >= 1 && type <= 5) ? type : 0;
}

inline bool ValidCharName(const std::string& name) {
    if (name.size() < 4 || name.size() > static_cast<size_t>(kMaxCharName)) return false;
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!ok) return false;
    }
    return true;
}

struct Command {
    enum Kind { Invalid, Ping, Burst, NpcPos, ClickOption, CloseDialog, DropItem, PartyCreate, PartyInvite } kind = Invalid;
    int npc = 0;
    int option = 0;   // click_option: 1-8
    int item = 0;     // drop_item
    int count = 0;    // drop_item: 1 unless given
    std::string name; // party_invite
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
    int npc = 0;
    if (std::sscanf(line.c_str(), "npc_pos %d %c", &npc, &extra) == 1) {
        if (npc <= 0) { c.error = "npc id must be positive"; return c; }
        c.kind = Command::NpcPos;
        c.npc = npc;
        return c;
    }
    int option = 0;
    if (std::sscanf(line.c_str(), "click_option %d %c", &option, &extra) == 1) {
        if (option < 1 || option > kMaxMenuOptions) { c.error = "option must be 1-8"; return c; }
        c.kind = Command::ClickOption;
        c.option = option;
        return c;
    }
    int item = 0, count = 0;
    const int dropFields = std::sscanf(line.c_str(), "drop_item %d %d %c", &item, &count, &extra);
    if (dropFields == 1 || dropFields == 2) {
        if (!InventoryTypeOf(item)) { c.error = "item id must be 1000000-5999999"; return c; }
        if (dropFields == 1) count = 1;
        if (count < 1 || count > kMaxDropCount) { c.error = "count must be 1-32767"; return c; }
        c.kind = Command::DropItem;
        c.item = item;
        c.count = count;
        return c;
    }
    if (line == "party_create") { c.kind = Command::PartyCreate; return c; }
    if (line == "close_dialog") { c.kind = Command::CloseDialog; return c; }
    char name[32] = {};
    if (std::sscanf(line.c_str(), "party_invite %31s %c", name, &extra) == 1) {
        if (!ValidCharName(name)) { c.error = "name must be 4-12 letters or digits"; return c; }
        c.kind = Command::PartyInvite;
        c.name = name;
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

// {"ev":"<name>","t":<ms since the client started>[,"utc":"<ISO 8601>"]}<extra> -- extra is ",\"key\":value..." or
// empty. utc is the wall clock the runner's chat.log and manifest are keyed by (QA, 2026-11-06).
inline std::string EventLine(const char* name, uint64_t ms, const std::string& extra = std::string(),
    const std::string& utc = std::string()) {
    std::string line = std::string("{\"ev\":\"") + name + "\",\"t\":" + std::to_string(ms);
    if (!utc.empty()) line += ",\"utc\":\"" + utc + "\"";
    return line + extra + "}\n";
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

// ---- npcs on the field (P1-Q2 click_npc) ------------------------------------------------------------------------
// The module keeps the field's npcs from the server's own spawn packets (PacketCreator.spawnNPC and the controller
// form), so it needs no client address: object id -> template id and the npc's foothold point (x, cy).
constexpr uint16_t kOpSpawnNpc = 0x101;              // int oid, int id, short x, short cy, bool f, short fh, rx0, rx1, 1
constexpr uint16_t kOpRemoveNpc = 0x102;             // int oid
constexpr uint16_t kOpSpawnNpcController = 0x103;    // byte 1 + the spawn body, or byte 0 + int oid (control dropped)

struct NpcUpdate {
    enum Op { None, Add, Remove } op = None;
    int oid = 0;
    int npc = 0;
    int x = 0;
    int y = 0;   // cy: the foothold the npc stands on
};

inline NpcUpdate ParseNpcPacket(uint16_t opcode, const unsigned char* data, size_t size) {
    NpcUpdate u;
    if (!data || size < 2) return u;
    detail::Reader r{data, size, 2};
    int mode = 1, oid = 0, npc = 0, x = 0, cy = 0;
    if (opcode == kOpRemoveNpc) {
        if (!r.U32(oid)) return u;
        u.op = NpcUpdate::Remove;
        u.oid = oid;
        return u;
    }
    if (opcode == kOpSpawnNpcController && !r.U8(mode)) return u;
    if (opcode != kOpSpawnNpc && opcode != kOpSpawnNpcController) return u;
    if (mode == 0) return u;   // the controller dropped: the npc itself stays on the field
    if (!r.U32(oid) || !r.U32(npc) || !r.U16(x) || !r.U16(cy)) return u;
    u.op = NpcUpdate::Add;
    u.oid = oid;
    u.npc = npc;
    u.x = static_cast<int16_t>(x);
    u.y = static_cast<int16_t>(cy);
    return u;
}

constexpr int kNpcClickRise = 30;   // click this far above the npc's feet, inside a standing sprite

// A world point as a game-screen point: IWzGr2D::center is the world point at the middle of the w x h screen.
struct ScreenPoint {
    int x = 0;
    int y = 0;
    bool inView = false;
};

inline ScreenPoint WorldToScreen(int wx, int wy, int camX, int camY, int w, int h) {
    ScreenPoint p;
    p.x = wx - camX + w / 2;
    p.y = wy - camY + h / 2;
    p.inView = p.x >= 0 && p.x < w && p.y >= 0 && p.y < h;
    return p;
}

// The worker clicks in an 800x600 reference space scaled onto the window (client-mcp windows.py move()), so a point on
// a wider game screen (1366x768, resolution.cpp) is scaled to it before it is reported (D1: an NPC at x=721 of 1366 was
// clicked at 1231). In-view is judged on the game screen, before scaling.
constexpr int kRefWidth = 800;
constexpr int kRefHeight = 600;
inline ScreenPoint ToReference(ScreenPoint p, int w, int h) {
    if (w <= 0 || h <= 0) return p;
    p.x = p.x * kRefWidth / w;
    p.y = p.y * kRefHeight / h;
    return p;
}

// The runner's name for a chat line's kind (QA's chat.log groups by it): the packet a line came in and its mode byte.
inline const char* ChatKindName(uint16_t opcode, int kind) {
    switch (opcode) {
    case kOpChatText: return "all";
    case kOpMultiChat:
        switch (kind) {
        case 0: return "buddy";
        case 1: return "party";
        case 2: return "guild";
        case 3: return "alliance";
        default: return "unknown";
        }
    case kOpWhisper: return "whisper";
    case kOpSpouseChat: return "spouse";
    case kOpServerMessage:   // 2 megaphone, 3 super megaphone, 8 item megaphone, 9 multi megaphone; the rest notices
        return (kind == 2 || kind == 3 || kind == 8 || kind == 9) ? "megaphone" : "notice";
    case kOpYellowTip: return "system";
    default: return "unknown";
    }
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

// The npc an NPC_TALK packet is from (short opcode, byte speaker type, int npc id), or -1.
inline int NpcIdForTalkPacket(const unsigned char* data, size_t size) {
    if (!data || size < 7) return -1;
    return static_cast<int>(static_cast<unsigned>(data[3]) | (static_cast<unsigned>(data[4]) << 8) |
                            (static_cast<unsigned>(data[5]) << 16) | (static_cast<unsigned>(data[6]) << 24));
}

// An npc menu (sendSimple, NPC_TALK message type 4) and the #L<value># of each row in the order they are drawn.
// NPC_TALK: short opcode, byte 4, int npc, byte message type, byte speaker, string text (PacketCreator.getNPCTalk).
constexpr int kNpcTalkAskMenu = 4;

struct NpcMenu {
    bool isTalk = false;     // the packet was an npc dialog at all
    bool isMenu = false;     // ... and a menu
    std::vector<int> values; // row n is values[n - 1]
};

inline NpcMenu ParseNpcMenu(const unsigned char* data, size_t size) {
    NpcMenu m;
    if (!data || size < 9) return m;
    m.isTalk = true;
    if (data[7] != kNpcTalkAskMenu) return m;
    detail::Reader r{data, size, 9};
    std::string text;
    if (!r.Str(text)) return m;
    m.isMenu = true;
    for (size_t at = text.find("#L"); at != std::string::npos; at = text.find("#L", at + 2)) {
        size_t end = at + 2;
        int value = 0;
        bool digits = false;
        while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
            value = value * 10 + (text[end] - '0');
            digits = true;
            ++end;
        }
        if (digits && end < text.size() && text[end] == '#') m.values.push_back(value);
    }
    return m;
}

// Where CScriptMan's ask-menu reply reads the dialog (MapleStory.exe 0x747073..0x7470DE): CWnd::DoModal (0x004EDBA1)
// runs CWvsApp::Run on the dialog's m_bTerminate and returns its m_nRet; m_nRet 1 sends NPC_TALK_MORE with type 4,
// action 1 and the int at the selection field, which is the row's #L value.
constexpr uint32_t kDlgOffRet = 0x6C;         // CDialog::m_nRet
constexpr uint32_t kDlgOffTerminate = 0x70;   // CDialog::m_bTerminate
constexpr uint32_t kDlgOffSelection = 0xF4;   // CUtilDlgEx: the picked row's #L value
constexpr int kDlgRetOk = 1;
// Any other return but 3 (closed by the server) makes every npc dialog type reply "end chat": [type][-1] (say 0x0074689D,
// menu 0x00747073, yes/no 6/7 0x007469F7). IDCANCEL is what the dialog's own close button returns.
constexpr int kDlgRetEnd = 2;
// CUtilDlgEx, the npc dialog (built at 0x009A31AD, 0x6E8 bytes). It is a CDialog, which derives from CWnd and not
// CUIWnd (0x004ED9FC calls CWnd's constructor 0x009DE383), so the window walk lets it through by its vtable (D1).
constexpr uint32_t kVtable_CUtilDlgEx = 0x00B3E100;
constexpr uint32_t kAddr_CWndManInstance = 0x00BEC20C;   // TSingleton<CWndMan>::ms_pInstance (DoModal reads it)
constexpr uint32_t kOff_ModalList = 0x6C;                // CWndMan: ZList<IUIMsgHandler*> of open modal dialogs

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
