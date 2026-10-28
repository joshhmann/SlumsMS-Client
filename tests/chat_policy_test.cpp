// Ticket 123: chat tabs and filters (chat_policy.h).
#include "../src/chat_policy.h"
#include <iostream>
#include <stdexcept>

using namespace Chat;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

void CheckTabs() {
    Check(TabOf(1) == Tab::Whisper && TabOf(2) == Tab::Party && TabOf(5) == Tab::Guild && TabOf(7) == Tab::System, "tabs");
    Check(TabOf(0) == Tab::General && TabOf(11) == Tab::General && TabOf(14) == Tab::Megaphone, "general and megaphone");
    Check(InTab(Tab::All, 7) && !InTab(Tab::Party, 0), "all vs one");
    for (int t = 0; t < kTypes; ++t) Check(InTab(Tab::All, t) && (StockColour(t) >> 24) == 0xFF, "every type: All, opaque");
    Check(StockColour(-1) == 0xFFFFFFFF && StockColour(kTypes) == 0xFFFFFFFF, "unknown type is white");
}

void CheckSpeakers() {
    Check(SenderOf(0, "Momo : hi all") == "Momo", "map chat sender");
    Check(SenderOf(1, "Momo[Ch 2]>> psst") == "Momo" && SenderOf(1, "Momo>> psst") == "Momo", "incoming whisper");
    Check(SenderOf(1, "Momo<< psst").empty(), "outgoing whisper has no speaker");
    Check(SenderOf(2, "Momo : party up") == "Momo" && SenderOf(4, "Momo : gm") == "Momo", "party and guild");
    Check(SenderOf(7, "You have gained experience (+100)").empty(), "system has no sender");
    Check(SenderOf(8, "Momo : looks like chat").empty(), "only chat types are parsed");
    Check(SenderOf(0, "two words : x").empty() && SenderOf(0, " : x").empty(), "no name, no speaker");
    Check(MessageOf(0, "Momo : hi : there") == "hi : there", "message keeps later separators");
    Check(MessageOf(7, "You have gained") == "You have gained", "no speaker: the whole line");
}

void CheckKeep() {
    Rules r;
    r.keywords = {"buy mesos", "  "};
    r.players = {" spammer01 "};
    Check(!Keep(0, "Spammer01 : hello", r), "blocked player, any case, trimmed");
    Check(!Keep(1, "spammer01[Ch 3]>> hey", r), "blocked player's whisper");
    Check(Keep(1, "spammer01<< stop it", r), "my whisper to a blocked player stays");
    Check(!Keep(0, "Bob : cheap BUY MESOS here", r), "keyword, any case");
    Check(Keep(0, "Bob : hello", r), "normal line; a blank keyword blocks nothing");
    Check(Keep(10, "[Notice] buy mesos is against the rules", r) && Keep(11, "Spammer01 : GM here", r), "notices and GM");
    r.keywords = {"momo"};
    Check(Keep(0, "Momo : hi", r) && !Keep(0, "Bob : hi momo", r), "keywords match the message, not the speaker");
    Check(!Keep(8, "Momo picked up an item", r), "system lines match on the whole line");
    r.keywords.clear();
    Check(Keep(7, "You have gained experience (+100)", r), "tips show by default");
    r.hideSystemTips = true;
    Check(!Keep(7, "You have gained experience (+100)", r) && Keep(12, "The party has been formed.", r), "tips vs results");
}

void CheckLists() {
    std::vector<std::string> l;
    Check(AddUnique(l, " Momo ") && !AddUnique(l, "MOMO") && !AddUnique(l, "  ") && l.size() == 1 && l[0] == "Momo",
        "add unique, trimmed");
    const auto s = SplitList("a\n\nB\nb\n c \n");
    Check(s.size() == 3 && s[0] == "a" && s[1] == "B" && s[2] == "c", "split drops blanks and duplicates");
    Check(JoinList(s) == "a\nB\nc" && SplitList(JoinList(s)) == s, "join round-trips");
    Check(SplitList("").empty() && JoinList({}).empty(), "empty list");
}

void CheckRing() {
    Ring<int, 3> r;
    Check(r.Size() == 0 && r.Serial() == 0, "empty ring");
    r.Push(1); r.Push(2);
    Check(r.Size() == 2 && r[0] == 1 && r[1] == 2, "fills oldest first");
    r.Push(3); r.Push(4);
    Check(r.Size() == 3 && r[0] == 2 && r[2] == 4 && r.Serial() == 4, "drops the oldest when full");
    r.Clear();
    Check(r.Size() == 0 && r.Serial() == 5, "clear counts as a change");
}
}  // namespace

int main() {
    try {
        CheckTabs();
        CheckSpeakers();
        CheckKeep();
        CheckLists();
        CheckRing();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "chat: ok\n";
    return 0;
}
