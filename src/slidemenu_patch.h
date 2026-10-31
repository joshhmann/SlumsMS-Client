#pragma once
// ============================================================
// slidemenu_patch.h -- the Dimensional Mirror's slide menu no longer reads past its text and closes the client (b105).
//
// Header-only so tests/slidemenu_patch_test.cpp can check the rule and the bytes without the client; slidemenu.cpp
// installs it.
//
// THE CRASH. ScriptMessage (packet 0x130, CScriptMan::OnPacket 0x007465F4 -> 0x0074660A) type 14 is the slide menu
// (0x00746752 -> 0x00747A46): a 0xCC-byte dialog whose constructor 0x0076B3F0 hands the packet's text, a
// ZXString<char> by value, to the menu parser 0x0076B6D2 (call at 0x0076B60B; the only caller). The parser widens the
// text and walks it as "#key#value#key#value...#": at a '#' it copies the key up to the next '#' (0x0076B79E), then
// scans the value up to the next '#' (0x0076B7C9: cmp word ptr [eax], 0x23; inc; jmp) -- neither scan checks the
// length. The server's menu text never ends with a closing '#': PacketCreator.getDimensionalMirror sends "#w" + the
// script's text (scripts/npc/9010022.js, Dimensional Mirror: "#0# Ariant Coliseum#1# Mu Lung Dojo"; "#-1# There is
// no place..." below level 20). The "#w" shifts every pair by one, so the walk ends in an open KEY scan (an unprefixed
// text would end in an open value scan); either way the last scan runs past the terminator through the heap until
// some word happens to be 0x0023. Usually it finds one; when the string sits near the end of a heap page it reads
// into the next, unmapped one: C0000005 at 0x0076B7C9 reading DD80B000 (stack 0x0076B610 <- 0x00747A7B <- 0x0074675A
// <- 0x00746607 <- 0x00496664, seen clicking the Dimensional Mirror 9010022 in Vanishing Journey 450001000).
//
// THE FIX. The parser is detoured: when the stock walk of the text would run off its end, one or two '#' are appended
// (a missing value terminator needs one, a missing key terminator two) and the stock parser runs on that copy; a
// well-formed text is passed through unchanged, so every entry, key and value is what the stock client shows. The
// entry bytes are checked first; on a mismatch nothing is hooked.
//
// Not changed: a text whose first character is not '#' makes the stock loop spin without advancing (a hang, not a
// read past the end); no server script sends one.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace SlideMenu {

constexpr std::uint32_t kAddr_Parse        = 0x0076B6D2;   // __thiscall(this, ZXString<char> text), ret 4
constexpr std::uint32_t kAddr_ParseCall    = 0x0076B60B;   // the dialog constructor 0x0076B3F0: call 0x0076B6D2
constexpr std::uint32_t kAddr_ValueScan    = 0x0076B7C9;   // cmp word ptr [eax], 0x23 (the faulting read)
constexpr std::uint32_t kAddr_EhProlog     = 0x00A60B98;   // the client's _EH_prolog
constexpr std::size_t kEntryLength = 10;
constexpr int kMaxAppended = 2;

inline const std::array<unsigned char, kEntryLength>& OriginalBytes() {
    static const std::array<unsigned char, kEntryLength> bytes = {{
        0xB8, 0xE0, 0x56, 0xAB, 0x00,       // mov eax, 0x00AB56E0   (the frame's EH table)
        0xE8, 0xBC, 0x54, 0x2F, 0x00,       // call 0x00A60B98      (_EH_prolog)
    }};
    return bytes;
}

enum class SiteState { Original, Detoured, Unknown };

inline SiteState Classify(const unsigned char* current) {
    if (std::memcmp(current, OriginalBytes().data(), kEntryLength) == 0) return SiteState::Original;
    if (current[0] == 0xE9) return SiteState::Detoured;
    return SiteState::Unknown;
}

// The stock parser's walk (0x0076B76F..0x0076B8F0) over text[0..n) followed by `extra` '#': true when every scan stops
// inside the string. Index n is the terminator (read, not '#'); a scan that passes it reads past the end. The outer
// loop runs while i < length - 1 (signed, so an empty text does nothing); after an entry i stays on the '#' that ended
// its value, which starts the next entry. A non-'#' at i never advances (the stock hang noted above): no read past the
// end, so it counts as in bounds.
inline bool StaysInBounds(const char* text, std::size_t n, int extra) {
    const long total = static_cast<long>(n) + extra;
    auto at = [&](long i) { return i < static_cast<long>(n) ? text[i] : (i < total ? '#' : '\0'); };
    long i = 0;
    while (i < total - 1) {
        if (at(i) != '#') return true;
        for (++i; at(i) != '#'; ++i) {   // key
            if (i >= total) return false;
        }
        for (++i; at(i) != '#'; ++i) {   // value
            if (i >= total) return false;
        }
    }
    return true;
}

// How many '#' to append so the stock parser stays inside the text: 0 for a well-formed text, else 1 or 2.
inline int ClosingHashesNeeded(const char* text, std::size_t n) {
    for (int extra = 0; extra <= kMaxAppended; ++extra) {
        if (StaysInBounds(text, n, extra)) return extra;
    }
    return kMaxAppended;   // unreachable: only the last entry can run off; one '#' ends its value, two its key
}

}  // namespace SlideMenu
