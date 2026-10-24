#include "../src/settings_values.h"
#include <cassert>
#include <map>
#include <vector>
#include <utility>

int main() {
    using namespace slums_settings;
    using Key = std::pair<std::string, std::string>;
    std::map<Key, std::string> ini;
    auto read = [&](const char* section, const char* key) -> std::optional<std::string> {
        auto it = ini.find({section, key});
        return it == ini.end() ? std::nullopt : std::optional<std::string>(it->second);
    };
    auto defaults = load(read);
    assert(!defaults[0] && defaults[1] && defaults[2] && !defaults[3]);
    assert(defaults[4] && defaults[5] && defaults[6] && defaults[7] && defaults[8]);
    ini[{"clienthax", "moveattack"}] = "false";
    auto legacy = load(read);
    assert(!legacy[0] && !legacy[1] && !legacy[2]);
    ini[{"clienthax", "attackjump"}] = " TRUE ";
    legacy = load(read);
    assert(legacy[1] && !legacy[2]);
    ini[{"clienthax", "attackwalk"}] = "yes";
    ini[{"clienthax", "doubletapjump"}] = "1";
    assert(load(read)[0] && load(read)[3]);
    assert(boolean(std::string("f alse"), true));
    assert(!boolean(std::string("garbage"), false));
    assert(boolean(std::string("On"), false));
    assert(!boolean(std::string("OFF"), true));
    ini[{"config", "host"}] = "example.invalid";
    ini[{"general", "resolution"}] = "5";
    ini[{"weather", "sway"}] = "false";
    auto before = ini;
    std::vector<Key> writes;
    Write write = [&](const char* section, const char* key, const char* value) {
        writes.push_back({section, key});
        if (value) ini[{section, key}] = value; else ini.erase({section, key});
        return true;
    };
    assert(save(defaults, read, write, 7));
    assert(writes.size() == 7);
    assert(ini.at({"config", "host"}) == "example.invalid");
    assert(ini.at({"general", "resolution"}) == "5");
    assert(ini.at({"weather", "sway"}) == "false"); // unsupported build leaves weather alone
    for (const auto& key : writes) assert(key.first == "clienthax" || key.first == "discord");
    ini = before;
    int calls = 0;
    Write fail = [&](const char* section, const char* key, const char* value) {
        write(section, key, value); // even a backend that changed the failed key must be restored
        return ++calls != 3;
    };
    assert(!save(defaults, read, fail, 9));
    assert(ini == before);
    calls = 0;
    assert(!save(defaults, read, fail, 10));
    assert(calls == 0);
    return 0;
}
