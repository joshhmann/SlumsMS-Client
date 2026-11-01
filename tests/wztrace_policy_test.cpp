// WZ trace rules (wztrace_policy.h): what a lookup line, a throw line and the ring of the last lookups say, and where
// the log stops. Pure logic, no Windows -- the Windows side (wztrace.cpp) only supplies the stamp, the thread id, the
// HRESULT and the path. Fail-before: none of these helpers existed before ticket b251.
#include "../src/wztrace_policy.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace WzTrace;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

const QaPolicy::UtcTime kStamp = QaPolicy::UtcTime{2026, 11, 1, 15, 4, 5, 7};   // "2026-11-01T15:04:05.007Z"

bool ValidUtf8(const std::string& s) {
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t n = 0;
        if (c < 0x80) n = 1;
        else if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        else return false;
        if (i + n > s.size()) return false;
        for (std::size_t k = 1; k < n; ++k) {
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
        }
        i += n;
    }
    return true;
}

std::size_t CountNewlines(const std::string& s) {
    std::size_t n = 0;
    for (char c : s) {
        if (c == '\n') ++n;
    }
    return n;
}

void TestLevel() {
    Check(ClampLevel(-5) == 0, "a negative switch is off");
    Check(ClampLevel(0) == 0, "0 is off");
    Check(ClampLevel(1) == 1, "1 stays 1");
    Check(ClampLevel(2) == 2, "2 stays 2");
    Check(ClampLevel(3) == kMaxLevel && ClampLevel(999) == kMaxLevel, "a larger switch clamps to 2");
}

void TestHresultFilter() {
    Check(IsHResult(0x80030002), "0x80030002 (WZ item not found) is an HRESULT");
    Check(IsHResult(0x80030001), "0x80030001 (WZ namespace) is an HRESULT");
    Check(!IsHResult(0x80004005), "0x80004005 (E_FAIL) is outside the rule of crash_diagnostics.cpp:37");
    Check(IsHResult(0x80040001) && IsHResult(0x8007000E), "the 0x8004 and 0x8007 facilities are in the rule");
    Check(IsHResult(0x20000000) && IsHResult(0x2200000E), "the 0x2xxxxxxx range, both ends");
    Check(!IsHResult(0x2200000F), "just past the 0x2xxxxxxx range");
    Check(!IsHResult(0x80010108), "an RPC HRESULT is not one of the client's throw values");
    Check(!IsHResult(0x00000000), "S_OK is not printed as a throw's HRESULT");
    Check(!IsHResult(0x00B41250), "a thrown object's first dword can be a pointer: never an HRESULT");
    Check(!IsHResult(0xDEADBEEF) && !IsHResult(0x6C616E67), "neither a magic value nor ASCII is an HRESULT");
    Check(Succeeded(0) && Succeeded(1) && Succeeded(0x0000FFFF), "non-negative HRESULTs succeeded");
    Check(!Succeeded(0x80030002) && !Succeeded(0x80004005), "0x8xxxxxxx failed");
}

// A path is one log field: no byte may split the line, and a long path is cut on a character boundary.
void TestLogPath() {
    Check(LogPath("Map/Tile/grassySoil/0") == "Map/Tile/grassySoil/0", "a plain path is unchanged");
    Check(LogPath("a\nb\rc\td").find('\n') == std::string::npos, "a newline in a path cannot start a line");
    Check(LogPath("a\nb\rc\td") == "a b c d", "control bytes become spaces");
    Check(LogPath(std::string("a\0b", 3)).size() == 3, "an embedded NUL keeps its place");

    std::string euro;
    for (int i = 0; i < 200; ++i) euro += "\xE2\x82\xAC";
    Check(euro.size() == 600, "three-byte characters");
    const std::string cut = LogPath(euro);
    Check(cut.size() <= kMaxPathBytes, "a long path is capped");
    Check(cut.size() == 255, "a 3-byte character at the cap backs off to 255");
    Check(ValidUtf8(cut), "the cut path is still UTF-8");

    std::string emoji;
    for (int i = 0; i < 100; ++i) emoji += "\xF0\x9F\x98\x80";
    const std::string emojiCut = LogPath(emoji);
    Check(emojiCut.size() == 256 && ValidUtf8(emojiCut), "a 4-byte character that ends at the cap is kept whole");
}

