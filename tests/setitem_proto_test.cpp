// SetEquip addon (ticket 18): the client parses the exact bytes the server writes.
// The two hex strings are SetItemPacketsTest.GOLDEN_CATALOG / GOLDEN_PIECES in the SlumsMS server: change both together.
#include "../src/setitem_proto.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<unsigned char> Hex(const std::string& h) {
    std::vector<unsigned char> out;
    for (size_t i = 0; i + 1 < h.size(); i += 2) out.push_back(static_cast<unsigned char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return out;
}

void Check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

const char* kCatalog = "273701030100020000000b00476f64646573732053657402000200475d10000d00476f64646573732053686f6573"
                       "788310001100476f646465737320577269737462616e640100020002000a00696e63416c6c53746174030000000600696e634d48502c010000";
const char* kPieces = "2737020100020000000200475d100078831000030000000300000003000000030000002c01000000000000";

// Body after opcode and type byte, as setitem.cpp hands it to the parser.
setitem::Reader Body(const std::vector<unsigned char>& bytes, unsigned char type) {
    Check(bytes.size() >= 3 && bytes[0] == 0x27 && bytes[1] == 0x37 && bytes[2] == type, "opcode 0x3727 and type");
    return setitem::Reader(bytes.data() + 3, bytes.size() - 3);
}

}  // namespace

int main() {
    const auto cat = Hex(kCatalog);
    {
        auto r = Body(cat, setitem::kSyncCatalog);
        unsigned flags = 0;
        std::vector<setitem::Def> defs;
        Check(setitem::ParseCatalogPart(r, flags, defs), "catalog parses");
        Check(flags == (setitem::kFlagFirst | setitem::kFlagLast), "single part is first and last");
        Check(defs.size() == 1, "one set");
        const auto& d = defs[0];
        Check(d.id == 2 && d.name == "Goddess Set" && d.complete == 2, "set header");
        Check(d.items.size() == 2 && d.items[0].itemId == 1072455 && d.items[0].name == "Goddess Shoes"
              && d.items[1].itemId == 1082232 && d.items[1].name == "Goddess Wristband", "items");
        Check(d.tiers.size() == 1 && d.tiers[0].pieces == 2 && d.tiers[0].stats.size() == 2, "tier");
        Check(d.tiers[0].stats[0].first == "incAllStat" && d.tiers[0].stats[0].second == 3, "AllStat 3");
        Check(d.tiers[0].stats[1].first == "incMHP" && d.tiers[0].stats[1].second == 300, "MHP 300");
    }
    // Every truncation is rejected whole, never half-applied.
    for (size_t cut = 3; cut < cat.size(); ++cut) {
        std::vector<unsigned char> part(cat.begin(), cat.begin() + cut);
        auto r = Body(part, setitem::kSyncCatalog);
        unsigned flags = 0;
        std::vector<setitem::Def> defs;
        Check(!setitem::ParseCatalogPart(r, flags, defs) && defs.empty(), "truncated catalog rejected");
    }
    {
        auto extra = cat;
        extra.push_back(0);
        auto r = Body(extra, setitem::kSyncCatalog);
        unsigned flags = 0;
        std::vector<setitem::Def> defs;
        Check(!setitem::ParseCatalogPart(r, flags, defs), "trailing byte rejected");
    }

    const auto pc = Hex(kPieces);
    {
        auto r = Body(pc, setitem::kSyncPieces);
        std::vector<setitem::Worn> worn;
        setitem::Bonus b;
        Check(setitem::ParsePieces(r, worn, b), "pieces parse");
        Check(worn.size() == 1 && worn[0].setId == 2 && worn[0].items == std::vector<int>{1072455, 1082232}, "worn");
        Check(b.str == 3 && b.dex == 3 && b.int_ == 3 && b.luk == 3 && b.maxHp == 300 && b.maxMp == 0, "folded bonus");
    }
    for (size_t cut = 3; cut < pc.size(); ++cut) {
        std::vector<unsigned char> part(pc.begin(), pc.begin() + cut);
        auto r = Body(part, setitem::kSyncPieces);
        std::vector<setitem::Worn> worn;
        setitem::Bonus b{7, 7, 7, 7, 7, 7};
        Check(!setitem::ParsePieces(r, worn, b) && b.str == 7, "truncated pieces rejected, bonus kept");
    }

    Check(setitem::StatLabel("incAllStat") == "All Stats" && setitem::StatLabel("incPAD") == "Weapon ATT"
          && setitem::StatLabel("incFoo") == "Foo", "labels");
    std::cout << "PASS: SetEquip catalog and pieces packets decode the server's golden bytes; truncations rejected\n";
}
