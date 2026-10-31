// Slide menu (slidemenu_patch.h): the closing-'#' rule, and with a MapleStory.exe argument the stock bytes and the
// call path the analysis rests on.
#include "../src/slidemenu_patch.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace SlideMenu;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

int Need(const std::string& s) {
    return ClosingHashesNeeded(s.c_str(), s.size());
}

void CheckRule() {
    // Well formed: passed through unchanged.
    Check(Need("") == 0, "empty text");
    Check(Need("#") == 0, "a lone '#'");
    Check(Need("#0# Ariant Coliseum#") == 0, "closed menu");
    Check(Need("#0# Ariant Coliseum#1# Mu Lung Dojo#") == 0, "closed two-entry menu");
    // What the server sends for the Dimensional Mirror: "#w" + the script's text (PacketCreator.getDimensionalMirror,
    // scripts/npc/9010022.js). The "#w" pairs w/"0", " Ariant Coliseum"/"1", and leaves the last key open.
    Check(Need("#w#0# Ariant Coliseum#1# Mu Lung Dojo") == 2, "9010022 packet text");
    Check(Need("#w#-1# There is no place for you to transport to from here.") == 2, "9010022 packet, below level 20");
    // Without the prefix the last value is open.
    Check(Need("#0# Ariant Coliseum#1# Mu Lung Dojo") == 1, "9010022 menu");
    Check(Need("#1# Mu Lung Dojo#2# Monster Carnival 1") == 1, "9010022 menu, level 30");
    Check(Need("#-1# There is no place for you to transport to from here.") == 1, "9010022 below level 20");
    Check(Need("#0#") == 1, "key closed, value empty and open");
    Check(Need("##") == 1, "empty key, value open");
    // The key itself is open: one '#' closes it, a second the value.
    Check(Need("#0# A#1") == 2, "open key");
    Check(Need("#0") == 2, "open key only");
    // A text not starting with '#' makes the stock loop spin (no read past the end): left alone.
    Check(Need("no menu") == 0, "no leading '#'");
    // With the hashes appended, every case stays in bounds.
    for (const char* s : {"#0# A#1# B", "#0# A#1", "#0", "##", "#0#", "#-1# x", "#w#0# A#1# B"}) {
        const std::string t(s);
        Check(StaysInBounds(t.c_str(), t.size(), Need(t)), "fixed text stays in bounds: " + t);
        Check(Need(t + std::string(Need(t), '#')) == 0, "fixed text needs nothing more: " + t);
    }
    Check(Classify(OriginalBytes().data()) == SiteState::Original, "stock classifies as original");
    unsigned char jmp[kEntryLength] = {0xE9, 0x00, 0x10, 0x00, 0x10, 0xE8, 0xBC, 0x54, 0x2F, 0x00};
    Check(Classify(jmp) == SiteState::Detoured, "a jmp at the entry is someone's detour");
    unsigned char other[kEntryLength] = {0xB8, 0xE0, 0x56, 0xAB, 0x00, 0xE8, 0xBC, 0x54, 0x2F, 0x01};
    Check(Classify(other) == SiteState::Unknown, "anything else is left alone");
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

std::uint32_t CallTarget(std::uint32_t va, const unsigned char* call) {
    return va + 5 + Le32(call + 1);
}

struct Exe {
    std::vector<unsigned char> d;
    explicit Exe(const char* path) {
        std::ifstream f(path, std::ios::binary);
        Check(f.good(), "cannot open the exe");
        d.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    // .text of this client has raw offset == VA - image base.
    const unsigned char* At(std::uint32_t va, std::size_t n) const {
        Check(va >= 0x401000 && va - 0x400000 + n <= d.size(), "address out of range");
        return d.data() + (va - 0x400000);
    }
};

void CheckClient(const char* path) {
    const Exe exe(path);
    Check(std::memcmp(exe.At(kAddr_Parse, kEntryLength), OriginalBytes().data(), kEntryLength) == 0,
          "stock entry bytes");
    Check(CallTarget(kAddr_Parse + 5, exe.At(kAddr_Parse + 5, 5)) == kAddr_EhProlog, "entry calls _EH_prolog");
    const unsigned char* call = exe.At(kAddr_ParseCall, 5);
    Check(call[0] == 0xE8 && CallTarget(kAddr_ParseCall, call) == kAddr_Parse, "the dialog constructor calls the parser");
    // The constructor 0x0076B3F0 is created only by ScriptMessage type 14 (0x00747A46: call at 0x00747A76).
    const unsigned char* ctor = exe.At(0x00747A76, 5);
    Check(ctor[0] == 0xE8 && CallTarget(0x00747A76, ctor) == 0x0076B3F0, "slide menu dialog constructor");
    // The unbounded scans: key 0x0076B79E and value 0x0076B7C9 (cmp word ptr [..], '#', no length check).
    Check(std::memcmp(exe.At(0x0076B79E, 5), "\x66\x83\x3C\x71\x23", 5) == 0, "key scan");
    Check(std::memcmp(exe.At(kAddr_ValueScan, 9), "\x66\x83\x38\x23\x74\x05\x46\x40\x40", 9) == 0, "value scan");
    // Outer loop: i < length - 1 (dec eax; cmp esi, eax; jge).
    Check(std::memcmp(exe.At(0x0076B77F, 3), "\x48\x3B\xF0", 3) == 0, "loop bound");
    // __thiscall with one stack argument (the ZXString<char>, released by the parser at 0x0076B95B): ret 4.
    Check(std::memcmp(exe.At(0x0076B9F9, 3), "\xC2\x04\x00", 3) == 0, "ret 4");
    Check(std::memcmp(exe.At(0x0076B948, 3), "\x8B\x45\x08", 3) == 0, "the parser releases its argument");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CheckRule();
        if (argc > 1) {
            CheckClient(argv[1]);
            std::cout << "slidemenu: rule and client bytes OK\n";
        } else {
            std::cout << "slidemenu: rule OK (no exe given)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "slidemenu_patch_test FAILED: " << e.what() << "\n";
        return 1;
    }
}
