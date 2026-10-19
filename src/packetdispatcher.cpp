#include "pch.h"
#include "hook.h"
#include "wvs/packet.h"
#include <mutex>
#ifdef MAPLEROOT_CASHSHOP_WINDOW
#include "cashshopwnd.h"
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
}
}
void ShutdownCustomPackets() {
    std::lock_guard<std::mutex> lock(dispatchGate);
    shuttingDown = true;
}
void AttachCustomPacketMod() { ATTACH_HOOK(ProcessPacket, ProcessPacketHook); }
