#pragma once
// buildshow_text.h -- how the build is shown to players (1.6.0): the window title and the login-screen corner text.
// Header-only so tests/buildshow_test.cpp can check the strings. buildshow.cpp draws the login text; system.cpp sets
// the title. Both carry the same release and build as the CP 0x3738 stamp (cmake/BuildStamp.cmake), so a player's
// screenshot says exactly which client they run.
#include <cstddef>
#include <cstdio>

namespace BuildShow {

// "SlumsMS 1.6.0 (10600)"
inline void WindowTitle(char* out, std::size_t n, const char* release, int build) {
    std::snprintf(out, n, "SlumsMS %s (%d)", release, build);
}

// "SlumsMS 1.6.0 <U+00B7> build 10600" (a middle dot; the login font, Dotum, has it). The release is ASCII
// (cmake/BuildStamp.cmake checks it), so the text is built narrow with 0xB7 and widened byte by byte (Latin-1),
// which also avoids %s/%hs meaning different things in MSVC's and the standard's swprintf.
inline void LoginLabel(wchar_t* out, std::size_t n, const char* release, int build) {
    char narrow[96];
    std::snprintf(narrow, sizeof(narrow), "SlumsMS %s \xB7 build %d", release, build);
    std::size_t i = 0;
    for (; i + 1 < n && narrow[i]; ++i) out[i] = static_cast<wchar_t>(static_cast<unsigned char>(narrow[i]));
    if (n) out[i] = 0;
}

}  // namespace BuildShow
