// Coloring Prism (MAPLEROOT_COLOR_PRISM): the client decodes the exact bytes the server writes.
// The hex strings are ColorPrismPacketsTest's golden packets in the SlumsMS server: change both together.
#include "../src/weapontint_proto.h"
#include <iostream>
#include <map>
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

// Snapshot: a Cash weapon at Tone 120 / Chroma +50 / Brightness -20, its effect layer at Tone 0, hair at Tone 359.
const char* kSnapshot = "2f37010370f8190087ff32ecf08eb200ffff00000100000098fe9c64";
// Remote table: "Pinky" (weapon + face), "Abcd" (no items).
const char* kRemote = "2f370302050050696e6b790270f8190087ff32ec0200000038ff0a0004004162636400";
// Result: not tinted.
const char* kResult = "2f370204";

struct Cursor {
    const std::vector<unsigned char>& b;
    size_t at;
    unsigned char U8() { Check(at < b.size(), "truncated"); return b[at++]; }
    unsigned U16() { unsigned v = U8(); return v | (U8() << 8); }
    std::map<int, WeaponTint> Entries(int n) {
        std::map<int, WeaponTint> out;
        for (int i = 0; i < n; ++i) {
            Check(at + kTintEntryBytes <= b.size(), "entry truncated");
            int key = 0;
            WeaponTint t;
            if (WeaponTint_DecodeEntry(&b[at], key, t)) out[key] = t;
            at += kTintEntryBytes;
        }
        return out;
    }
};

Cursor Body(const std::vector<unsigned char>& bytes, unsigned char type) {
    Check(bytes.size() >= 3 && bytes[0] == 0x2F && bytes[1] == 0x37 && bytes[2] == type, "opcode 0x372F and type");
    return Cursor{bytes, 3};
}

}  // namespace

int main() {
    try {
        Check(kWeaponTintActionOpcode == 0x372E && kWeaponTintSyncOpcode == 0x372F, "opcodes");

        // Encoding: Tone d is -(d + 1); 0/0/0 is identity.
        Check(WeaponTint::EncodeAbsoluteHue(0) == -1 && WeaponTint::EncodeAbsoluteHue(359) == -360, "hue encode");
        Check(WeaponTint::EncodeAbsoluteHue(120) == -121, "hue 120");
        WeaponTint red;
        red.hue = -1;
        Check(red.IsHueEncodingValid() && red.HueDegrees() == 0 && !red.IsIdentity(), "Tone 0 is a colour, not identity");

        {
            const auto bytes = Hex(kSnapshot);
            auto c = Body(bytes, kTintSync_Snapshot);
            const auto m = c.Entries(c.U8());
            Check(c.at == bytes.size(), "snapshot fully consumed");
            Check(m.size() == 3, "three tints");
            const WeaponTint& w = m.at(1702000);
            Check(w.hue == -121 && w.HueDegrees() == 120 && w.chroma == 50 && w.bright == -20, "weapon tint survives decode");
            Check(m.at(EffectTintKeyFor(1702000)).HueDegrees() == 0, "effect key, Tone 0");
            const WeaponTint& hair = m.at(kTintKey_Hair);
            Check(hair.HueDegrees() == 359 && hair.chroma == -100 && hair.bright == 100, "hair extremes");
        }
        {
            const auto bytes = Hex(kRemote);
            auto c = Body(bytes, kTintSync_RemoteTable);
            const int players = c.U8();
            Check(players == 2, "two players");
            std::map<std::string, std::map<int, WeaponTint>> table;
            for (int i = 0; i < players; ++i) {
                const unsigned len = c.U16();
                Check(len >= 1 && len <= 13, "name length");
                std::string name(reinterpret_cast<const char*>(&bytes[c.at]), len);
                c.at += len;
                table[name] = c.Entries(c.U8());
            }
            Check(c.at == bytes.size(), "table fully consumed");
            Check(table.at("Pinky").at(1702000).HueDegrees() == 120, "remote weapon");
            Check(table.at("Pinky").at(kTintKey_Face).HueDegrees() == 199 && table.at("Pinky").at(kTintKey_Face).chroma == 10, "remote face");
            Check(table.at("Abcd").empty(), "untinted player");
        }
        {
            const auto bytes = Hex(kResult);
            auto c = Body(bytes, kTintSync_Result);
            Check(c.U8() == kTintResult_NotTinted, "result code 4 is NotTinted");
            Check(kTintResult_Ok == 1 && kTintResult_NoCashWeapon == 2 && kTintResult_NoItem == 3 && kTintResult_Failed == 5, "result codes");
        }
        {
            // Rejected entries: an old relative hue (+120), identity, hue -361, key 0. All are skipped, never zeroed.
            const unsigned char rel[8] = {0x70, 0xF8, 0x19, 0x00, 0x78, 0x00, 0x32, 0x00};
            const unsigned char ident[8] = {0x70, 0xF8, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00};
            const unsigned char low[8] = {0x70, 0xF8, 0x19, 0x00, 0x97, 0xFE, 0x00, 0x00};
            const unsigned char nokey[8] = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};
            int key = 0;
            WeaponTint t;
            Check(!WeaponTint_DecodeEntry(rel, key, t), "relative hue dropped");
            Check(!WeaponTint_DecodeEntry(ident, key, t), "identity dropped");
            Check(!WeaponTint_DecodeEntry(low, key, t), "hue -361 dropped");
            Check(!WeaponTint_DecodeEntry(nokey, key, t), "key 0 dropped");
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "weapontint_proto_test: ok\n";
    return 0;
}
