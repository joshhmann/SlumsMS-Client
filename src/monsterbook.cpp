#include "pch.h"
#include "constants.h"
#include "hook.h"
#include "monsterbook.h"
#include "wvs/util.h"
#include <cstring>
#include <cwchar>

// ===========================================================================
// CUIMonsterBook -- open the Monster Book up.
//
// Stock v83 rations the book out as the player collects cards: un-owned cards
// are drawn greyed and translucent with no counter, the four right-hand tabs
// unlock one at a time, and the Basic Info page hides the mob art and prints
// "HP : ???" until enough cards are in. Mush wants the whole book legible from
// the first login -- every icon in colour, every card carrying a count that
// starts at 0, and all four tabs always readable.
//
// The gates are five spots in the exe, all of them a single branch, so this is
// five surgical writes plus one detour rather than a re-implementation.
//
//   CUIMonsterBook::Draw  -- card grid ----------------------------------
//   0x00864959  jne 0x00864C23   after sub_95FC65(cardId) (card level, 0 = not
//                                owned). Taken  -> full-colour icon, alpha 0xFF.
//                                Not taken -> sub_98DD63 greyscale + alpha 0x55.
//                                => forced unconditional: never grey, never faded.
//   0x00864E5A  je  0x0086509A   "level == 0, skip the counter". NOP'd, so the
//                                UI/Basic.img/ItemNo digits also render the 0.
//                                (5/5 still takes the fullMark branch above it,
//                                 guarded by its own sub_95FCB7 test -- untouched.)
//
//   CUIMonsterBook::Draw  -- Basic Info page -----------------------------
//   0x00865F9C  jle 0x0086651C   "level <= 0, don't draw the mob art". NOP'd.
//   0x00866551  jle 0x00866581   "level <= 1, keep HP : ??? / MP : ???". NOP'd,
//                                so the real numbers print. They come from
//                                CMonsterBookMan's record, which LoadBook fills
//                                from Mob/%07d.img info/maxHP (or info/defaultHP)
//                                -- ONLY for mob ids present in
//                                String/MonsterBook.img. A mob missing there has
//                                no record at all, GetInfo returns NULL, the
//                                setter bails at 0x0086721A and the previous
//                                mob's strings stay put -> "HP : (null)". That
//                                half is a DATA fix (Cosmic tools/monsterbook_gen
//                                writes an entry for all 860 card mobs); without
//                                it this NOP would just print (null) sooner.
//
//   CUIMonsterBook::SetTabEnable(int nCardLevel) @ 0x00866B2D -------------
//   Enables tab i by comparing the level: 0 (Basic Info) > 0, 1 (Episode) > 2,
//   2 (Dropping) > 3, 3 (Found In) > 4, and always disables tab 4. Detoured to
//   call the ORIGINAL with a level of 5 -- that satisfies all four comparisons
//   and keeps tab 4 off, so no calling convention is re-derived here.
// ===========================================================================

