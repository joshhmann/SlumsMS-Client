#include "pch.h"
#include "hook.h"
#include "wvs/packet.h"
#include <mutex>
#include "qamodule.h"
#ifdef SLUMS_QA_MODULE
#include "qamodule_policy.h"   // PacketFromOpcode
#endif
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
#ifdef SLUMS_QA_MODULE
    // Where the opcode starts: the receive buffer begins with the socket header, and the handler moves the offset on.
    const size_t start = packet ? packet->GetOffset() : 0;
#endif
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
    ProcessPacket(socket, packet);
    // QA builds only (ticket 166): a handled packet may make a screen ready, add a chat line, or ask for a dialog.
    // The module reads each packet from its opcode (qamodule_policy.h PacketFromOpcode): from the start of the buffer,
    // every NPC, chat and dialog parse read the header and failed (D1, 2026-11-06).
#ifdef SLUMS_QA_MODULE
    if (packet) {
        const QaPolicy::PacketBytes body = QaPolicy::PacketFromOpcode(packet->BufferPublic(), packet->LengthPublic(), start);
        if (body.data) QaModule_OnPacket(opcode, body.data, body.size);
    }
#endif
}
}
void ShutdownCustomPackets() {
    std::lock_guard<std::mutex> lock(dispatchGate);
    shuttingDown = true;
}
void AttachCustomPacketMod() { ATTACH_HOOK(ProcessPacket, ProcessPacketHook); }
