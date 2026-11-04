// MakeObj missing-art guard (mapobj_guard.h): the decision table, the log wording, and the stock
// sites the guard relies on. With a MapleStory.exe argument it also checks those bytes in that client.
//
// Fail-before: mapobj_guard.h, Classify/Describe and the guard in lamps.cpp did not exist, so this
// test (and the build of it) failed. After: the known missing-art HRESULTs are skipped, every other
// HRESULT still propagates, and the call site still ignores MakeObj's return value -- which is what
// makes returning nullptr safe.
#include "../src/mapobj_guard.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace MapObjGuard;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

std::uint32_t Read32(const std::vector<unsigned char>& d, std::size_t at) {
    Check(at + 4 <= d.size(), "pe read out of range");
    return Le32(d.data() + at);
}

std::size_t FileOffset(const std::vector<unsigned char>& d, std::uint32_t va) {
    const std::uint32_t pe = Read32(d, 0x3C);
    Check(Read32(d, pe) == 0x00004550, "not a PE file");
    const unsigned sections = d[pe + 6] | (d[pe + 7] << 8);
    const unsigned optSize = d[pe + 20] | (d[pe + 21] << 8);
    const std::uint32_t imageBase = Read32(d, pe + 24 + 28);
    const std::uint32_t rva = va - imageBase;
    for (unsigned i = 0; i < sections; ++i) {
        const std::size_t sh = pe + 24 + optSize + i * 40;
        const std::uint32_t vsize = Read32(d, sh + 8), vaddr = Read32(d, sh + 12);
        const std::uint32_t rawSize = Read32(d, sh + 16), rawPtr = Read32(d, sh + 20);
        if (rva >= vaddr && rva < vaddr + (vsize > rawSize ? vsize : rawSize)) return rawPtr + (rva - vaddr);
    }
    throw std::runtime_error("address outside every section");
}

std::wstring Wide(const char* text) {
    std::wstring out;
    for (; *text; ++text) out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*text)));
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    try {
        // The guard's whole decision: a missing object costs the object, nothing else.
        Check(Classify(kE_POINTER) == Action::Skip, "E_POINTER (missing art) is skipped");
        Check(Classify(kStgEFileNotFound) == Action::Skip, "STG_E_FILENOTFOUND is skipped");
        Check(Classify(kStgEPathNotFound) == Action::Skip, "STG_E_PATHNOTFOUND is skipped");
        Check(Classify(kE_FAIL) == Action::Skip, "E_FAIL is skipped");
        Check(Classify(static_cast<long>(0x8007000Eu)) == Action::Propagate, "E_OUTOFMEMORY propagates");
        Check(Classify(static_cast<long>(0x80004005u) + 1) == Action::Propagate, "an unknown HRESULT propagates");
        Check(Classify(0) == Action::Propagate, "S_OK in a throw path propagates");

        // The log names the art path the map asked for, which is the actionable part of it.
        Check(Describe(L"acc10", L"timeTemple", L"pillar_old", L"7") ==
                  L"Obj/acc10.img/timeTemple/pillar_old/7", "the 272000000 path is described exactly");
        Check(Describe(L"acc10", L"timeTemple", L"pillar_old", L"") ==
                  L"Obj/acc10.img/timeTemple/pillar_old", "a missing l2 is left out, not printed as empty");
        Check(Describe(L"", L"", L"", L"") == L"Obj/?.img", "an unreadable entry still logs something");
        Check(std::string(ReasonLabel(kE_POINTER)).find("E_POINTER") != std::string::npos,
              "the skip reason names the HRESULT");

        // The stock chain the guard stands on: RestoreObj calls MakeObj and ignores its result.
        Check(kAddr_MakeObjCall + sizeof(kMakeObjCallBytes) == kAddr_MakeObjReturn,
              "the 'return value ignored' instruction follows the MakeObj call");
        const long callTarget = static_cast<long>(kAddr_MakeObjCall) + 5 +
                                static_cast<std::int32_t>(Le32(kMakeObjCallBytes + 1));
        Check(static_cast<std::uint32_t>(callTarget) == kAddr_MakeObj, "0x0063ACB5 calls CMapLoadable::MakeObj");
        Check(kAddr_MakeObj - kAddr_RestoreObj < 0x400, "RestoreObj and MakeObj are neighbours");

        if (argc > 1) {
            std::ifstream f(argv[1], std::ios::binary);
            Check(f.good(), "cannot open client exe");
            std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const std::size_t call = FileOffset(d, kAddr_MakeObjCall);
            Check(std::memcmp(d.data() + call, kMakeObjCallBytes, sizeof(kMakeObjCallBytes)) == 0,
                  "client exe holds the MakeObj call");
            const std::size_t after = FileOffset(d, kAddr_MakeObjReturn);
            Check(std::memcmp(d.data() + after, kMakeObjReturnBytes, sizeof(kMakeObjReturnBytes)) == 0,
                  "client exe ignores MakeObj's return value");
            const std::size_t prologue = FileOffset(d, kAddr_MakeObj);
            Check(std::memcmp(d.data() + prologue, kMakeObjPrologue, sizeof(kMakeObjPrologue)) == 0,
                  "client exe holds the MakeObj prologue the hook detours");
            std::cout << "mapobj_guard: client exe holds the stock call, return-ignored and prologue bytes\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "mapobj_guard: " << e.what() << '\n';
        return 1;
    }
    std::cout << "mapobj_guard: ok\n";
    return 0;
}
