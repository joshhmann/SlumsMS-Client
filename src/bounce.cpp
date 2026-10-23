// Double-tap jump and the bounce block zones (MAPLEROOT_INTEGRATION). The rules, addresses and the
// story of how MapleRoot's bounces work are in bounce_rules.h; this file only touches the game.
//
// Installed once at startup, nothing is rewritten afterwards:
//   * 0x0094F5A7: the jump key's `call CUserLocal::Jump` -> JumpKeyThunk (rel32 only), when
//     [clienthax] doubletapjump is true (the default).
//   * The block is a check at the top of MapleRoot's existing CUserLocal::DoActiveSkill detour
//     (mapleroot/ijl15/SkillEdits/Activeskill.h) calling SlumsBounce_BlockSkill; no new hook.
//   * LP 0x3737 BOUNCE_ZONES_SYNC through the shared packet dispatcher (packetdispatcher.cpp).
#include "pch.h"
#include "hook.h"
#include "patch_common.h"
#include "constants.h"
#include "wvs/packet.h"
#include "bounce_rules.h"
#include "attackstop_patch.h"
#include <mutex>

namespace {

constexpr uintptr_t kAddr_CWvsContext = 0x00BE7918;
constexpr size_t kCtxCharacterData = 0x20B8;
constexpr uintptr_t kAddr_CSkillInfo = 0x00BE78DC;
constexpr size_t kUserVecCtrl = 0x11A4;   // IWzVector2D* of the CVecCtrlUser, object at -0xC
constexpr size_t kVecFoothold = 0x110;

auto CUserLocal__Jump = reinterpret_cast<void(__thiscall*)(void*, int)>(bounce::kCUserLocalJump);
// Calling the address goes through MapleRoot's detour, exactly like a skill hotkey.
auto CUserLocal__DoActiveSkill = reinterpret_cast<int(__thiscall*)(void*, int, unsigned int, int*)>(0x00966F7A);
auto CSkillInfo__GetSkillLevel = reinterpret_cast<int(__thiscall*)(void*, void*, int, void**)>(0x007616F6);
auto CVecCtrl__IsOnLadder = reinterpret_cast<int(__thiscall*)(void*)>(0x00705309);
auto CVecCtrl__IsOnRope = reinterpret_cast<int(__thiscall*)(void*)>(0x00705343);
auto CVecCtrl__IsSwimOrFly = reinterpret_cast<int(__thiscall*)(void*)>(0x007046AC);
auto CWvsContext__GetCurFieldID = reinterpret_cast<int(__thiscall*)(void*)>(0x00A1238B);
// CUserLocal::GetJobCode, as MapleRoot's Discord presence calls it (YourStuff.h GetJobCode).
constexpr uintptr_t kAddr_CUserLocal = 0x00BEBF98;
auto CUserLocal__GetJobCode = reinterpret_cast<int(__thiscall*)(void*)>(0x0095FFC3);

bool g_doubleTap = true;
DWORD g_jumpKeyLParam = 0; // written by JumpKeyThunk, game thread only

std::mutex g_zoneGate;
bounce::ZoneSet g_zones = bounce::ZoneSet::FromRanges(bounce::BuiltinJumpQuestRanges());
bounce::ZoneSet g_fjZones = bounce::ZoneSet::FromRanges(bounce::BuiltinJumpQuestRanges());
int g_nativeBounce = 0;
bool g_fromServer = false;
// Main thread reads it per bound jump; the receive thread replaces it under g_zoneGate.
bounce::BounceImpulses g_impulses;
bool g_impulsesFromServer = false;

int CurrentFieldId() {
    __try {
        return CWvsContext__GetCurFieldID(nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int CurrentJob() {
    __try {
        void* user = *reinterpret_cast<void**>(kAddr_CUserLocal);
        return user ? CUserLocal__GetJobCode(user) : -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

bounce::MapBlock MapBlocked(int mapId) {
    bounce::MapBlock b;
    if (mapId <= 0) return b;
    std::lock_guard<std::mutex> lock(g_zoneGate);
    b.bounce = g_zones.Contains(mapId);
    b.flashJump = g_fjZones.Contains(mapId);
    return b;
}

int SkillLevel(int skillId) {
    __try {
        void* ctx = *reinterpret_cast<void**>(kAddr_CWvsContext);
        void* skillInfo = *reinterpret_cast<void**>(kAddr_CSkillInfo);
        if (!ctx || !skillInfo) return 0;
        void* charData = *reinterpret_cast<void**>(reinterpret_cast<char*>(ctx) + kCtxCharacterData);
        if (!charData) return 0;
        return CSkillInfo__GetSkillLevel(skillInfo, charData, skillId, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bounce::AirState ReadAirState(void* user) {
    bounce::AirState s;
    __try {
        auto raw = *reinterpret_cast<char**>(reinterpret_cast<char*>(user) + kUserVecCtrl);
        if (!raw) return s;
        void* vec = raw - 0xC;
        s.hasVecCtrl = true;
        s.onFoothold = *reinterpret_cast<void**>(reinterpret_cast<char*>(vec) + kVecFoothold) != nullptr;
        s.onLadder = CVecCtrl__IsOnLadder(vec) != 0;
        s.onRope = CVecCtrl__IsOnRope(vec) != 0;
        s.swimOrFly = CVecCtrl__IsSwimOrFly(vec) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s = bounce::AirState{};
    }
    return s;
}

void __fastcall JumpKeyCall(void* user, void*, int bEnforced) {
    bounce::DoubleTapInput in;
    in.enabled = g_doubleTap && bEnforced == 0;
    in.lParam = g_jumpKeyLParam;
    if (in.enabled && bounce::IsFreshPress(in.lParam)) {
        in.air = ReadAirState(user);
        if (bounce::IsAirborne(in.air)) in.here = MapBlocked(CurrentFieldId());
    }
    int native;
    {
        std::lock_guard<std::mutex> lock(g_zoneGate);
        native = g_nativeBounce;
    }
    const int skill = bounce::DecideDoubleTap(in, SkillLevel, native);
    CUserLocal__Jump(user, bEnforced);
    if (skill) CUserLocal__DoActiveSkill(user, skill, 0, nullptr);
}

// Replaces the rel32 of the jump key's call. ecx = CUserLocal*, [esp+4] = bEnforced, and ebp is still
// CUserLocal::OnKeyDown's frame, whose only argument [ebp+8] is the WM_KEYDOWN lParam.
__declspec(naked) void JumpKeyThunk() {
    __asm {
        mov eax, dword ptr [ebp + 8]
        mov g_jumpKeyLParam, eax
        jmp JumpKeyCall
    }
}

bool ReadIniBool(const char* section, const char* key, bool fallback) {
    char value[32] = {};
    GetPrivateProfileStringA(section, key, "", value, sizeof(value), ".\\" CONSTANTS_CONFIG_NAME);
    return integration::ParseIniBool(value).value_or(fallback);
}

} // namespace

// Called by MapleRoot's FlashJumpAll cave (SkillEdits/Activeskill.h) for every bound jump it
// handles, right before CUserLocal::DoActiveSkill_BoundJump reads its impulse: writes the three
// immediates for this skill and this character's job family. Same bytes, same moment as
// MapleRoot's applyVelocityChange / restoreVelocityChange, which this replaces.
extern "C" void __cdecl SlumsBounce_ApplyImpulse(int nSkillID) {
    bounce::BounceImpulses cfg;
    {
        std::lock_guard<std::mutex> lock(g_zoneGate);
        cfg = g_impulses;
    }
    const int job = CurrentJob();
    bounce::Impulse v;
    if (!bounce::ImpulseFor(nSkillID, job, cfg, v)) return;
    PatchInt(bounce::kImpulseLeftVx, v.leftVx);
    PatchInt(bounce::kImpulseRightVx, v.rightVx);
    PatchInt(bounce::kImpulseVy, v.vy);
    LOG_ONCE_PER_ID(nSkillID, "Bounce: skill %d as job %d -> impulse %d / %d / %d (%s)", nSkillID, job,
        v.leftVx, v.rightVx, v.vy, g_impulsesFromServer ? "server values" : "MapleRoot defaults");
}

// Called by MapleRoot's DoActiveSkill detour before anything else (Activeskill.h).
extern "C" int __cdecl SlumsBounce_BlockSkill(int nSkillID) {
    if (!bounce::IsUniversalBounce(nSkillID) && !bounce::IsFlashJump(nSkillID)) return 0;
    const int mapId = CurrentFieldId();
    if (!bounce::ShouldBlockSkill(nSkillID, MapBlocked(mapId))) return 0;
    LOG_ONCE("Bounce: skill %d refused on blocked map %d (logged once per session)", nSkillID, mapId);
    return 1;
}

void BounceZones_HandleSync(CInPacket* packet) {
    const size_t start = packet->GetOffset();
    const unsigned char* bytes = packet->CurrentPublic();
    const size_t length = packet->LengthPublic();
    if (!bytes || length < start + 2) return;
    // Version 2 carries the bounce impulses; version 1 the zones (see bounce_rules.h).
    if (length >= start + 3 && bytes[2] == bounce::kBounceImpulsesVersion) {
        bounce::BounceImpulses imp;
        if (!bounce::DecodeImpulsesSync(bytes + 2, length - start - 2, imp)) {
            DebugMessage("Bounce: malformed bounce impulse sync (%u bytes), keeping the current values",
                static_cast<unsigned>(length - start));
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_zoneGate);
            g_impulses = imp;
            g_impulsesFromServer = true;
        }
        DebugMessage("Bounce: server impulses ninja %d/%d/%d (other jobs %d/%d/%d), vertical %d/%d/%d (other jobs %d/%d/%d)",
            imp.ninjaNative.leftVx, imp.ninjaNative.rightVx, imp.ninjaNative.vy,
            imp.ninjaOther.leftVx, imp.ninjaOther.rightVx, imp.ninjaOther.vy,
            imp.verticalNative.leftVx, imp.verticalNative.rightVx, imp.verticalNative.vy,
            imp.verticalOther.leftVx, imp.verticalOther.rightVx, imp.verticalOther.vy);
        return;
    }
    bounce::ZonesSync sync;
    if (!bounce::DecodeZonesSync(bytes + 2, length - start - 2, sync)) {
        DebugMessage("Bounce: malformed BOUNCE_ZONES_SYNC (%u bytes), keeping the current list",
            static_cast<unsigned>(length - start));
        return;
    }
    auto zones = bounce::ZoneSet::FromRanges(std::move(sync.bounce));
    auto fjZones = bounce::ZoneSet::FromRanges(std::move(sync.flashJump));
    const size_t ranges = zones.Ranges().size(), maps = zones.MapCount(), fjRanges = fjZones.Ranges().size();
    {
        std::lock_guard<std::mutex> lock(g_zoneGate);
        g_zones = std::move(zones);
        g_fjZones = std::move(fjZones);
        g_nativeBounce = sync.nativeBounce;
        g_fromServer = true;
    }
    DebugMessage("Bounce: server zone list, %u ranges / %u maps blocked (%u Flash Jump ranges), native bounce %d",
        static_cast<unsigned>(ranges), static_cast<unsigned>(maps), static_cast<unsigned>(fjRanges), sync.nativeBounce);
}

void AttachBounceMod() {
    g_doubleTap = ReadIniBool("clienthax", "doubletapjump", true);
    DebugMessage("Bounce: built-in jump-quest list %u ranges; doubletapjump=%d",
        static_cast<unsigned>(bounce::BuiltinJumpQuestRanges().size()), g_doubleTap ? 1 : 0);
    if (!g_doubleTap) return;
    const auto thunk = static_cast<std::uint32_t>(reinterpret_cast<uintptr_t>(&JumpKeyThunk));
    switch (bounce::ClassifyJumpSite(reinterpret_cast<const unsigned char*>(bounce::kJumpKeyContext), thunk)) {
    case bounce::JumpSiteState::Original:
        PatchCall(bounce::kJumpKeyCallSite, &JumpKeyThunk);
        DebugMessage("Bounce: double-tap jump installed at 0x%08X", bounce::kJumpKeyCallSite);
        break;
    case bounce::JumpSiteState::Patched:
        break;
    default:
        DebugMessage("Bounce: unexpected bytes at 0x%08X, double-tap jump NOT installed", bounce::kJumpKeyContext);
        break;
    }
}
