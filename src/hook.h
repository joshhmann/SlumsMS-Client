#pragma once
#include "debug.h"
#include "mapleroot.h"
#include "avatar_features.h"
#include <type_traits>

#ifdef _DEBUG
#define ATTACH_HOOK(TARGET, DETOUR) \
    AttachHook(reinterpret_cast<void**>(&TARGET), CastHook(&DETOUR)) ? true : (ErrorMessage("Failed to attach detour function \"%s\" at target address : 0x%08X.", #DETOUR, TARGET), false)
#else
#define ATTACH_HOOK(TARGET, DETOUR) \
    AttachHook(reinterpret_cast<void**>(&TARGET), CastHook(&DETOUR))
#endif

#define MEMBER_AT(T, OFFSET, NAME) \
    __declspec(property(get = get_##NAME, put = set_##NAME)) T NAME; \
    __forceinline const T& get_##NAME() const { \
        return *reinterpret_cast<const T*>(reinterpret_cast<uintptr_t>(this) + OFFSET); \
    } \
    __forceinline T& get_##NAME() { \
        return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(this) + OFFSET); \
    } \
    __forceinline void set_##NAME(const T& value) { \
        *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(this) + OFFSET) = const_cast<T&>(value); \
    } \
    __forceinline void set_##NAME(T& value) { \
        *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(this) + OFFSET) = value; \
    }

