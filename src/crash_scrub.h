#pragma once
// ============================================================
// crash_scrub.h -- keep a crash report free of personal data (ticket 121).
//
// No Windows, no client: tests/crash_scrub_test.cpp checks it. Every line copied into LATEST_CRASH.txt (the
// integration.log tail, module paths) goes through ScrubLine; the launcher scrubs again in Rust before showing or
// sending. Rules: a Windows profile path becomes <user>\..., NPC dialog bodies are dropped, IPv4 addresses are masked.
// ============================================================

#include <cctype>
#include <cstddef>
#include <cstring>

namespace CrashScrub {

namespace detail {
inline bool MatchI(const char* s, const char* p) {
    for (; *p; ++s, ++p) {
        if (!*s || std::tolower(static_cast<unsigned char>(*s)) != std::tolower(static_cast<unsigned char>(*p))) return false;
    }
    return true;
}
// Length of an IPv4 address starting at s (4 dot-separated groups of 1-3 digits, 5+ digits in all so a version
// like 1.6.0.2 is left alone), or 0.
inline size_t Ipv4At(const char* s) {
    size_t i = 0, digits = 0;
    for (int g = 0; g < 4; ++g) {
        size_t d = 0;
        while (d < 3 && std::isdigit(static_cast<unsigned char>(s[i + d]))) ++d;
        if (!d) return 0;
        i += d;
        digits += d;
        if (g < 3) {
            if (s[i] != '.') return 0;
            ++i;
        }
    }
    return std::isdigit(static_cast<unsigned char>(s[i])) || digits < 5 ? 0 : i;
}
}  // namespace detail

// Copies `in` to `out` (at most cap-1 chars plus NUL) with the rules applied; returns the length written.
inline size_t ScrubLine(const char* in, char* out, size_t cap) {
    if (!cap) return 0;
    size_t o = 0;
    auto put = [&](const char* s, size_t n) {
        for (size_t k = 0; k < n && o + 1 < cap; ++k) out[o++] = s[k];
    };
    if (std::strstr(in, "Dialog:") || std::strstr(in, "dialog text")) {   // NPC/player dialog lines: keep the tag only
        put("[dialog line removed]", 21);
        out[o] = 0;
        return o;
    }
    for (size_t i = 0; in[i];) {
        if ((i == 0 || !std::isalnum(static_cast<unsigned char>(in[i - 1]))) && std::isalpha(static_cast<unsigned char>(in[i])) &&
            in[i + 1] == ':' && (in[i + 2] == '\\' || in[i + 2] == '/') && detail::MatchI(in + i + 3, "users") &&
            (in[i + 8] == '\\' || in[i + 8] == '/')) {
            put("<user>", 6);                      // C:\Users\name -> <user>
            i += 9;
            while (in[i] && in[i] != '\\' && in[i] != '/') ++i;
            continue;
        }
        if ((i == 0 || !std::isdigit(static_cast<unsigned char>(in[i - 1]))) && std::isdigit(static_cast<unsigned char>(in[i]))) {
            if (size_t n = detail::Ipv4At(in + i)) {
                put("<ip>", 4);
                i += n;
                continue;
            }
        }
        put(in + i, 1);
        ++i;
    }
    out[o] = 0;
    return o;
}

}  // namespace CrashScrub
