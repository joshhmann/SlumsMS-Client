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
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/Item/backgrnd")) == "item", "the kept UOL is the background canvas");
        Check(std::string(WindowNameFromUol("UI/UIWindow.img/Equip/backgrnd")) == "equip", "equip background canvas");
        Check(WindowNameFromUol("UI/UIWindow.img/UtilDlgEx/it") == nullptr, "a dialog is not named from its UOL");
        Check(WindowNameFromUol("UI/UIWindow.img/KeyConfig/backgrnd") == nullptr, "settings stay unnamed");

        Check(std::string(DialogForPacket(kOpPartyOperation, std::vector<unsigned char>{0x3E, 0x00, 4}.data(), 3)) == "party_invite",
            "party invite dialog");
        Check(DialogForPacket(kOpPartyOperation, std::vector<unsigned char>{0x3E, 0x00, 3}.data(), 3) == nullptr,
            "other party operations open no dialog");
        Check(std::string(DialogForPacket(kOpMarriageRequest, std::vector<unsigned char>{0x48, 0x00, 0}.data(), 3)) == "proposal",
            "proposal dialog");
        Check(EventLine("hello", 7, "", "2026-11-06T07:00:00.000Z") == "{\"ev\":\"hello\",\"t\":7,\"utc\":\"2026-11-06T07:00:00.000Z\"}\n",
            "event line with utc");
        Check(std::string(ChatKindName(kOpMultiChat, 1)) == "party", "multi-chat mode 1 is party");
        Check(std::string(ChatKindName(kOpWhisper, 0x12)) == "whisper", "whisper kind");
        Check(std::string(ChatKindName(kOpServerMessage, 3)) == "megaphone", "super megaphone");
        Check(std::string(ChatKindName(kOpServerMessage, 5)) == "notice", "pink notice");
        Check(std::string(ChatKindName(kOpYellowTip, 0xFF)) == "system", "gm tip");
        Check(NpcIdForTalkPacket(std::vector<unsigned char>{0x30, 0x01, 4, 0x08, 0xFA, 0x1E, 0x00, 0}.data(), 8) == 2030088,
            "npc id from NPC_TALK");
        Check(NpcIdForTalkPacket(std::vector<unsigned char>{0x30, 0x01, 4}.data(), 3) == -1, "short NPC_TALK has no npc id");
        {
            Command c = ParseCommand("npc_pos 2030008");
            Check(c.kind == Command::NpcPos && c.npc == 2030008, "npc_pos parses");
            Check(ParseCommand("npc_pos 0").kind == Command::Invalid, "npc_pos needs a positive id");
            Check(ParseCommand("npc_pos 5 x").kind == Command::Invalid, "npc_pos takes one argument");
            // spawnNPC: opcode 0x101, oid 0x10, id 2030008, x -969, cy -218, f, fh, rx0, rx1, 1
            std::vector<unsigned char> spawn{0x01, 0x01, 0x10, 0, 0, 0, 0xB8, 0xF9, 0x1E, 0x00, 0x37, 0xFC, 0x26, 0xFF,
                                             1, 0x9F, 0, 0, 0, 0, 0, 1};
            NpcUpdate u = ParseNpcPacket(kOpSpawnNpc, spawn.data(), spawn.size());
            Check(u.op == NpcUpdate::Add && u.oid == 0x10 && u.npc == 2030008 && u.x == -969 && u.y == -218, "spawn npc");
            std::vector<unsigned char> ctl{0x03, 0x01, 1, 0x10, 0, 0, 0, 0xB8, 0xF9, 0x1E, 0x00, 0x37, 0xFC, 0x26, 0xFF};
            u = ParseNpcPacket(kOpSpawnNpcController, ctl.data(), ctl.size());
            Check(u.op == NpcUpdate::Add && u.npc == 2030008 && u.y == -218, "controller spawn");
            std::vector<unsigned char> drop{0x03, 0x01, 0, 0x10, 0, 0, 0};
            Check(ParseNpcPacket(kOpSpawnNpcController, drop.data(), drop.size()).op == NpcUpdate::None,
                "a dropped controller leaves the npc");
            std::vector<unsigned char> rem{0x02, 0x01, 0x10, 0, 0, 0};
            u = ParseNpcPacket(kOpRemoveNpc, rem.data(), rem.size());
            Check(u.op == NpcUpdate::Remove && u.oid == 0x10, "remove npc");
            ScreenPoint p = WorldToScreen(-969, -248, -1000, -300, 800, 600);
            Check(p.x == 431 && p.y == 352 && p.inView, "world to screen");
            Check(!WorldToScreen(1300, 0, 0, 0, 800, 600).inView, "off screen");
            // D1 on 1366x768: Mark of the Squad (x 898) with the camera on the character at 860 is 38 px right of the
            // centre; the worker's 800x600 space puts that at 422, not 721.
            ScreenPoint wide = WorldToScreen(898, 260 - 30, 860, 230, 1366, 768);
            Check(wide.x == 721 && wide.inView, "game pixels on a wide screen");
            wide = ToReference(wide, 1366, 768);
            Check(wide.x == 422 && wide.y == 300 && wide.inView, "scaled to the worker's 800x600");
            Check(ToReference(WorldToScreen(-969, -248, -1000, -300, 800, 600), 800, 600).x == 431, "800x600 unchanged");
        }
        {
            Command c = ParseCommand("click_option 1");
            Check(c.kind == Command::ClickOption && c.option == 1, "click_option parses");
            Check(ParseCommand("click_option 9").kind == Command::Invalid, "click_option is 1-8");
            Check(ParseCommand("click_option 0").kind == Command::Invalid, "click_option starts at 1");
            // D1 (2026-11-06): the receive buffer starts with the socket header; the parsers get the packet from its opcode.
            const std::vector<unsigned char> received{0xAA, 0xBB, 0xCC, 0xDD, 0x01, 0x01, 0x03, 0xCA, 0x9A, 0x3B, 0xBC,
                                                      0xC8, 0x1F, 0x00, 0x82, 0x03, 0x04, 0x01, 0x01, 0x6A, 0x00, 0x50,
                                                      0x03, 0xB4, 0x03, 0x01};
            const PacketBytes body = PacketFromOpcode(received.data(), received.size(), 4);
            Check(body.data == received.data() + 4 && body.size == 22, "the packet starts at the opcode");
            const NpcUpdate mark = ParseNpcPacket(kOpSpawnNpc, body.data, body.size);
            Check(mark.op == NpcUpdate::Add && mark.npc == 2083004 && mark.x == 898 && mark.y == 260,
                "Mark of the Squad from the logged TEST bytes");
            Check(ParseNpcPacket(kOpSpawnNpc, received.data(), received.size()).npc != 2083004,
                "from the buffer start the header is read as the packet (the D1 bug)");
            Check(!PacketFromOpcode(received.data(), received.size(), 25).data, "no room for an opcode");
            Check(!PacketFromOpcode(received.data(), received.size(), 40).data, "offset past the end");
            Check(!PacketFromOpcode(nullptr, 10, 0).data, "no buffer");
            // P1-Q2 (Zakum run): drop_item and the party requests.
            c = ParseCommand("drop_item 4001017");
            Check(c.kind == Command::DropItem && c.item == 4001017 && c.count == 1, "drop_item drops one by default");
            c = ParseCommand("drop_item 2000000 5");
            Check(c.kind == Command::DropItem && c.count == 5, "drop_item takes a count");
            Check(ParseCommand("drop_item 999999").kind == Command::Invalid, "drop_item needs an inventory item id");
            Check(ParseCommand("drop_item 6000000").kind == Command::Invalid, "no sixth tab");
            Check(ParseCommand("drop_item 4001017 0").kind == Command::Invalid, "count starts at 1");
            Check(ParseCommand("drop_item 4001017 40000").kind == Command::Invalid, "count fits a short");
            Check(ParseCommand("drop_item 4001017 1 x").kind == Command::Invalid, "nothing after the count");
            Check(InventoryTypeOf(1302000) == 1 && InventoryTypeOf(4001017) == 4 && InventoryTypeOf(5000000) == 5,
                "the tab is the millions digit");
            Check(ParseCommand("close_dialog").kind == Command::CloseDialog, "close_dialog parses");
            Check(ParseCommand("close_dialog 1").kind == Command::Invalid, "close_dialog takes nothing");
            Check(kDlgRetEnd != kDlgRetOk && kDlgRetEnd != 3 && kDlgRetEnd != 0x2000 && kDlgRetEnd != 0x2001 &&
                kDlgRetEnd != 6 && kDlgRetEnd != 7, "the end return is none of the answers nor a server close");
            Check(ParseCommand("party_create").kind == Command::PartyCreate, "party_create parses");
            Check(ParseCommand("party_create now").kind == Command::Invalid, "party_create takes nothing");
            c = ParseCommand("party_invite Bishbot");
            Check(c.kind == Command::PartyInvite && c.name == "Bishbot", "party_invite parses");
            Check(ParseCommand("party_invite abc").kind == Command::Invalid, "names are at least 4");
            Check(ParseCommand("party_invite abcdefghijklm").kind == Command::Invalid, "names are at most 12");
            Check(ParseCommand("party_invite Bish\"bot").kind == Command::Invalid, "names are letters and digits");
            Check(ParseCommand("party_invite Bish bot").kind == Command::Invalid, "one name only");
            auto talk = [](int type, const std::string& text) {
                std::vector<unsigned char> b{0x30, 0x01, 4, 0x08, 0xFA, 0x1E, 0x00, static_cast<unsigned char>(type), 0,
                                             static_cast<unsigned char>(text.size() & 0xFF),
                                             static_cast<unsigned char>(text.size() >> 8)};
                b.insert(b.end(), text.begin(), text.end());
                return b;
            };
            auto menu = talk(4, "#wPick:#b\r\n#L0#Stage 1#l\r\n#L5#Stage 2#l\r\n#L12#Stage 3#l");
            NpcMenu m = ParseNpcMenu(menu.data(), menu.size());
            Check(m.isTalk && m.isMenu && m.values == std::vector<int>({0, 5, 12}), "menu rows keep their #L values");
            auto say = talk(0, "#wHello #L0#not a row in a say#l");
            m = ParseNpcMenu(say.data(), say.size());
            Check(m.isTalk && !m.isMenu && m.values.empty(), "a say dialog is not a menu");
            auto odd = talk(4, "#L#broken #Lx# #L3 #L7#ok#l");
            Check(ParseNpcMenu(odd.data(), odd.size()).values == std::vector<int>({7}), "only well-formed #L<n># rows count");
        }
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
