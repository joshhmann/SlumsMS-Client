#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "wvs/packet.h"
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
#ifdef MAPLEROOT_RECV_TRACE
    // Diagnostic build only: every received opcode and length, and the read position and bytes of a packet whose
    // stock handler throws (a decode past the end ends the session with a CDisconnectException).
    if (packet) DebugMessage("RECV op=0x%04X len=%u", opcode, static_cast<unsigned>(packet->LengthPublic()));
    try {
        ProcessPacket(socket, packet);
    } catch (...) {
        if (packet && packet->BufferPublic()) {
            char hex[3 * 96 + 1] = {};
            const size_t n = packet->LengthPublic() < 96 ? packet->LengthPublic() : 96;
            for (size_t i = 0; i < n; ++i) sprintf_s(hex + 3 * i, 4, "%02X ", packet->BufferPublic()[i]);
            DebugMessage("RECV THREW op=0x%04X len=%u read-offset=%u bytes: %s", opcode,
                static_cast<unsigned>(packet->LengthPublic()), static_cast<unsigned>(packet->GetOffset()), hex);
        }
        throw;
    }
#else
    ProcessPacket(socket, packet);
#endif
}
}
void ShutdownCustomPackets() {
    std::lock_guard<std::mutex> lock(dispatchGate);
    shuttingDown = true;
}
void AttachCustomPacketMod() { ATTACH_HOOK(ProcessPacket, ProcessPacketHook); }
