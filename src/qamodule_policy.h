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
