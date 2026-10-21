#pragma once

// ============================================================
// monsterbook.h -- shared surface of the Monster Book addon (MAPLEROOT_MONSTER_BOOK, ticket 19).
//
// Four modules, ported from the "MonsterBook Publish" package (client-kentakae), each owning its own hooks:
//   monsterbook.cpp          open the book up (colour icons, 0-counters, art/HP/MP, all four tabs),
//                            the card-level dual key, and the book-data redirect below
//   monsterbook_foundin.cpp  Found In row click -> world map; Found In / Episode text styling
//   monsterbook_drops.cpp    live drop % under each Dropping icon (MONSTER_BOOK_RESULT type 0)
//   monsterbook_search.cpp   mob-name search (local) and item-name search (types 1 / 2)
//
// The package's design notes, addresses and hard-won client facts are kept in the module headers;
// docs/monsterbook.md is the SlumsMS-side summary (opcodes, data pipeline).
// ============================================================

#include "wvs/packet.h"
#include "ztl/ztl.h"
#include <string>

#ifdef MAPLEROOT_MONSTER_BOOK
#define USE_MONSTER_BOOK_OPEN    TRUE
#define USE_MONSTER_BOOK_FOUNDIN TRUE
#define USE_MONSTER_BOOK_DROPS   TRUE
#define USE_MONSTER_BOOK_SEARCH  TRUE
#else
#define USE_MONSTER_BOOK_OPEN    FALSE
#define USE_MONSTER_BOOK_FOUNDIN FALSE
#define USE_MONSTER_BOOK_DROPS   FALSE
#define USE_MONSTER_BOOK_SEARCH  FALSE
#endif

// The package's per-hook breadcrumb. This client has no breadcrumb log, so it compiles away.
#define MUSH_FEATURE(NAME) ((void)0)
// Likewise the package's crash-attribution registry and crash-loop quarantine: this client has neither.
#define REGISTER_CODECAVE(FN, NAME) ((void)0)
inline void MushRegisterCode(uintptr_t, const char*) {
}
inline bool MushFeatureQuarantined(const char*) {
    return false;
}

// Custom opcode pair, claimed in /root/work/addon-opcodes.md. The package shipped 0x372B / 0x372C, but 0x372C
// is Beauty's (see cashshopwnd.h); this is the next free pair after DAILY_CHECKIN 0x3732 / 0x3733.
// Server halves: net.opcodes.RecvOpcode.MONSTER_BOOK_QUERY / SendOpcode.MONSTER_BOOK_RESULT.
constexpr unsigned short kMonsterBookQueryOpcode  = 0x3734; // client -> server
constexpr unsigned short kMonsterBookResultOpcode = 0x3735; // server -> client

// The image the book reads its per-mob map / reward / episode lists from. "String/MonsterBookEx.img" -- the
// server-generated book that tools/wz/add_monster_book.py bakes into Custom.wz -- when Custom.wz carries it,
// else the stock "String/MonsterBook.img". Decided once, just before CMonsterBookMan::LoadBook runs.
const wchar_t* MonsterBook_BookImg();

// Helpers the package took from Kentakae's util / mob-name headers.
Ztl_variant_t get_object_or_empty(const wchar_t* sUOL);
Ztl_variant_t get_item_or_empty(IWzPropertyPtr& pProperty, const wchar_t* sName);
std::string GetMobNameById(int nMobId); // "Unknown" when String/Mob.img has no name

// Swaps the decoded text of one StringPool entry while it is alive and puts the original back on exit. The
// client caches each decoded entry for the session, so this swaps the cache slot, not the encoded string.
class ScopedStringPoolText {
public:
    ScopedStringPoolText(unsigned nIdx, const char* sText);
    ~ScopedStringPoolText();
    ScopedStringPoolText(const ScopedStringPoolText&) = delete;
    ScopedStringPoolText& operator=(const ScopedStringPoolText&) = delete;

private:
    void** m_pSlot = nullptr;
    void* m_pSaved = nullptr;
};

void AttachMonsterBookMod();
void AttachMonsterBookFoundInMod();
void AttachMonsterBookDropsMod();
void AttachMonsterBookSearchMod();
void MonsterBookDrops_OnClientTick();
void MonsterBookDrops_OnPacket(CInPacket* pPacket);
void MonsterBookSearch_OnClientTick();
void MonsterBookSearch_OnPacket(CInPacket* pPacket);
void MonsterBookSearch_SetItemResultView(bool bOn);
bool MonsterBookSearch_IsItemResultView();

// One call for the dispatcher / tick so neither has to know the module split.
void MonsterBook_HandleResult(CInPacket* pPacket);
void MonsterBook_Tick();
