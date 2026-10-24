#pragma once
#include <array>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

namespace slums_settings {
struct Preference { const char* section; const char* key; const char* label; bool fallback; };
inline constexpr std::array<Preference, 9> preferences{{
    {"clienthax", "attackwalk", "Walk while attacking", false},
    {"clienthax", "attackjump", "Jump while attacking", true},
    {"clienthax", "chargemove", "Move while charging an attack", true},
    {"clienthax", "doubletapjump", "Double-tap jump for Ninja Bounce", false},
    {"clienthax", "nobulb", "Hide quest lightbulbs", true},
    {"clienthax", "customcolor", "Use coloured character text", true},
    {"discord", "enableactivitystatus", "Show Discord activity", true},
    {"weather", "sway", "Wind moves plants and ropes", true},
    {"weather", "ropesway", "Wind moves ropes", true},
}};
using Values = std::array<bool, preferences.size()>;
using Read = std::function<std::optional<std::string>(const char*, const char*)>;
// A null value removes the key, for restoring a previously absent setting after a failed save.
using Write = std::function<bool(const char*, const char*, const char*)>;
inline bool boolean(const std::optional<std::string>& raw, bool fallback) {
    if (!raw) return fallback;
    const auto first = raw->find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return fallback;
    std::string text = raw->substr(first, raw->find_last_not_of(" \t\r\n") - first + 1);
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (text == "true" || text == "yes" || text == "on" || text == "1") return true;
    if (text == "false" || text == "no" || text == "off" || text == "0") return false;
    return fallback;
}
inline Values load(const Read& read) {
    Values values{};
    const bool legacy = boolean(read("clienthax", "moveattack"), true);
    for (std::size_t i = 0; i < preferences.size(); ++i) {
        const auto& p = preferences[i];
        values[i] = boolean(read(p.section, p.key), i == 1 || i == 2 ? legacy : p.fallback);
    }
    return values;
}
inline bool save(const Values& values, const Read& read, const Write& write, std::size_t count) {
    if (count > preferences.size()) return false;
    std::array<std::optional<std::string>, preferences.size()> old{};
    for (std::size_t i = 0; i < count; ++i) old[i] = read(preferences[i].section, preferences[i].key);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& p = preferences[i];
        if (!write(p.section, p.key, values[i] ? "true" : "false")) {
            // Include the failed key: a storage backend may have changed it before reporting failure.
            for (std::size_t j = 0; j <= i; ++j) {
                const auto& previous = preferences[j];
                write(previous.section, previous.key, old[j] ? old[j]->c_str() : nullptr);
            }
            return false;
        }
    }
    return true;
}
}