void TestLookupLine() {
    const std::string line = LookupLine(kStamp, 4321, 0x80030002, "Map/Tile/grassySoil/0");
    Check(line == "2026-11-01T15:04:05.007Z thread=4321 lookup hr=80030002 Map/Tile/grassySoil/0\n",
        "a failed lookup line: " + line);
    Check(CountNewlines(line) == 1, "one lookup, one line");
    const std::string injected = LookupLine(kStamp, 1, 0x80004005, "a\nb");
    Check(CountNewlines(injected) == 1, "a path with a newline still makes one line");
    Check(injected.find("lookup hr=80004005 a b\n") != std::string::npos, "the hr field precedes the path");
}

void TestMakeTileLine() {
    const std::string line = MakeTileLine(kStamp, 4321, 0x00A12345, 3, 7, 0x12345678);
    Check(line == "2026-11-01T15:04:05.007Z thread=4321 makeTile this=00A12345 a0=00000003 a1=00000007 a2=12345678\n",
        "a makeTile line: " + line);
    Check(CountNewlines(line) == 1, "one tile, one line");
}

void TestThrowLine() {
    const std::string withHr =
        ThrowLine(kStamp, 4321, 0x004287DD, "MapleStory.exe", 0x000287DD, true, 0x80030002);
    Check(withHr == "2026-11-01T15:04:05.007Z thread=4321 throw address=004287DD module=MapleStory.exe "
        "offset=000287DD hr=80030002\n", "a throw line with an HRESULT: " + withHr);
    const std::string noHr = ThrowLine(kStamp, 4321, 0x004287DD, "MapleStory.exe", 0x000287DD, false, 0);
    Check(noHr.find(" hr=none\n") != std::string::npos, "no HRESULT is said, not guessed");
    const std::string unknown = ThrowLine(kStamp, 1, 0x12345678, "", 0x12345678, false, 0);
    Check(unknown.find("module=unknown") != std::string::npos, "an address outside a module says unknown");
    std::string hugeModule(300, 'x');
    const std::string capped = ThrowLine(kStamp, 1, 0x12345678, hugeModule, 0, false, 0);
    Check(capped.size() < 400 && CountNewlines(capped) == 1, "a long module name cannot grow the line");
    Check(CountNewlines(withHr) == 1, "one throw, one line");
}

void TestRingLinesAndBurst() {
    const std::string begin = RingBeginLine(kStamp, 128);
    Check(begin == "2026-11-01T15:04:05.007Z ring_begin count=128 (the last WZ lookups, oldest first)\n",
        "the ring's opening line: " + begin);
    Check(RingEntryLine(0, 0x80030002, "a/b") == "ring[0] hr=80030002 a/b\n", "a ring entry line");
    Check(RingEntryLine(127, 0, "x").rfind("ring[127] hr=00000000 ", 0) == 0, "the last entry is ring[count-1]");
    Check(RingEndLine(kStamp) == "2026-11-01T15:04:05.007Z ring_end\n", "the ring's closing line");

    Check(ShouldDumpRing(false, 0, 5), "the first throw always dumps the ring");
    Check(!ShouldDumpRing(true, 1000, 2999), "a second throw inside 2 s does not");
    Check(ShouldDumpRing(true, 1000, 3000), "2 s later it does again");
    Check(ShouldDumpRing(true, 5000, 4000) == false, "a clock that went backwards does not dump twice");
}

