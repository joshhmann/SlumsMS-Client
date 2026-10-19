#pragma once
struct CInPacket;
constexpr unsigned short kDailyCheckinSyncOpcode = 0x3733;
void DailyCheckin_HandleSync(CInPacket* pPacket);