#define MEMBER_ARRAY_AT(T, OFFSET, NAME, N) \
    __declspec(property(get = get_##NAME)) T(&NAME)[N]; \
    __forceinline T(&get_##NAME())[N] { \
        return *reinterpret_cast<T(*)[N]>(reinterpret_cast<uintptr_t>(this) + OFFSET); \
    }

#define MEMBER_HOOK(T, ADDRESS, NAME, ...) \
    inline static auto NAME = reinterpret_cast<T(__thiscall*)(void*, __VA_ARGS__)>(ADDRESS); \
    T NAME##_hook(__VA_ARGS__);

#define TO_UINTPTR(VALUE) ((uintptr_t)(VALUE))

#define TO_PVOID(VALUE) ((void*)(VALUE))


// called in injector.cpp -> DllMain
void AttachSystemHooks();

// called in system.cpp -> SetUnhandledExceptionFilter_hook
void AttachClientBypass();
#if defined(MAPLEROOT_CASHSHOP_WINDOW) || defined(MAPLEROOT_WEATHER) || defined(MAPLEROOT_DAILY_CHECKIN) || defined(MAPLEROOT_SET_EQUIP) || defined(MAPLEROOT_MONSTER_BOOK) || defined(MAPLEROOT_COLOR_PRISM) || defined(MAPLEROOT_INTEGRATION)
void AttachCustomPacketMod();
void ShutdownCustomPackets();
#endif
void AttachClientInlink();
void ReleaseClientInlinkResources();
void AttachStringPoolMod();
void AttachResManMod();
void AttachAvatarDataMod();
void AttachAvatarStorageMod();
void AttachAvatarChairMod();
void AttachItemEffectMod();
void ReleaseItemEffectResources();
void AttachResolutionMod();
void AttachMobHpTagMod();
void AttachToolTipMod();
void AttachIconIconMod();
void AttachPetEquipFlagMod();   // petequipflag.cpp: newer pets overflow the 128-bit pet flag
void AttachProposalGenderMod(); // proposalgender.cpp: any gender may use an engagement ring box
void AttachCreateBlockMod();    // createblock.cpp: no random "high population" block on character creation
void AttachMobDamageMod();      // mobdamage.cpp: low defence never adds mob touch damage, [clienthax] lowdefensepenalty
void AttachMeleeRangeMod();     // meleerange.cpp: a remote attack in a pose without a range no longer closes the client
void AttachHpMpMod();           // hpmp.cpp: 4-byte HP/MP up to 99,999; refuses to start on unexpected bytes
void HpMp_Verify();             // hpmp.cpp: after every other mod, checks none overwrote an HP/MP site
void AttachUiLayoutMod();       // uilayout.cpp: moved status bar buttons keep their clicks (CUIStatusBar::HitTest)
void AttachTempStatMod();
#ifdef MAPLEROOT_CHAT_TABS
void AttachChatLogMod();     // chatlog.cpp: chat tabs and blocklists, config.ini [clienthax] chattabs
#endif
#ifdef MAPLEROOT_INV_RESIZE
void AttachInvResizeMod();   // invresize.cpp: drag-to-resize item inventory, config.ini [clienthax] invresize
#endif
#ifdef MAPLEROOT_MOUNT_INFO
void AttachMountInfoMod();
#endif
#ifdef MAPLEROOT_MOVEMENT_KEYS
void AttachMovementKeysMod();
#endif
#ifdef MAPLEROOT_MONSTER_BOOK
// monsterbook*.cpp; the shared surface is monsterbook.h
void AttachMonsterBookMod();
void AttachMonsterBookFoundInMod();
void AttachMonsterBookDropsMod();
void AttachMonsterBookSearchMod();
void MonsterBook_Tick();
#endif
#ifdef MAPLEROOT_COLOR_PRISM
// weapontint.cpp (renderer + protocol, CAvatar::PrepareActionLayer and the face builder),
// coloringprism.cpp (the window), coloringprism_hooks.cpp (cash-use gate/sends, item drop)
void AttachWeaponTintMod();
void AttachColoringPrismMod();
void AttachColoringPrismHostHooks();
void WeaponTint_Tick();
#endif
#ifdef MAPLEROOT_INTEGRATION
// bounce.cpp: double-tap jump ([clienthax] doubletapjump) and the bounce block zones (LP 0x3737)
void AttachBounceMod();
class CInPacket;
void BounceZones_HandleSync(CInPacket* packet);
#endif
#ifdef MAPLEROOT_ATTACK_DIAG
// attackdiag.cpp: logs other characters' attacks the client drops (config.ini [debug] attackdiag=1)
void AttachAttackDiagMod();
#endif
#ifdef MAPLEROOT_WEATHER
void AttachWeatherMod();
void AttachWeatherWindMod();
#endif

inline void AttachClientHooks() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    DebugMessage("Installing: AttachClientBypass();");
    AttachClientBypass();
#ifdef MAPLEROOT_MOUNT_INFO
    AttachMountInfoMod();
    VerifyKaentakeHooks("Mount preview and name");
#endif
#ifdef MAPLEROOT_MOVEMENT_KEYS
    AttachMovementKeysMod();
    VerifyKaentakeHooks("Bindable movement keys");
#endif
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachClientBypass");
#endif
    // Before MapleRoot and the other mods, so every HP/MP site is checked against the stock client.
    DebugMessage("Installing: AttachHpMpMod();");
    AttachHpMpMod();
#ifdef MAPLEROOT_INTEGRATION
    InitializeMapleRoot();
#endif
    DebugMessage("Installing: AttachClientInlink();");
    AttachClientInlink();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachClientInlink");
#endif
    DebugMessage("Installing: AttachStringPoolMod();");
    AttachStringPoolMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachStringPoolMod");
#endif
    DebugMessage("Installing: AttachResManMod();");
    AttachResManMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachResManMod");
#endif
#ifdef MAPLEROOT_INTEGRATION
    constexpr bool mapleRootChairs = true;
#else
    constexpr bool mapleRootChairs = false;
#endif
    integration::InstallAvatarFeatures(mapleRootChairs,
        [] {
            DebugMessage("Installing: AttachAvatarStorageMod();");
            AttachAvatarStorageMod();
#ifdef MAPLEROOT_INTEGRATION
            VerifyKaentakeHooks("AttachAvatarStorageMod");
#endif
        },
        [] {
            DebugMessage("Installing: AttachAvatarChairMod();");
            AttachAvatarChairMod();
        },
        [] {
            DebugMessage("Installing: AttachItemEffectMod();");
            AttachItemEffectMod();
#ifdef MAPLEROOT_INTEGRATION
            VerifyKaentakeHooks("AttachItemEffectMod");
#endif
        });
    DebugMessage("Installing: AttachResolutionMod();");
    AttachResolutionMod();
#ifdef MAPLEROOT_WEATHER
    AttachWeatherMod();
    AttachWeatherWindMod();
    VerifyKaentakeHooks("Weather/time/lamps");
#endif
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachResolutionMod");
#endif
#ifndef MAPLEROOT_INTEGRATION
    DebugMessage("Installing: AttachMobHpTagMod();");
    AttachMobHpTagMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachMobHpTagMod");
#endif
#endif
#ifdef MAPLEROOT_MONSTER_BOOK
    // DROPS before SEARCH: the search module reads the 16-per-page Dropping patch the drops module installs.
    AttachMonsterBookMod();
    AttachMonsterBookFoundInMod();
    AttachMonsterBookDropsMod();
    AttachMonsterBookSearchMod();
    VerifyKaentakeHooks("Monster Book");
#endif
#ifdef MAPLEROOT_COLOR_PRISM
    AttachWeaponTintMod();
    AttachColoringPrismMod();
    AttachColoringPrismHostHooks();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("Coloring Prism");
#endif
#endif
    DebugMessage("Installing: AttachToolTipMod();");
    AttachToolTipMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachToolTipMod");
#endif
    DebugMessage("Installing: AttachIconIconMod();");
    AttachIconIconMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachIconIconMod");
#endif
    DebugMessage("Installing: AttachPetEquipFlagMod();");
    AttachPetEquipFlagMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachPetEquipFlagMod");
#endif
    DebugMessage("Installing: AttachProposalGenderMod();");
    AttachProposalGenderMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachProposalGenderMod");
#endif
    DebugMessage("Installing: AttachCreateBlockMod();");
    AttachCreateBlockMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachCreateBlockMod");
#endif
#ifdef MAPLEROOT_CHAT_TABS
    DebugMessage("Installing: AttachChatLogMod();");
    AttachChatLogMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachChatLogMod");
#endif
#endif
#ifdef MAPLEROOT_INV_RESIZE
    DebugMessage("Installing: AttachInvResizeMod();");
    AttachInvResizeMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachInvResizeMod");
#endif
#endif
    DebugMessage("Installing: AttachUiLayoutMod();");
    AttachUiLayoutMod();
    DebugMessage("Installing: AttachMobDamageMod();");
    AttachMobDamageMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachMobDamageMod");
#endif
    DebugMessage("Installing: AttachMeleeRangeMod();");
    AttachMeleeRangeMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachMeleeRangeMod");
#endif
#ifndef MAPLEROOT_INTEGRATION
    DebugMessage("Installing: AttachTempStatMod();");
    AttachTempStatMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachTempStatMod");
#endif
#endif
#ifdef MAPLEROOT_INTEGRATION
    DebugMessage("Installing: AttachBounceMod();");
    AttachBounceMod();
    VerifyKaentakeHooks("AttachBounceMod");
#endif
#ifdef MAPLEROOT_ATTACK_DIAG
    DebugMessage("Installing: AttachAttackDiagMod();");
    AttachAttackDiagMod();
#ifdef MAPLEROOT_INTEGRATION
    VerifyKaentakeHooks("AttachAttackDiagMod");
#endif
#endif
    HpMp_Verify();
}



template <typename T>
constexpr auto CastHook(T fn) -> void* {
    union {
        T fn;
        void* p;
    } u;
    u.fn = fn;
    return u.p;
}

long GetKaentakeHookFailureCount();
bool AttachHook(void** ppTarget, void* pDetour);
bool DetachHook(void** ppTarget, void* pDetour);

void* VMTHook(void* pInstance, void* pDetour, size_t uIndex);

void* GetAddress(const char* sModuleName, const char* sProcName);

void* GetAddressByPattern(const char* sModuleName, const char* sPattern);

void PatchMemory(void* pAddress, void* pValue, size_t uSize);

void PatchAllByPattern(void* pStart, void* pEnd, const char* sPattern, void* pValue, size_t uSize);


template <typename T>
void Patch1(T pAddress, unsigned char uValue) {
    PatchMemory(TO_PVOID(pAddress), &uValue, sizeof(uValue));
}

template <typename T>
void Patch4(T pAddress, unsigned int uValue) {
    PatchMemory(TO_PVOID(pAddress), &uValue, sizeof(uValue));
}

template <typename T>
void PatchStr(T pAddress, const char* sValue) {
    PatchMemory(TO_PVOID(pAddress), TO_PVOID(sValue), strlen(sValue));
}

template <typename T, typename U>
void PatchNop(T pAddress, U pDestination) {
    size_t uSize = TO_UINTPTR(pDestination) - TO_UINTPTR(pAddress);
    void* pValue = malloc(uSize);
    memset(pValue, 0x90, uSize);
    PatchMemory(TO_PVOID(pAddress), pValue, uSize);
    free(pValue);
}

template <typename T, typename U>
void PatchJmp(T pAddress, U pDestination) {
    Patch1(pAddress, 0xE9);
    Patch4(pAddress + 1, TO_UINTPTR(pDestination) - TO_UINTPTR(pAddress) - 5);
}

template <typename T, typename U>
void PatchCall(T pAddress, U pDestination, size_t uSize = 5) {
    if (uSize < 5) {
        ErrorMessage("Cannot PatchCall at 0x%08X with uSize = %d", TO_UINTPTR(pAddress), uSize);
        return;
    }
    Patch1(pAddress, 0xE8);
    Patch4(pAddress + 1, TO_UINTPTR(pDestination) - TO_UINTPTR(pAddress) - 5);
    if (uSize > 5) {
        PatchNop(pAddress + 5, pAddress + uSize);
    }
}

template <typename T>
void PatchRetZero(T pAddress) {
    PatchStr(pAddress, "\x33\xC0\xC3");
}
