// QA client runner channel (qamodule_policy.h): signals, commands, event lines, capture header.
#include "../src/qamodule_policy.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    using namespace QaPolicy;
    try {
        Check(std::strcmp(SignalForOpcode(0x1B), "worldselect.ready") == 0, "world select after the recommended-world packet");
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
        std::cout << "qa module policy ok\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