void TestRing() {
    LookupRing ring;
    Check(ring.Count() == 0, "a new ring is empty");
    Check(ring.Snapshot().empty(), "a new ring has no entries");
    char buffer[64] = {};
    Check(ring.AppendDump(buffer, sizeof buffer) == 0 && buffer[0] == '\0', "an empty ring dumps nothing");

    ring.Push(0x80030002, "one");
    ring.Push(0x00000000, "two");
    ring.Push(0x80004005, "three");
    Check(ring.Count() == 3, "three lookups");
    {
        const std::vector<RingEntry> snap = ring.Snapshot();
        Check(snap.size() == 3, "three entries");
        Check(snap[0].path == "one" && snap[0].hr == 0x80030002, "oldest first");
        Check(snap[2].path == "three" && snap[2].hr == 0x80004005, "newest last");
    }

    // Wrap: 200 pushes leave the last 128, oldest of those first, newest last.
    for (int i = 0; i < 200; ++i) ring.Push(static_cast<std::uint32_t>(i), "p" + std::to_string(i));
    Check(ring.Count() == kRingSize, "the ring keeps exactly 128 entries");
    {
        const std::vector<RingEntry> snap = ring.Snapshot();
        Check(snap.size() == kRingSize, "128 entries after the wrap");
        Check(snap[0].path == "p72", "the oldest kept entry is 200-128: " + snap[0].path);
        Check(snap[0].hr == 72, "with its own HRESULT");
        Check(snap[127].path == "p199", "the newest entry is the last push: " + snap[127].path);
        for (std::size_t i = 1; i < snap.size(); ++i) {
            Check(snap[i].hr == snap[i - 1].hr + 1, "the ring is in push order");
        }
    }

    // The dump writes the same order and never past the end of the buffer.
    {
        std::vector<char> big(kRingSize * (32 + kMaxPathBytes + 2) + 16, '\x7F');
        const std::size_t used = ring.AppendDump(big.data(), kRingSize * (32 + kMaxPathBytes + 2));
        Check(used > 0 && big[used] == '\0', "the dump is NUL terminated");
        const std::string text(big.data(), used);
        Check(text.rfind("ring[0] hr=00000048 p72\n", 0) == 0, "the dump starts with the oldest entry: " + text.substr(0, 40));
        Check(text.find("ring[127] hr=000000C7 p199\n") != std::string::npos, "and ends with the newest");
        for (std::size_t i = kRingSize * (32 + kMaxPathBytes + 2); i < big.size(); ++i) {
            Check(big[i] == '\x7F', "the dump stays inside its capacity");
        }
        Check(CountNewlines(text) == kRingSize, "one line per entry");
    }
    {
        char small[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
        const std::size_t used = ring.AppendDump(small, sizeof small);
        Check(used <= sizeof small - 1, "a small buffer gets no partial line");
        Check(used == 0 && small[0] == '\0' && small[7] == 'x', "an entry longer than the buffer writes nothing");
        char none[4] = {};
        Check(ring.AppendDump(nullptr, 16) == 0 && ring.AppendDump(none, 0) == 0, "no buffer or no room writes nothing");
    }

    ring.Clear();
    Check(ring.Count() == 0, "clear empties the ring");

    // A cleared ring starts over at the front, not at a stale index.
    ring.Push(0x80030001, "after");
    Check(ring.Snapshot().size() == 1 && ring.Snapshot()[0].path == "after", "the ring restarts cleanly");
}

void TestCap() {
    Check(UnderCap(0, 100), "an empty log has room");
    Check(UnderCap(kMaxLogBytes - 10, 10), "a line that ends exactly at the cap fits");
    Check(!UnderCap(kMaxLogBytes - 10, 11), "a line that would cross the cap does not");
    Check(!UnderCap(kMaxLogBytes, 1), "a full log takes nothing more");
    Check(!UnderCap(kMaxLogBytes + 500, 1), "a log already past the cap takes nothing more");
    Check(kMaxLogBytes == 2u * 1024u * 1024u, "the cap is 2 MB");
    const std::string cap = CapLine(kStamp);
    Check(cap.find("cap") != std::string::npos && CountNewlines(cap) == 1, "the cap notice says why the log stops: " + cap);
}

void TestSessionLine() {
    const std::string one = SessionLine(kStamp, 1);
    Check(one.rfind("2026-11-01T15:04:05.007Z wztrace level=1 ring=128 max=2097152\n", 0) == 0,
        "the session line names the switch and the cap: " + one);
    Check(SessionLine(kStamp, 2).find("level=2") != std::string::npos, "level 2 says 2");
}
}  // namespace

int main() {
    try {
        TestLevel();
        TestHresultFilter();
        TestLogPath();
        TestLookupLine();
        TestMakeTileLine();
        TestThrowLine();
        TestRingLinesAndBurst();
        TestRing();
        TestCap();
        TestSessionLine();
    } catch (const std::exception& e) {
        std::cerr << "wztrace_policy: " << e.what() << '\n';
        return 1;
    }
    std::cout << "wztrace_policy: ok\n";
    return 0;
}
