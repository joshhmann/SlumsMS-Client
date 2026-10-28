// Ticket 123: chat tabs and filters (chat_policy.h).
#include "../src/chat_policy.h"
#include <iostream>
#include <stdexcept>

using namespace Chat;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        Check(TabOf(1) == Tab::Whisper && TabOf(2) == Tab::Party && TabOf(5) == Tab::Guild && TabOf(7) == Tab::System, "tabs");
        Check(InTab(Tab::All, 7) && !InTab(Tab::Party, 0), "all vs one");
        Check(SenderOf(0, "Momo : hi all") == "Momo", "map chat sender");
        Check(SenderOf(1, "Momo[Ch 2]>> psst") == "Momo", "whisper sender");
        Check(SenderOf(7, "You have gained experience (+100)").empty(), "system has no sender");
        Rules r;
        r.keywords = {"buy mesos"};
        r.players = {"spammer01"};
        Check(!Keep(0, "Spammer01 : hello", r), "blocked player");
        Check(!Keep(0, "Bob : cheap BUY MESOS here", r), "keyword, any case");
        Check(Keep(0, "Bob : hello", r), "normal line");
        Check(Keep(10, "[Notice] buy mesos is against the rules", r), "notices always show");
        r.hideSystemTips = true;
        Check(!Keep(7, "You have gained experience (+100)", r) && Keep(12, "The party has been formed.", r), "tips vs results");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "chat: ok\n";
    return 0;
}
