#pragma once
// ============================================================
// hpmp_patch.h -- 4-byte HP / MaxHP / MP / MaxMP, up to 99,999 (1.6.0).
//
// Header-only so tests/hpmp_patch_test.cpp can check every site against the exe without the client; hpmp.cpp
// installs it. Based on the community "4-byte HP/MP" mod (docs/PROVENANCE.md), re-derived site by site against our
// exe: its list mislabels ~15 sites (they truncate job, LUK, INT, AP or equip stats, not HP/MP) and misses ~20.
//
// STORAGE. GW_CharacterStat keeps HP +0x61, MaxHP +0x69, MP +0x71, MaxMP +0x79 as ZtlSecure<short> (4 data bytes,
// checksum at +4). Every write to those slots (_ZtlSecureTear<short>, 0x004E80EB, fastcall ecx = value, edx = slot,
// returns the checksum the caller stores at +4) is redirected to FakeTear, which stores the raw 32-bit value and
// returns checksum 0. _ZtlSecureFuse<short> (0x004746DD) is detoured: checksum 0 returns the raw int; any other
// slot goes to the original and comes back sign-extended (stock leaves junk in eax's upper half). Real checksums
// are never 0 in practice, and an all-zero slot would make the stock Fuse throw anyway.
// MaxHP / MaxMP copies in the calculated stat (sub_77EC9F) and CWvsContext +0x211C / +0x2128 are already
// ZtlSecure<long>, so they need no change beyond the 30,000 caps.
//
// WIRE (agreed with the server lead, 2026-10-25). Nine Decode2 calls become Decode4: GW_CharacterStat::Decode
// (4), CStage::OnSetField's plain map change (HP), DecodeChangeStat mask 0x400/0x800/0x1000/0x2000 (4). Not
// widened: party result 37 and CUserRemote::OnAttack 0xBB (those shorts are positions, not HP/MP), and the
// outgoing CP 0x59 recovery and GW_CharacterStat::Encode (sub_4E2829), which stay 2 bytes.
//
// WIDEN. Code that reads HP/MP back through Fuse and truncates it to 16 bits: movsx r32, r16 / word [ebp+d8]
// become mov r32, r32 / dword [ebp+d8] (+nop; every slot was stored as a dword), and 66-prefixed test/cmp lose the
// prefix (+nop). Found by tracing every Fuse of the four slots (69 reads) forward to its 16-bit uses.
//
// Every site's bytes are checked before anything is patched; on any mismatch the client refuses to start, since a
// half-widened client would misread every stat packet from a 4-byte server.
//
// WARNING: CLIENT-BUILD-SPECIFIC ADDRESSES (MapleStory.exe sha256 86e4e6be...ac90, image base 0x400000).
// ============================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace HpMp {

constexpr std::uint32_t kAddr_Decode2   = 0x0042470C;   // CInPacket::Decode2
constexpr std::uint32_t kAddr_Decode4   = 0x00406629;   // CInPacket::Decode4
constexpr std::uint32_t kAddr_TearShort = 0x004E80EB;   // _ZtlSecureTear<short>
constexpr std::uint32_t kAddr_FuseShort = 0x004746DD;   // _ZtlSecureFuse<short>
constexpr int kMaxValue = 99999;
constexpr int kStockCap = 30000;

constexpr std::size_t kFuseEntryLength = 8;
constexpr unsigned char kFuseEntry[kFuseEntryLength] = {0x55, 0x8B, 0xEC, 0x51, 0x51, 0x8B, 0x45, 0x08};

enum class Kind { Decode, Tear, Widen, Cap };

struct Site {
    std::uint32_t va;
    Kind kind;
    std::uint8_t len;
    unsigned char orig[5];   // Widen / Cap: the exact bytes. Decode / Tear: checked as call rel32 to the stock target.
    const char* what;
};