namespace {

constexpr uintptr_t kCardIconOwnedBranch = 0x00864959; // jne  -> full-colour icon path
constexpr uintptr_t kCardCountSkip       = 0x00864E5A; // je   -> skip the counter digits
constexpr uintptr_t kBasicInfoArtGate    = 0x00865F9C; // jle  -> skip the mob art
constexpr uintptr_t kBasicInfoHpMpGate   = 0x00866551; // jle  -> keep "HP : ???"

class CUIMonsterBook {
public:
    MEMBER_HOOK(void, 0x00866B2D, SetTabEnable, int nCardLevel)
};

// Every tab this UI has art for. 5 clears all four "level > N" tests in the
// original (N = 0, 2, 3, 4) while leaving the unused fifth tab disabled.
constexpr int kUnlockAllTabs = 5;

// ---------------------------------------------------------------------------
// Card counter: the login snapshot and the live pickup disagree about the key.
//
// The book's grid asks for a level with the FULL card id -- CUIMonsterBook::Draw
// pulls it out of the card record CMonsterBookMan::LoadCardList built from the
// Item/Consume/0238.img node name (0x00684A31 atoi -> record+0), then calls
// GetCardLevel at 0x0086494E.
//
// MONSTER_BOOK_SET_CARD (a pickup) carries that same full id as an int, so cards
// collected in this session answer correctly. The CHARSTATS login snapshot does
// not: it writes `cardid % 10000` into a short, so every card the character
// already owned is filed under 3024 while the grid looks up 2383024 -- the
// lookup misses and the level reads 0. Stock v83 hid the "0" (the counter draw
// was gated on level != 0, the branch NOP'd above), which is why this only
// became visible once every card started showing a number.
//
// Fixed here rather than on the wire because the snapshot field is a short and
// cannot carry the full id. Taking the best of both keys is correct whichever
// convention the client used to file the entry, and the two key spaces cannot
// collide: card ids run 2380000..2388999, so `% 10000` is injective over them.
// ---------------------------------------------------------------------------
using CardLevelFn = int(__cdecl*)(int nCardId);

auto GetCardLevel_Orig = reinterpret_cast<CardLevelFn>(0x0095FC65);
auto IsCardComplete_Orig = reinterpret_cast<CardLevelFn>(0x0095FCB7);

constexpr int kCardIdBase = 2380000;

int __cdecl GetCardLevel_hook(int nCardId) {
    int nLevel = GetCardLevel_Orig(nCardId);
    if (nLevel <= 0 && nCardId >= kCardIdBase) {
        nLevel = GetCardLevel_Orig(nCardId % 10000);
    }
    return nLevel;
}

int __cdecl IsCardComplete_hook(int nCardId) {
    int bComplete = IsCardComplete_Orig(nCardId);
    if (!bComplete && nCardId >= kCardIdBase) {
        bComplete = IsCardComplete_Orig(nCardId % 10000);
    }
    return bComplete;
}

} // namespace

// ---------------------------------------------------------------------------
// Shared flag between the search module and the drop-% module.
//
// Both paint the RIGHT page: the item search renders plain item icons there with
// NO percentages, while the Dropping tab renders the same icons WITH them. They
// must never paint at once. The flag lives HERE, in the base module, rather than
// in either of them, so neither has to link against the other and the two can be
// built in any order.
// ---------------------------------------------------------------------------
namespace {
bool g_itemResultView = false;
}

void MonsterBookSearch_SetItemResultView(bool bOn) {
    g_itemResultView = bOn;
}

bool MonsterBookSearch_IsItemResultView() {
    return g_itemResultView;
}

void CUIMonsterBook::SetTabEnable_hook(int /*nCardLevel*/) {
    MUSH_FEATURE("monsterBook:SetTabEnable");
    CUIMonsterBook::SetTabEnable(this, kUnlockAllTabs);
}

// ---------------------------------------------------------------------------
// Book data redirect.
//
// CMonsterBookMan::LoadBook (0x0068487C) reads every per-mob map / reward / episode list from
// StringPool[0x8BF] = "String/MonsterBook.img", and nothing else in the client uses that entry (one
// `push 0x8BF` in .text, at 0x00684F71). Custom.wz cannot add or remove sub-folders of a stock image,
// so the server-generated book ships as a NEW image, String/MonsterBookEx.img, and the loader is
// pointed at it here -- only when it really resolves, so a Custom.wz without it keeps the stock book.
// LoadBook runs once, from CWvsApp's init (bypass.cpp), before anything has decoded entry 0x8BF, so
// the client's decode cache cannot hold the stock path yet.
// ---------------------------------------------------------------------------
void EncodeString(int nIdx, const char* sSource, char* sDestination); // stringpool.cpp

