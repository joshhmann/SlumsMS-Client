// QA client runner channel (qamodule_policy.h): signals, commands, event lines, capture header.
#include "../src/qamodule_policy.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    using namespace QaPolicy;
    try {
        Check(std::strcmp(SignalForOpcode(0x1B), "worldselect.ready") == 0, "world select after the recommended-world packet");
        Check(std::strcmp(SignalForOpcode(0x03), "channels.ready") == 0, "the channel panel after the world click");
        Check(std::strcmp(SignalForOpcode(0x0B), "charselect.ready") == 0, "character select after the character list");
        Check(std::strcmp(SignalForOpcode(0x7D), "field.ready") == 0, "map ready after SET_FIELD");
        Check(SignalForOpcode(0x0A) == nullptr, "the world list itself is not ready yet (more packets follow)");

        Check(ParseCommand("ping").kind == Command::Ping, "ping");
        Command b = ParseCommand("burst 10 2000 b140-heal");
        Check(b.kind == Command::Burst && b.fps == 10 && b.ms == 2000 && b.label == "b140-heal", "burst parses");
        Check(ParseCommand("burst 0 2000 x").kind == Command::Invalid, "fps 0 refused");
        Check(ParseCommand("burst 31 2000 x").kind == Command::Invalid, "fps over 30 refused");
        Check(ParseCommand("burst 10 50 x").kind == Command::Invalid, "too short refused");
        Check(ParseCommand("burst 10 6000 x").kind == Command::Invalid, "too long refused");
        Check(ParseCommand("burst 10 2000 ../up").kind == Command::Invalid, "no path in a label");
        Check(ParseCommand("burst 10 2000 a b").kind == Command::Invalid, "one label word only");
        Check(ParseCommand("burst 10 2000 " + std::string(41, 'a')).kind == Command::Invalid, "label at most 40");
        Check(ParseCommand("rm -rf").kind == Command::Invalid && !ParseCommand("rm -rf").error.empty(), "unknown command says why");

        Check(EventLine("field.ready", 1234) == "{\"ev\":\"field.ready\",\"t\":1234}\n", "event line");
        Check(EventLine("error", 5, ",\"msg\":\"" + JsonEscape("a\"b\\c\n") + "\"") ==
            "{\"ev\":\"error\",\"t\":5,\"msg\":\"a\\\"b\\\\c\\u000a\"}\n", "escaped text");

        uint8_t h[54];
        BmpHeader(h, 800, 600);
        Check(h[0] == 'B' && h[1] == 'M' && h[10] == 54 && h[14] == 40 && h[28] == 32, "BMP header fields");
        const uint32_t size = h[2] | (h[3] << 8) | (h[4] << 16) | (static_cast<uint32_t>(h[5]) << 24);
        Check(size == 54u + 800u * 600u * 4u, "BMP file size");

        // ---- chat lines: what the server puts on the wire, opcode first, as the client receives it ----
        struct Packet {
            std::vector<unsigned char> b;
            Packet(uint16_t op) : b{static_cast<unsigned char>(op), static_cast<unsigned char>(op >> 8)} {}
            Packet& U8(int v) {
                b.push_back(static_cast<unsigned char>(v));
                return *this;
            }
            Packet& U16(int v) {
                b.push_back(static_cast<unsigned char>(v));
                b.push_back(static_cast<unsigned char>(v >> 8));
                return *this;
            }
            Packet& U32(int v) {
                return U16(v & 0xFFFF).U16((v >> 16) & 0xFFFF);
            }
            Packet& Str(const std::string& s) {
                U16(static_cast<int>(s.size()));
                b.insert(b.end(), s.begin(), s.end());
                return *this;
            }
            ChatLine Chat() { return ParseChatPacket(static_cast<uint16_t>(b[0] | (b[1] << 8)), b.data(), b.size()); }
        };

        {
            Packet p(kOpChatText);
            p.U32(123).U8(0).Str("hello there").U8(0);
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 0 && c.text == "hello there", "map chat carries the line the player sees");
        }
        {
            Packet p(kOpChatText);   // an admin line still arrives as map chat, with the flag set
            p.U32(1).U8(1).Str("gm says").U8(0);
            Check(p.Chat().kind == 1, "an admin chat line is marked");
        }
        {
            Packet p(kOpMultiChat);  // party chat: mode 1, sender, text
            p.U8(1).Str("bot01").Str("joining");
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 1 && c.text == "bot01: joining", "party chat names the sender");
        }
        {
            Packet p(kOpWhisper);    // 0x12 received: from, channel, admin, text
            p.U8(0x12).Str("bot01").U8(5).U8(0).Str("psst");
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 0x12 && c.text == "bot01: psst", "whisper names the sender");
        }
        {
            Packet p(kOpWhisper);    // 0x0A is a send result, not a line the player reads
            p.U8(0x0A).Str("bot01").U8(0);
            Check(!p.Chat().ok, "a whisper send result is not a chat line");
        }
        {
            Packet p(kOpSpouseChat); // mode 5: the spouse named, then text
            p.U8(5).Str("love").U8(0).Str("hi love");
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 5 && c.text == "love: hi love", "spouse chat names the spouse");
        }
        {
            Packet p(kOpServerMessage);
            p.U8(1).Str("Server will restart at 20:00");
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 1 && c.text == "Server will restart at 20:00", "the notice popup is a chat line");
        }
        {
            Packet p(kOpServerMessage);   // type 4 ticker: the flag byte before the text, sized 6 + text length
            p.U8(4).U8(1).Str("ticker");
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 4 && c.text == "ticker", "the scrolling ticker is a chat line");
        }
        {
            Packet p(kOpServerMessage);   // a plain type-4 notice has no flag byte
            p.U8(4).Str("no flag");
            const ChatLine c = p.Chat();
            Check(c.ok && c.text == "no flag", "a type-4 notice without the ticker flag");
        }
        {
            Packet p(kOpYellowTip);       // what a GM command replies with
            p.U8(0xFF).Str("Spawned 1 BISHOP TrainingBot ... cids [123]").U16(0);
            const ChatLine c = p.Chat();
            Check(c.ok && c.kind == 0xFF && c.text == "Spawned 1 BISHOP TrainingBot ... cids [123]", "the yellow tip is a chat line");
        }
        {
            Packet p(0x7D);               // SET_FIELD is not chat
            p.U32(0).U32(0).U8(0).U32(100000000).U8(0).U16(100);
            Check(!p.Chat().ok, "a non-chat packet yields nothing");
        }
        {
            Packet p(kOpChatText);        // a cut-off line must not be reported as text
            p.U32(1).U8(0).U16(50).b.insert(p.b.end(), {'a', 'b'});
            Check(!p.Chat().ok, "a short payload is refused");
        }

        // Text the runner logs is cut to 200 BYTES without splitting a UTF-8 sequence.
        Check(TruncateUtf8(std::string(150, 'a'), 200) == std::string(150, 'a'), "short text kept whole");
        Check(TruncateUtf8(std::string(300, 'a'), 200).size() == 200, "long text cut to 200 bytes");
        Check(TruncateUtf8(std::string(199, 'a') + "\xC3\xA9" + "b", 200) == std::string(199, 'a'), "no half a character at the cut");
        {
            Packet p(kOpChatText);
            p.U32(1).U8(0).Str(std::string(300, 'x')).U8(0);
            Check(p.Chat().text.size() == 200, "a long line arrives cut to 200 bytes");
        }

        // ---- windows: named by class, never by position ----
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/Item")) == "item", "item window");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/Equip")) == "equip", "equip window");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/Skill")) == "skill", "skill window");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/MonsterBook")) == "monster_book", "monster book");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/WorldMap")) == "world_map", "world map");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/TradingRoom")) == "trade", "trade window");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/Trunk")) == "storage", "storage");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/EntrustedShop")) == "storage", "entrusted shop is storage");
        Check(std::string(WindowNameFromUol("UI/CashShop.img/Base")) == "cash_shop", "the cash shop draws from its own fill");
        Check(std::string(WindowNameFromUol("UI/Login.img/NewChar/character")) == "character_create", "character create screen");
        Check(WindowNameFromUol("UI/UIWindow.img/KeyConfig") == nullptr, "an unnamed window stays unnamed");
        Check(WindowNameFromUol("") == nullptr, "an empty UOL names nothing");
        Check(std::string(WindowNameFromUol("ui/uiwindow.img/item")) == "item", "UOL matching ignores case");

        Check(std::string(DialogForPacket(kOpPartyOperation, std::vector<unsigned char>{0x3E, 0x00, 4}.data(), 3)) == "party_invite",
            "party invite dialog");
        Check(DialogForPacket(kOpPartyOperation, std::vector<unsigned char>{0x3E, 0x00, 3}.data(), 3) == nullptr,
            "other party operations open no dialog");
        Check(std::string(DialogForPacket(kOpMarriageRequest, std::vector<unsigned char>{0x48, 0x00, 0}.data(), 3)) == "proposal",
            "proposal dialog");
        Check(std::string(DialogForPacket(kOpNpcTalk, std::vector<unsigned char>{0x30, 0x01, 0}.data(), 3)) == "npc_dialog",
            "npc dialog");
        Check(std::string(DialogForPacket(kOpServerMessage, std::vector<unsigned char>{0x44, 0x00, 1}.data(), 3)) == "notice",
            "the notice popup");
        Check(DialogForPacket(kOpServerMessage, std::vector<unsigned char>{0x44, 0x00, 4}.data(), 3) == nullptr,
            "a ticker notice opens no popup");

        Check(std::string(WindowNameForVtable(0x00B39FE0)) == "monster_book", "monster book by vtable");
        Check(WindowNameForVtable(0x00B39114) == nullptr, "an unknown vtable names nothing");

        std::cout << "qa module policy ok\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
