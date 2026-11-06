#include "pch.h"
#include "hook.h"
#include "wvs/packet.h"
#include "debug.h"
#include <mutex>
#ifdef MAPLEROOT_CASHSHOP_WINDOW
#include "cashshopwnd.h"
#endif
#ifdef MAPLEROOT_DAILY_CHECKIN
#include "dailycheckin.h"
#endif
#ifdef MAPLEROOT_SET_EQUIP
#include "setitem.h"
#endif
#ifdef MAPLEROOT_MONSTER_BOOK
#include "monsterbook.h"
#endif
#ifdef MAPLEROOT_COLOR_PRISM
#include "weapontint.h"
#endif
#ifdef MAPLEROOT_INTEGRATION
#include "bounce_rules.h"
#endif
#ifdef MAPLEROOT_WEATHER
#include "weather.h"
#include "lamps.h"
#endif

namespace {
auto ProcessPacket = reinterpret_cast<void(__thiscall*)(void*, CInPacket*)>(0x004965F1);
std::mutex dispatchGate;
bool shuttingDown = false;

// b331: the last incoming packets (opcode, length), written to integration.log when a packet's handler throws and at
// session cleanup, so a silent client close names the packet behind it. Opcodes and lengths only, never packet bytes.
constexpr int kRecentPackets = 16;
struct RecentPacket { unsigned short opcode; unsigned short length; };
RecentPacket recentPackets[kRecentPackets] = {};
volatile LONG recentCount = 0;
volatile LONG throwsLogged = 0;

void NoteRecentPacket(unsigned short opcode, unsigned short length) {
    const LONG n = InterlockedIncrement(&recentCount) - 1;
    recentPackets[n % kRecentPackets] = {opcode, length};
}

void LogRecentPackets(const char* why) {
    const LONG n = recentCount;
    char line[512] = {};
    int used = sprintf_s(line, "PACKET_TRAIL %s (%ld received):", why, n);
    for (LONG i = n > kRecentPackets ? n - kRecentPackets : 0; i < n && used > 0 && used < 480; ++i) {
        const RecentPacket& p = recentPackets[i % kRecentPackets];
        used += sprintf_s(line + used, sizeof(line) - used, " %04X/%u", p.opcode, p.length);
    }
    DebugMessage("%s", line);
}

int NotePacketThrow(EXCEPTION_POINTERS* ep, unsigned short opcode, unsigned short length) {
    if (InterlockedIncrement(&throwsLogged) <= 8) {
        const DWORD code = ep->ExceptionRecord->ExceptionCode;
        DWORD hr = 0;
        if (code == 0xE06D7363 && ep->ExceptionRecord->NumberParameters >= 2) {
            __try { hr = *reinterpret_cast<DWORD*>(ep->ExceptionRecord->ExceptionInformation[1]); }
            __except (EXCEPTION_EXECUTE_HANDLER) { hr = 0; }
        }
        DebugMessage("PACKET_THROW opcode=%04X length=%u code=%08lX hresult=%08lX eip=%08lX", opcode, length, code, hr,
            ep->ContextRecord->Eip);
        LogRecentPackets("at throw");
    }
    return EXCEPTION_CONTINUE_SEARCH;   // the game's own handler still decides what happens
}

void ProcessPacketObserved(void* socket, CInPacket* packet, unsigned short opcode, unsigned short length) {
    __try {
        ProcessPacket(socket, packet);
    } __except (NotePacketThrow(GetExceptionInformation(), opcode, length)) {
    }
}
void __fastcall ProcessPacketHook(void* socket, void*, CInPacket* packet) {
    unsigned short opcode = 0;
    if (packet && packet->TryPeekOpcode(opcode)) {
#ifdef MAPLEROOT_CASHSHOP_WINDOW
        if (opcode == kCashShopSyncOpcode) {
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) CashShopWnd_HandleSync(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_DAILY_CHECKIN
        if (opcode == kDailyCheckinSyncOpcode) {
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) DailyCheckin_HandleSync(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_SET_EQUIP
        if (opcode == kSetItemSyncOpcode) {
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) SetItem_HandleSync(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_MONSTER_BOOK
        if (opcode == kMonsterBookResultOpcode) {
            // Client-addon-only opcode: swallowed here, the stock client has no handler for it.
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) MonsterBook_HandleResult(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_HUD_METERS
        if (opcode == 0x3739) {
            // Party damage ranks (hud_meters_policy.h kDamageRankOpcode): client-addon only, swallowed here.
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) HudMeters_HandleDamageRank(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_COLOR_PRISM
        if (opcode == kWeaponTintSyncOpcode) {
            // WeaponTint_HandleSync skips the opcode itself from the CURRENT offset, which the peek left in place.
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) WeaponTint_HandleSync(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_INTEGRATION
        if (opcode == bounce::kBounceZonesSyncOpcode) {
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) BounceZones_HandleSync(packet);
            return;
        }
#endif
#ifdef MAPLEROOT_WEATHER
        if (opcode == 0x373D || opcode == 0x373F) {
            std::lock_guard<std::mutex> lock(dispatchGate);
            if (!shuttingDown) {
                if (opcode == 0x373D) Weather_HandleWorldState(packet);
                else Lamp_HandlePreviewPacket(packet);
            }
            return;
        }
#endif
    }
    const unsigned short length = packet ? static_cast<unsigned short>(packet->LengthPublic()) : 0;
    NoteRecentPacket(opcode, length);
    ProcessPacketObserved(socket, packet, opcode, length);
}
}
void ShutdownCustomPackets() {
    LogRecentPackets("at cleanup");
    std::lock_guard<std::mutex> lock(dispatchGate);
    shuttingDown = true;
}
void AttachCustomPacketMod() { ATTACH_HOOK(ProcessPacket, ProcessPacketHook); }
