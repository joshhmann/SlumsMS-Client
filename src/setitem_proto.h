#pragma once

// ============================================================
// setitem_proto.h — wire format of the SetEquip addon (ticket 18), no game headers, so tests/ can build it.
//
// LP SET_ITEM_SYNC 0x3727 (server.setitem.SetItemPackets), after the opcode:
//   byte type
//   type 1 catalog part: byte flags (1 = first part: drop the old catalog, 2 = last part: catalog complete)
//                        short sets, per set: int id, str name, short complete,
//                        short items (int itemId, str itemName)..., short tiers
//                        (short pieces, short stats (str key, int value)...)...
//   type 2 pieces:       short sets, per set: int id, short worn (int itemId)...
//                        then int str, dex, int, luk, maxHp, maxMp (the folded bonus the Ability window adds)
// str = short length + bytes. Every read is bounds-checked; a short or malformed packet is rejected whole.
// ============================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace setitem {

constexpr unsigned char kSyncCatalog = 1;
constexpr unsigned char kSyncPieces = 2;
constexpr unsigned char kFlagFirst = 1;
constexpr unsigned char kFlagLast = 2;
constexpr unsigned char kActionCatalog = 1;
constexpr unsigned char kActionPieces = 2;

struct Tier {
    int pieces = 0;
    std::vector<std::pair<std::string, int>> stats;  // server order
};

struct Entry {
    int itemId = 0;
    std::string name;
};

struct Def {
    int id = 0;
    std::string name;
    int complete = 0;            // pieces for the whole set
    std::vector<Entry> items;    // already filtered to the player's job by the server
    std::vector<Tier> tiers;     // ascending
};

struct Bonus {
    int str = 0, dex = 0, int_ = 0, luk = 0, maxHp = 0, maxMp = 0;
    bool operator==(const Bonus& o) const {
        return str == o.str && dex == o.dex && int_ == o.int_ && luk == o.luk && maxHp == o.maxHp && maxMp == o.maxMp;
    }
    bool operator!=(const Bonus& o) const { return !(*this == o); }
};

struct Worn {
    int setId = 0;
    std::vector<int> items;
};

class Reader {
public:
    Reader(const unsigned char* p, size_t n) : p_(p), n_(n) {}
    bool bad = false;
    size_t left() const { return bad ? 0 : n_ - at_; }

    unsigned int u8() { return static_cast<unsigned int>(take(1)); }
    unsigned int u16() { return static_cast<unsigned int>(take(2)); }
    int i32() { return static_cast<int>(static_cast<uint32_t>(take(4))); }
    std::string str() {
        size_t len = u16();
        if (bad || len > left()) { bad = true; return {}; }
        std::string s(reinterpret_cast<const char*>(p_ + at_), len);
        at_ += len;
        return s;
    }

private:
    uint64_t take(size_t k) {
        if (bad || k > n_ - at_) { bad = true; return 0; }
        uint64_t v = 0;
        for (size_t i = 0; i < k; ++i) v |= static_cast<uint64_t>(p_[at_ + i]) << (8 * i);
        at_ += k;
        return v;
    }
    const unsigned char* p_;
    size_t n_;
    size_t at_ = 0;
};

// Sanity limits: a real catalog part is ~16 KB, so these only stop garbage from allocating wildly.
constexpr unsigned kMaxSetsPerPart = 2000;
constexpr unsigned kMaxItems = 512;
constexpr unsigned kMaxTiers = 64;
constexpr unsigned kMaxStats = 32;

// Body of a catalog part (after the type byte). False (and nothing in `out`) on any error.
inline bool ParseCatalogPart(Reader& r, unsigned& flags, std::vector<Def>& out) {
    out.clear();
    flags = r.u8();
    unsigned sets = r.u16();
    if (r.bad || sets > kMaxSetsPerPart) return false;
    out.reserve(sets);
    for (unsigned s = 0; s < sets; ++s) {
        Def d;
        d.id = r.i32();
        d.name = r.str();
        d.complete = static_cast<int>(r.u16());
        unsigned items = r.u16();
        if (r.bad || items > kMaxItems) { out.clear(); return false; }
        for (unsigned i = 0; i < items; ++i) {
            Entry e;
            e.itemId = r.i32();
            e.name = r.str();
            d.items.push_back(std::move(e));
        }
        unsigned tiers = r.u16();
        if (r.bad || tiers > kMaxTiers) { out.clear(); return false; }
        for (unsigned t = 0; t < tiers; ++t) {
            Tier tier;
            tier.pieces = static_cast<int>(r.u16());
            unsigned stats = r.u16();
            if (r.bad || stats > kMaxStats) { out.clear(); return false; }
            for (unsigned k = 0; k < stats; ++k) {
                std::string key = r.str();
                int value = r.i32();
                tier.stats.emplace_back(std::move(key), value);
            }
            d.tiers.push_back(std::move(tier));
        }
        if (r.bad) { out.clear(); return false; }
        out.push_back(std::move(d));
    }
    if (r.bad || r.left() != 0) { out.clear(); return false; }
    return true;
}

// Body of a pieces packet (after the type byte).
inline bool ParsePieces(Reader& r, std::vector<Worn>& worn, Bonus& bonus) {
    worn.clear();
    unsigned sets = r.u16();
    if (r.bad || sets > kMaxSetsPerPart) return false;
    for (unsigned s = 0; s < sets; ++s) {
        Worn w;
        w.setId = r.i32();
        unsigned n = r.u16();
        if (r.bad || n > kMaxItems) { worn.clear(); return false; }
        for (unsigned i = 0; i < n; ++i) w.items.push_back(r.i32());
        worn.push_back(std::move(w));
    }
    Bonus b;
    b.str = r.i32();
    b.dex = r.i32();
    b.int_ = r.i32();
    b.luk = r.i32();
    b.maxHp = r.i32();
    b.maxMp = r.i32();
    if (r.bad || r.left() != 0) { worn.clear(); return false; }
    bonus = b;
    return true;
}

// "incAllStat" -> "All Stats" etc.; unknown keys lose a leading "inc".
inline std::string StatLabel(const std::string& key) {
    static const std::pair<const char*, const char*> kLabels[] = {
        {"incSTR", "STR"}, {"incDEX", "DEX"}, {"incINT", "INT"}, {"incLUK", "LUK"}, {"incAllStat", "All Stats"},
        {"incMHP", "Max HP"}, {"incMMP", "Max MP"}, {"incPAD", "Weapon ATT"}, {"incMAD", "Magic ATT"},
    };
    for (const auto& l : kLabels) {
        if (key == l.first) return l.second;
    }
    return key.rfind("inc", 0) == 0 ? key.substr(3) : key;
}

}  // namespace setitem
