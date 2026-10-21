#pragma once

// ============================================================
// setitem.h — SetEquip addon (ticket 18, MAPLEROOT_SET_EQUIP): equipment-set tooltip and Ability-window bonus.
//
// The server owns everything (server/setitem in SlumsMS): which items form a set, the tiers, and the bonus it
// adds to the character. This client half only shows it:
//   * setitem.cpp       asks for the catalog and the worn pieces, and keeps the last answers;
//   * tooltip.cpp       adds "SET EFFECT / PIECES EQUIPPED" rows to the equip tooltip and a second, docked
//                       tooltip listing the pieces and tiers (green = worn / active, grey = not);
//   * setstatbonus.cpp  adds the server's STR/DEX/INT/LUK/MaxHP/MaxMP set bonus to the client's own cached
//                       totals so the Ability window (and the client's damage math) match the server.
// Same opcode split as cashshopwnd.h: the 0x372x block reserves SET_ITEM 0x3726/7.
// ============================================================

#include "setitem_proto.h"

class CInPacket;

static constexpr unsigned short kSetItemActionOpcode = 0x3726;   // client -> server, RecvOpcode.SET_ITEM_ACTION
static constexpr unsigned short kSetItemSyncOpcode   = 0x3727;   // server -> client, SendOpcode.SET_ITEM_SYNC

// Routed from packetdispatcher.cpp.
void SetItem_HandleSync(CInPacket* pPacket);

// Per-frame driver from CWvsApp::CallUpdate_hook (bypass.cpp): requests the catalog until it arrives, polls the
// worn pieces, re-validates the stat cache when the bonus changed, and hides an orphaned set tooltip.
void SetItem_Tick();

// Copies the set `itemId` belongs to and the item ids of it the player wears. False if none / no catalog yet.
bool SetItem_Find(int itemId, setitem::Def& def, std::vector<int>& worn);

// The server's folded set bonus (zero when none).
setitem::Bonus SetItem_GetBonus();

// tooltip.cpp: hide an orphaned set tooltip; release its layer and fonts at CWvsApp::CleanUp.
void SetEffectToolTip_Tick();
void SetEffectToolTip_Shutdown();

// setstatbonus.cpp, attached from AttachToolTipMod.
void AttachSetStatBonusMod();
void SetStatBonus_Revalidate();