namespace {

constexpr unsigned kSpMonsterBookImg = 0x8BF;
constexpr char kBookImgEx[] = "String/MonsterBookEx.img";
const wchar_t* g_bookImg = L"String/MonsterBook.img";

using LoadBookFn = int(__thiscall*)(void* pThis);
auto CMonsterBookMan_LoadBook = reinterpret_cast<LoadBookFn>(0x0068487C);

int __fastcall CMonsterBookMan_LoadBook_hook(void* pThis, void* /*edx*/) {
    MUSH_FEATURE("monsterBook:LoadBook");
    try {
        Ztl_variant_t v = get_object_or_empty(L"String/MonsterBookEx.img");
        IUnknownPtr pUnk = v.vt == VT_UNKNOWN || v.vt == VT_DISPATCH ? v.GetUnknown(false, false) : nullptr;
        IWzPropertyPtr pBook;
        if (pUnk && SUCCEEDED(pUnk->QueryInterface(&pBook)) && pBook) {
            static char sEncoded[sizeof(kBookImgEx) + 2];
            EncodeString(kSpMonsterBookImg, kBookImgEx, sEncoded);
            g_bookImg = L"String/MonsterBookEx.img";
            DebugMessage("[monsterBook] book data: String/MonsterBookEx.img (Custom.wz)");
        } else {
            DebugMessage("[monsterBook] book data: stock String/MonsterBook.img");
        }
    } catch (...) {
    }
    return CMonsterBookMan_LoadBook(pThis);
}

// Writes `patch` over `expected` only when the bytes really are `expected`; a client this was not built
// for keeps its stock look instead of being patched blind.
bool PatchVerified(uintptr_t address, const unsigned char* expected, const unsigned char* patch, size_t size) {
    if (memcmp(reinterpret_cast<const void*>(address), expected, size) != 0) {
        DebugMessage("[monsterBook] signature mismatch at 0x%08X -- patch skipped", static_cast<unsigned>(address));
        return false;
    }
    PatchMemory(reinterpret_cast<void*>(address), const_cast<unsigned char*>(patch), size);
    return true;
}

} // namespace

const wchar_t* MonsterBook_BookImg() {
    return g_bookImg;
}

Ztl_variant_t get_object_or_empty(const wchar_t* sUOL) {
    try {
        if (sUOL && get_rm()) {
            return get_rm()->GetObjectA(sUOL);
        }
    } catch (...) {
    }
    return Ztl_variant_t();
}

Ztl_variant_t get_item_or_empty(IWzPropertyPtr& pProperty, const wchar_t* sName) {
    try {
        if (pProperty && sName) {
            return pProperty->item[sName];
        }
    } catch (...) {
    }
    return Ztl_variant_t();
}

std::string GetMobNameById(int nMobId) {
    wchar_t sPath[64];
    _snwprintf_s(sPath, _countof(sPath), _TRUNCATE, L"String/Mob.img/%d/name", nMobId);
    Ztl_variant_t v = get_object_or_empty(sPath);
    if (v.vt == VT_BSTR && V_BSTR(&v)) {
        char sName[256] = {};
        WideCharToMultiByte(CP_ACP, 0, V_BSTR(&v), -1, sName, sizeof(sName) - 1, nullptr, nullptr);
        if (sName[0]) {
            return sName;
        }
    }
    return "Unknown";
}

