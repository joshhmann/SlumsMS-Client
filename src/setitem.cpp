// SetEquip addon (ticket 18): networking and state. See setitem.h for the split and setitem_proto.h for the format.
#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "wvs/packet.h"
#include "wvs/field.h"
#include "setitem.h"

#include <mutex>
#include <unordered_map>

namespace {

constexpr uintptr_t kAddr_ClientSocket_Instance   = 0x00BE7914;
constexpr uintptr_t kAddr_ClientSocket_SendPacket = 0x0049637B;
auto ClientSocket_SendPacket = reinterpret_cast<void(__thiscall*)(void*, const COutPacket&)>(kAddr_ClientSocket_SendPacket);

constexpr DWORD kCatalogRetryMs = 5000;   // until the catalog arrives (the first ask can beat the field load)
constexpr DWORD kPiecesPollMs   = 10000;  // safety net; the server also pushes on every equip change

std::mutex g_mtx;
std::vector<setitem::Def> g_catalog;          // complete catalog
std::vector<setitem::Def> g_incoming;         // parts received so far
bool g_loading = false;
bool g_loaded = false;
std::unordered_map<int, size_t> g_byItem;     // item id -> index in g_catalog
std::unordered_map<int, std::vector<int>> g_worn;
setitem::Bonus g_bonus;
bool g_bonusChanged = false;

DWORD g_lastCatalogAsk = 0;
DWORD g_lastPiecesAsk = 0;
bool g_askedPieces = false;

void Send(unsigned char action) {
    void* sock = *reinterpret_cast<void**>(kAddr_ClientSocket_Instance);
    if (!sock) return;
    COutPacket o(kSetItemActionOpcode);
    o.Encode1(action);
    ClientSocket_SendPacket(sock, o);
}

// CInPacket raw view: m_aRecvBuff at +0x8, m_uLength at +0xC, m_uOffset at +0x14 (static_assert'd in packet.h).
bool RawView(CInPacket* p, const unsigned char*& data, size_t& len) {
    auto base = reinterpret_cast<unsigned char*>(p);
    auto buf = *reinterpret_cast<unsigned char**>(base + 0x8);
    auto length = *reinterpret_cast<unsigned short*>(base + 0xC);
    auto offset = *reinterpret_cast<unsigned int*>(base + 0x14);
    if (!buf || offset + 3 > length) return false;   // opcode + type byte
    data = buf + offset + 2;
    len = length - offset - 2;
    return true;
}

}  // namespace

void SetItem_HandleSync(CInPacket* pPacket) {
    const unsigned char* data = nullptr;
    size_t len = 0;
    if (!pPacket || !RawView(pPacket, data, len)) return;
    setitem::Reader r(data, len);
    const unsigned type = r.u8();
    if (type == setitem::kSyncCatalog) {
        unsigned flags = 0;
        std::vector<setitem::Def> part;
        if (!setitem::ParseCatalogPart(r, flags, part)) {
            DebugMessage("setitem: rejected a malformed catalog part (%u bytes)", static_cast<unsigned>(len));
            std::lock_guard<std::mutex> lk(g_mtx);
            g_incoming.clear();
            g_loading = false;
            return;
        }
        std::lock_guard<std::mutex> lk(g_mtx);
        if (flags & setitem::kFlagFirst) {
            g_incoming.clear();
            g_loading = true;
        }
        if (!g_loading) return;   // a middle part without its first part: wait for the next full send
        for (auto& d : part) g_incoming.push_back(std::move(d));
        if (flags & setitem::kFlagLast) {
            g_catalog.swap(g_incoming);
            g_incoming.clear();
            g_byItem.clear();
            for (size_t i = 0; i < g_catalog.size(); ++i) {
                for (const auto& e : g_catalog[i].items) g_byItem[e.itemId] = i;
            }
            g_loading = false;
            g_loaded = true;
            DebugMessage("setitem: catalog of %u sets", static_cast<unsigned>(g_catalog.size()));
        }
    } else if (type == setitem::kSyncPieces) {
        std::vector<setitem::Worn> worn;
        setitem::Bonus bonus;
        if (!setitem::ParsePieces(r, worn, bonus)) {
            DebugMessage("setitem: rejected a malformed pieces packet (%u bytes)", static_cast<unsigned>(len));
            return;
        }
        std::lock_guard<std::mutex> lk(g_mtx);
        g_worn.clear();
        for (auto& w : worn) g_worn[w.setId] = std::move(w.items);
        if (bonus != g_bonus) {
            g_bonus = bonus;
            g_bonusChanged = true;
        }
    }
}

bool SetItem_Find(int itemId, setitem::Def& def, std::vector<int>& worn) {
    std::lock_guard<std::mutex> lk(g_mtx);
    auto it = g_byItem.find(itemId);
    if (it == g_byItem.end()) return false;
    def = g_catalog[it->second];
    auto w = g_worn.find(def.id);
    if (w != g_worn.end()) worn = w->second; else worn.clear();
    return true;
}

setitem::Bonus SetItem_GetBonus() {
    std::lock_guard<std::mutex> lk(g_mtx);
    return g_bonus;
}

void SetItem_Tick() {
    SetEffectToolTip_Tick();
    if (!get_field()) {
        // Login screen, cash shop or between fields: nothing to ask; ask for pieces again on the next field.
        g_askedPieces = false;
        return;
    }
    const DWORD now = GetTickCount();
    bool loaded, changed;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        loaded = g_loaded;
        changed = g_bonusChanged;
        g_bonusChanged = false;
    }
    if (!loaded && now - g_lastCatalogAsk >= kCatalogRetryMs) {
        g_lastCatalogAsk = now;
        Send(setitem::kActionCatalog);
    }
    if (!g_askedPieces || now - g_lastPiecesAsk >= kPiecesPollMs) {
        g_askedPieces = true;
        g_lastPiecesAsk = now;
        Send(setitem::kActionPieces);
    }
    if (changed) {
        SetStatBonus_Revalidate();
    }
}
