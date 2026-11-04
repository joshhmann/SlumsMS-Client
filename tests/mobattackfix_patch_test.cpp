// b251 mob attack info (mobattackfix_patch.h): the fatal IWzProperty cast site, its patch, the decision the
// cave implements, and the stock-vs-guarded table. With a MapleStory.exe argument it also checks every stock
// byte the cave relies on.
//
// Fail-before: on the baseline (client-164-mapfix 8eee9ac) this header and test do not exist, and the stock
// bytes it pins are the ones that raise the uncaught _com_error -- StockRaises(0x80004003) is asserted true,
// i.e. the stock client ends the session on exactly the HRESULT the guard tolerates.
#include "../src/mobattackfix_patch.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using namespace MobAttackFix;

namespace {

void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

constexpr std::int32_t Hr(std::uint32_t v) { return static_cast<std::int32_t>(v); }

// The site, the patch, and the cave's decision table -- no exe needed.
void CheckPatch() {
    Check(kAddr_Site + kSiteLength == kAddr_StockCompare, "the call is the instruction right before the compare");
    Check(kAddr_StockCompare + kStockBranchLength == kAddr_ToleratedPath, "the raise block ends where the tolerated path starts");

    const auto& orig = OriginalBytes();
    Check(orig[0] == 0xE8, "the stock site is a near call");
    Check(kAddr_Site + 5 + static_cast<std::int32_t>(Le32(&orig[1])) == kAddr_QueryCast, "the stock call targets 004052AD");

    // Patched: still a call of the same length, aimed at the cave.
    for (std::uint32_t cave : {0x10001000u, 0x7C0D2000u, 0x00A00000u}) {
        const auto patched = PatchedBytes(cave);
        Check(patched.size() == kSiteLength, "patch keeps the instruction length");
        Check(patched[0] == 0xE8, "patch is a call");
        Check(kAddr_Site + 5 + Le32(&patched[1]) == cave, "call rel32 reaches the cave");
    }
    Check(Classify(OriginalBytes().data()) == SiteState::Original, "stock classifies as original");
    const auto patched = PatchedBytes(0x10001000);
    Check(Classify(patched.data()) == SiteState::Patched, "a call is either ours or another detour");
    unsigned char other[kSiteLength] = {0x90, 0x90, 0x90, 0x90, 0x90};
    Check(Classify(other) == SiteState::Unknown, "anything else is left alone");
}

// The decision the cave implements (E_POINTER -> 0067FE49) against the stock branch.
void CheckDecision() {
    // The observed b251 HRESULT: stock raises, the guard takes the tolerated path.
    Check(ResumeFor(Hr(kEPointer)) == Resume::ToleratedPath, "E_POINTER takes the tolerated path");
    Check(StockRaises(Hr(kEPointer)), "stock raises on E_POINTER (that is the crash)");

    // E_NOINTERFACE is stock-tolerated already: unchanged, and it never reaches the cave's compare.
    Check(ResumeFor(Hr(kENoInterface)) == Resume::StockCompare, "E_NOINTERFACE stays on the stock path");
    Check(!StockRaises(Hr(kENoInterface)), "stock does not raise on E_NOINTERFACE");

    // Success and every other failure keep byte-identical stock behaviour.
    for (std::uint32_t hr : {0x00000000u, 0x00000001u, 0x00000002u, 0x80004005u, 0x8007000Eu, 0x80030002u,
                             0x8000FFFFu, 0x80010108u, 0x80020009u}) {
        Check(ResumeFor(Hr(hr)) == Resume::StockCompare, "only E_POINTER is tolerated");
    }
    // Nothing the guard suppresses is silently swallowed: every HRESULT it redirects is one stock raises,
    // and every other raising HRESULT still goes through the stock raiser.
    int tolerated = 0;
    for (std::uint32_t hr = 0x80004000u; hr <= 0x80004005u; ++hr) {
        const bool guarded = ResumeFor(Hr(hr)) == Resume::ToleratedPath;
        if (guarded) {
            ++tolerated;
            Check(StockRaises(Hr(hr)), "the guard only suppresses a raise that stock would do");
        }
        if (hr == kEPointer) continue;
        Check(!guarded, "no HRESULT but E_POINTER is redirected");
        Check(StockRaises(Hr(hr)) == (hr == kENoInterface ? false : true), "stock model matches the cmp/je bytes");
    }
    Check(tolerated == 1, "exactly one HRESULT is tolerated");
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

    // The patched call and the two pushes that set up its ECX/argument.
    Check(std::memcmp(exe.At(kAddr_Site, kSiteLength), OriginalBytes().data(), kSiteLength) == 0, "stock site bytes");
    const unsigned char* call = exe.At(kAddr_Site, kSiteLength);
    Check(kAddr_Site + 5 + Le32(call + 1) == kAddr_QueryCast, "site calls the IWzProperty cast");
    Check(std::memcmp(exe.At(kAddr_ContextBefore, kContextBeforeLength), ContextBefore().data(), kContextBeforeLength) == 0,
        "argument pushed and destination _com_ptr_t loaded into ECX");

    // Both continuations the cave jumps to.
    Check(std::memcmp(exe.At(kAddr_StockCompare, kStockBranchLength), StockBranch().data(), kStockBranchLength) == 0,
        "stock compare/raise block");
    const unsigned char* branch = exe.At(kAddr_StockCompare, kStockBranchLength);
    Check(std::memcmp(exe.At(kAddr_ToleratedPath, kToleratedPathLength), ToleratedPath().data(), kToleratedPathLength) == 0,
        "tolerated path bytes");
    Check(kAddr_StockCompare + 4 + static_cast<std::int8_t>(branch[3]) == kAddr_ToleratedPath, "jge goes to the tolerated path");
    Check(kAddr_StockCompare + 9 + 2 + static_cast<std::int8_t>(branch[10]) == kAddr_ToleratedPath, "je goes to the tolerated path");
    Check(kAddr_StockCompare + 12 + 5 + static_cast<std::int32_t>(Le32(branch + 13)) == kAddr_Raise, "the raise call target");
    Check(exe.At(kAddr_ThrowSite, 1)[0] == 0xCC, "the trace's throw address is the int3 after _CxxThrowException");

    // The stock callee's contract: __thiscall, `ret 4`, HRESULT in EAX, callee-saved registers restored.
    Check(exe.At(kAddr_QueryCast, 3)[0] == 0x55 && exe.At(kAddr_QueryCast, 3)[1] == 0x8B && exe.At(kAddr_QueryCast, 3)[2] == 0xEC,
        "cast prologue");
    Check(std::memcmp(exe.At(kAddr_QueryCast_Epilogue, kQueryCastEpilogueLength), QueryCastEpilogue().data(),
                      kQueryCastEpilogueLength) == 0,
        "cast epilogue: EAX = HRESULT, ret 4 (so the cave's `ret 4` keeps the stack identical)");
    Check(exe.At(kAddr_QueryCast + 0x1C, 2)[0] == 0xFF && exe.At(kAddr_QueryCast + 0x1C, 2)[1] == 0x11,
        "the cast's QueryInterface call");

    // ... and what it queries for: IID_IWzProperty {986515D9-0A0B-4929-8B4F-718682177B92}.
    const unsigned char iid[16] = {0xD9, 0x15, 0x65, 0x98, 0x0B, 0x0A, 0x29, 0x49,
                                   0x8B, 0x4F, 0x71, 0x86, 0x82, 0x17, 0x7B, 0x92};
    Check(std::memcmp(exe.At(kAddr_IidWzProperty, 16), iid, 16) == 0, "IID at 00BD8308 is IID_IWzProperty");
    const unsigned char iidPush[5] = {0x68, 0x08, 0x83, 0xBD, 0x00};
    Check(std::memcmp(exe.At(kAddr_QueryCast + 0x16, 5), iidPush, 5) == 0, "the cast pushes &IID_IWzProperty");

    // Context that identifies the caller chain in the report: 0067FD81's action-table index and the animation
    // cache singleton, plus 00529E7B's entry array (count this+0x218, pointer at this+0x234/+0x238).
    const unsigned char actionIndex[7] = {0x8D, 0x04, 0x85, 0xF8, 0xC4, 0xBE, 0x00};
    Check(std::memcmp(exe.At(0x0067FD99, 7), actionIndex, 7) == 0, "attack slot -> action-name table at 00BEC4F8");
    const unsigned char cacheLoad[6] = {0x8B, 0x0D, 0xD4, 0x78, 0xBE, 0x00};
    Check(std::memcmp(exe.At(0x0067FDC8, 6), cacheLoad, 6) == 0, "animation cache singleton 00BE78D4");
    const unsigned char countCompare[6] = {0x3B, 0xBE, 0x18, 0x02, 0x00, 0x00};
    Check(std::memcmp(exe.At(0x00529E92, 6), countCompare, 6) == 0, "00529E7B compares the index against this+0x218");
    const unsigned char entryTest[8] = {0x83, 0xBC, 0xFE, 0x38, 0x02, 0x00, 0x00, 0x00};
    Check(std::memcmp(exe.At(0x00529E9A, 8), entryTest, 8) == 0, "00529E7B tests the entry at this+0x238");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        CheckPatch();
        CheckDecision();
        if (argc > 1) {
            CheckClient(argv[1]);
            std::cout << "mobattackfix: patch, decision and client bytes OK\n";
        } else {
            std::cout << "mobattackfix: patch and decision OK (no exe given)\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "mobattackfix_patch_test FAILED: " << e.what() << "\n";
        return 1;
    }
}