// StringPool::GetString (0x0079E993 via 0x00406455) decodes an entry once and keeps the ZXString<char> it
// built in the pool's slot array (pool+0) for the session, so changing the encoded entry after its first
// use changes nothing. A swap therefore exchanges the CACHE slot: the first swap decodes our text once
// (by pointing the encoded entry at it with the slot empty), keeps that decoded object for every later
// swap, and every swap puts the original slot back on exit. No allocation is ever freed or leaked twice.
ScopedStringPoolText::ScopedStringPoolText(unsigned nIdx, const char* sText) {
    struct Cached {
        unsigned idx;
        void* decoded;
    };
    static Cached s_cache[4] = {};
    try {
        void* pPool = reinterpret_cast<void*(__cdecl*)()>(0x0079E805)();
        void** aSlots = pPool ? *reinterpret_cast<void***>(pPool) : nullptr;
        if (!aSlots || !sText) {
            return;
        }
        Cached* pEntry = nullptr;
        for (Cached& c : s_cache) {
            if (c.decoded && c.idx == nIdx) {
                pEntry = &c;
                break;
            }
        }
        if (!pEntry) {
            Cached* pFree = nullptr;
            for (Cached& c : s_cache) {
                if (!c.decoded) {
                    pFree = &c;
                    break;
                }
            }
            if (!pFree) {
                return;
            }
            auto ms_aString = reinterpret_cast<const char**>(0x00BDC9D4);
            const char* pStockEncoded = ms_aString[nIdx];
            void* pStockSlot = aSlots[nIdx];
            char* sEncoded = new char[strlen(sText) + 2];   // kept: the decode reads it lazily
            EncodeString(static_cast<int>(nIdx), sText, sEncoded);
            aSlots[nIdx] = nullptr;
            {
                ZXString<char> sOut;
                reinterpret_cast<void*(__thiscall*)(void*, ZXString<char>*, unsigned)>(0x00406455)(pPool, &sOut, nIdx);
            }
            pFree->idx = nIdx;
            pFree->decoded = aSlots[nIdx];
            aSlots[nIdx] = pStockSlot;
            ms_aString[nIdx] = pStockEncoded;
            if (!pFree->decoded) {
                return;
            }
            pEntry = pFree;
        }
        m_pSlot = &aSlots[nIdx];
        m_pSaved = *m_pSlot;
        *m_pSlot = pEntry->decoded;
    } catch (...) {
        m_pSlot = nullptr;
    }
}

ScopedStringPoolText::~ScopedStringPoolText() {
    if (m_pSlot) {
        *m_pSlot = m_pSaved;
    }
}

void MonsterBook_HandleResult(CInPacket* pPacket) {
    // Both readers walk the same packet without consuming it and ignore the subtypes that are not theirs.
    try {
        MonsterBookDrops_OnPacket(pPacket);
    } catch (...) {
    }
    try {
        MonsterBookSearch_OnPacket(pPacket);
    } catch (...) {
    }
}

void MonsterBook_Tick() {
    // Main-thread flush: queued queries are sent and views rebuilt here, never on a packet or draw path.
    try {
        MonsterBookDrops_OnClientTick();
    } catch (...) {
    }
    try {
        MonsterBookSearch_OnClientTick();
    } catch (...) {
    }
}

void AttachMonsterBookMod() {
#if USE_MONSTER_BOOK_OPEN
    ATTACH_HOOK(CMonsterBookMan_LoadBook, CMonsterBookMan_LoadBook_hook);

    // jne rel32 (0F 85 C4 02 00 00) -> jmp rel32 to the same target + one NOP: every icon full colour.
    {
        const unsigned char expected[] = { 0x0F, 0x85, 0xC4, 0x02, 0x00, 0x00 };
        const unsigned char patch[] = { 0xE9, 0xC5, 0x02, 0x00, 0x00, 0x90 };
        static_assert(0x00864C23 - (kCardIconOwnedBranch + 5) == 0x2C5, "jmp target");
        PatchVerified(kCardIconOwnedBranch, expected, patch, sizeof(patch));
    }
    {
        const unsigned char expected[] = { 0x0F, 0x84, 0x3A, 0x02, 0x00, 0x00 }; // je  0x0086509A
        const unsigned char nops[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
        PatchVerified(kCardCountSkip, expected, nops, sizeof(nops));
    }
    {
        const unsigned char expected[] = { 0x0F, 0x8E, 0x7A, 0x05, 0x00, 0x00 }; // jle 0x0086651C
        const unsigned char nops[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
        PatchVerified(kBasicInfoArtGate, expected, nops, sizeof(nops));
    }
    {
        const unsigned char expected[] = { 0x7E, 0x2E };                          // jle 0x00866581
        const unsigned char nops[] = { 0x90, 0x90 };
        PatchVerified(kBasicInfoHpMpGate, expected, nops, sizeof(nops));
    }

    ATTACH_HOOK(CUIMonsterBook::SetTabEnable, CUIMonsterBook::SetTabEnable_hook);

    // must follow the counter patch above: it is what makes a wrong level visible
    ATTACH_HOOK(GetCardLevel_Orig, GetCardLevel_hook);
    ATTACH_HOOK(IsCardComplete_Orig, IsCardComplete_hook);
#endif
}