// clang-format off
constexpr Site kSites[] = {
    // Decode2 -> Decode4 (the wire contract)
    {0x004E2B9A, Kind::Decode, 5, {}, "GW_CharacterStat::Decode HP"},
    {0x004E2BAE, Kind::Decode, 5, {}, "GW_CharacterStat::Decode MaxHP"},
    {0x004E2BC2, Kind::Decode, 5, {}, "GW_CharacterStat::Decode MP"},
    {0x004E2BD6, Kind::Decode, 5, {}, "GW_CharacterStat::Decode MaxMP"},
    {0x0077621A, Kind::Decode, 5, {}, "CStage::OnSetField HP"},
    {0x004E30DA, Kind::Decode, 5, {}, "DecodeChangeStat HP"},
    {0x004E30F4, Kind::Decode, 5, {}, "DecodeChangeStat MaxHP"},
    {0x004E310E, Kind::Decode, 5, {}, "DecodeChangeStat MP"},
    {0x004E3128, Kind::Decode, 5, {}, "DecodeChangeStat MaxMP"},
    // Tear<short> -> FakeTear: every write to the four slots
    {0x004E2BA4, Kind::Tear, 5, {}, "GW_CharacterStat::Decode HP"},
    {0x004E2BB8, Kind::Tear, 5, {}, "GW_CharacterStat::Decode MaxHP"},
    {0x004E2BCC, Kind::Tear, 5, {}, "GW_CharacterStat::Decode MP"},
    {0x004E2BE0, Kind::Tear, 5, {}, "GW_CharacterStat::Decode MaxMP"},
    {0x00776224, Kind::Tear, 5, {}, "CStage::OnSetField HP"},
    {0x004E30E4, Kind::Tear, 5, {}, "DecodeChangeStat HP"},
    {0x004E30FE, Kind::Tear, 5, {}, "DecodeChangeStat MaxHP"},
    {0x004E3118, Kind::Tear, 5, {}, "DecodeChangeStat MP"},
    {0x004E3132, Kind::Tear, 5, {}, "DecodeChangeStat MaxMP"},
    {0x007646F2, Kind::Tear, 5, {}, "CheckConsumeForActiveSkill HP cost"},
    {0x0076470F, Kind::Tear, 5, {}, "CheckConsumeForActiveSkill MP cost"},
    {0x0078D383, Kind::Tear, 5, {}, "level up: HP = MaxHP"},
    {0x0078D39E, Kind::Tear, 5, {}, "level up: MP = MaxMP"},
    {0x0078D914, Kind::Tear, 5, {}, "sub_78D46C MaxHP gain"},
    {0x0078D961, Kind::Tear, 5, {}, "sub_78D46C MaxMP gain"},
    {0x00967B94, Kind::Tear, 5, {}, "DoActiveSkill HP restore"},
    {0x00967BA2, Kind::Tear, 5, {}, "DoActiveSkill MP restore"},
    // 16-bit uses of a value read back through Fuse
    {0x004E3371, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xDC}, "GW_CharacterStat::GetString MaxMP"},
    {0x004E3376, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xD8}, "GW_CharacterStat::GetString MaxHP"},
    {0x00554AFC, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CField_Dojang::Update HP"},
    {0x00554B31, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CField_Dojang::Update HP"},
    {0x0064286B, Kind::Widen, 3, {0x0F, 0xBF, 0xF0}, "map damage (sub_642710) HP"},
    {0x00764401, Kind::Widen, 3, {0x0F, 0xBF, 0xC8}, "CheckConsumeForActiveSkill HP"},
    {0x007644E4, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CheckConsumeForActiveSkill HP"},
    {0x00764507, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CheckConsumeForActiveSkill MP"},
    {0x0077ED97, Kind::Widen, 3, {0x0F, 0xBF, 0xC8}, "stat calc (sub_77EC9F) MaxHP"},
    {0x0077EDB3, Kind::Widen, 3, {0x0F, 0xBF, 0xC8}, "stat calc (sub_77EC9F) MaxMP"},
    {0x0078D36C, Kind::Widen, 3, {0x66, 0x85, 0xC0}, "level up: test HP > 0"},
    {0x0078D8E7, Kind::Widen, 3, {0x66, 0x3B, 0xC6}, "sub_78D46C MaxHP < cap"},
    {0x0078D8FA, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "sub_78D46C MaxHP"},
    {0x0078D934, Kind::Widen, 3, {0x66, 0x3B, 0xC6}, "sub_78D46C MaxMP < cap"},
    {0x0078D947, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "sub_78D46C MaxMP"},
    {0x007A5B42, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CSummoned::TryDoingHeal HP"},
    {0x007A5BDF, Kind::Widen, 3, {0x0F, 0xBF, 0xCF}, "CSummoned::TryDoingHeal HP"},
    {0x008C5DC4, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xD8}, "CUIStat::Draw HP"},
    {0x008C5EB9, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xD8}, "CUIStat::Draw MP"},
    {0x008CBFDB, Kind::Widen, 3, {0x0F, 0xBF, 0xCB}, "stat window AP buttons (sub_8CBDDB) MaxHP"},
    {0x008CC01E, Kind::Widen, 3, {0x0F, 0xBF, 0xCB}, "stat window AP buttons (sub_8CBDDB) MaxMP"},
    {0x008CC216, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xE8}, "stat window AP buttons (sub_8CBDDB) MaxHP"},
    {0x008CC296, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xE8}, "stat window AP buttons (sub_8CBDDB) MaxMP"},
    {0x008CC464, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "stat window MaxHP getter (sub_8CE65F)"},
    {0x008CC50E, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "stat window MaxMP getter (sub_8CE66E)"},
    {0x008CC9E5, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0x08}, "stat window values (sub_8CC8DE) MaxMP"},
    {0x008CCAD3, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xE0}, "stat window values (sub_8CC8DE) MaxHP"},
    {0x008CDA12, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xDC}, "stat window details (sub_8CD8A0) MaxMP"},
    {0x008CDB29, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xE0}, "stat window details (sub_8CD8A0) MaxHP"},
    {0x008CDF2E, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xD0}, "stat window details (sub_8CD8A0) MaxMP"},
    {0x008CE007, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xD4}, "stat window details (sub_8CD8A0) MaxHP"},
    {0x008D822D, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xE0}, "CUIStatusBar::Draw MP"},
    {0x008D8237, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CUIStatusBar::Draw HP"},
    {0x009203F7, Kind::Widen, 4, {0x0F, 0xBF, 0x45, 0xE0}, "CUIPartyHP::Draw own HP"},
    {0x0094B096, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CUserLocal::Update HP"},
    {0x0094B230, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CUserLocal::Update HP"},
    {0x0094BB78, Kind::Widen, 3, {0x0F, 0xBF, 0xCE}, "CUserLocal::Update MP"},
    {0x0094EA4C, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CUserLocal (sub_94E88C) MP"},
    {0x00950610, Kind::Widen, 3, {0x66, 0x3B, 0xF3}, "CUserLocal::OnResolveMoveAction HP == 0"},
    {0x009506A5, Kind::Widen, 3, {0x66, 0x3B, 0xC3}, "CUserLocal::OnResolveMoveAction HP == 0"},
    {0x009584B6, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CUserLocal::SetDamaged HP"},
    {0x0095960C, Kind::Widen, 3, {0x0F, 0xBF, 0xC9}, "CUserLocal::SetDamaged MP"},
    {0x0095BA30, Kind::Widen, 4, {0x0F, 0xBF, 0x45, 0xF0}, "TryConsumePetHP HP"},
    {0x0095BC7B, Kind::Widen, 3, {0x0F, 0xBF, 0xC3}, "TryConsumePetMP MP"},
    {0x00967733, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "DoActiveSkill HP saved for restore"},
    {0x0096774F, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "DoActiveSkill MP saved for restore"},
    {0x0096AE9D, Kind::Widen, 3, {0x0F, 0xBF, 0xC7}, "DoActiveSkill_Prepare HP %"},
    {0x00485C1C, Kind::Widen, 3, {0x66, 0x85, 0xC0}, "CWvsContext::CanSendExclRequest HP > 0"},
    {0x00A02F5F, Kind::Widen, 3, {0x66, 0x85, 0xC0}, "CWvsContext::TryRecovery HP > 0"},
    {0x00A02F88, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CWvsContext::TryRecovery HP"},
    {0x00A03155, Kind::Widen, 3, {0x66, 0x85, 0xC0}, "CWvsContext::TryRecovery HP > 0"},
    {0x00A03180, Kind::Widen, 3, {0x0F, 0xBF, 0xC0}, "CWvsContext::TryRecovery MP"},
    {0x00A09687, Kind::Widen, 3, {0x66, 0x85, 0xC0}, "sub_A09662 HP > 0"},
    {0x00A2938D, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xEC}, "CheckDarkForce HP"},
    {0x00A29412, Kind::Widen, 3, {0x0F, 0xBF, 0xCF}, "CheckDarkForce HP"},
    {0x00A29535, Kind::Widen, 4, {0x0F, 0xBF, 0x4D, 0xE8}, "CheckDragonFury MP"},
    {0x00A29586, Kind::Widen, 3, {0x0F, 0xBF, 0xCF}, "CheckDragonFury MP"},
    // 30,000 caps -> 99,999 (imm32 of mov esi / mov ebx / cmp eax)
    {0x0078D8D2, Kind::Cap, 4, {0x30, 0x75, 0x00, 0x00}, "sub_78D46C MaxHP/MaxMP cap"},
    {0x0077F1A0, Kind::Cap, 4, {0x30, 0x75, 0x00, 0x00}, "stat calc (sub_77EC9F) MaxHP/MaxMP cap"},
    {0x008CD657, Kind::Cap, 4, {0x30, 0x75, 0x00, 0x00}, "stat window AP -> MaxHP cap"},
    {0x008CD6EB, Kind::Cap, 4, {0x30, 0x75, 0x00, 0x00}, "stat window AP -> MaxMP cap"},
};
// clang-format on
constexpr std::size_t kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

inline std::uint32_t Le32(const unsigned char* b) {
    return b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

inline void CallBytes(std::uint32_t va, std::uint32_t target, unsigned char* out) {
    const std::uint32_t rel = target - (va + 5);
    out[0] = 0xE8;
    for (int i = 0; i < 4; ++i) out[1 + i] = static_cast<unsigned char>(rel >> (8 * i));
}

// The bytes a site holds before patching. Decode / Tear: call to the stock callee.
inline void OriginalBytes(const Site& s, unsigned char* out) {
    if (s.kind == Kind::Decode) CallBytes(s.va, kAddr_Decode2, out);
    else if (s.kind == Kind::Tear) CallBytes(s.va, kAddr_TearShort, out);
    else std::memcpy(out, s.orig, s.len);
}

// The bytes after patching. fakeTear: the address of the FakeTear function (Tear sites only).
inline void PatchedBytes(const Site& s, std::uint32_t fakeTear, unsigned char* out) {
    switch (s.kind) {
    case Kind::Decode: CallBytes(s.va, kAddr_Decode4, out); return;
    case Kind::Tear: CallBytes(s.va, fakeTear, out); return;
    case Kind::Cap: {
        const std::uint32_t v = kMaxValue;
        for (int i = 0; i < 4; ++i) out[i] = static_cast<unsigned char>(v >> (8 * i));
        return;
    }
    case Kind::Widen:
        if (s.orig[0] == 0x66) {             // 66 op modrm -> op modrm nop
            out[0] = s.orig[1];
            out[1] = s.orig[2];
        } else {                             // 0F BF modrm [disp8] -> 8B modrm [disp8] nop
            out[0] = 0x8B;
            std::memcpy(out + 1, s.orig + 2, s.len - 2);
        }
        out[s.len - 1] = 0x90;
        return;
    }
}

enum class SiteState { Original, Patched, Unknown };

inline SiteState Classify(const Site& s, const unsigned char* current, std::uint32_t fakeTear) {
    unsigned char b[5];
    OriginalBytes(s, b);
    if (std::memcmp(current, b, s.len) == 0) return SiteState::Original;
    PatchedBytes(s, fakeTear, b);
    if (std::memcmp(current, b, s.len) == 0) return SiteState::Patched;
    return SiteState::Unknown;
}

}  // namespace HpMp
