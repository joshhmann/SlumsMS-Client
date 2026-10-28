#pragma once
// ============================================================
// customwz_policy.h -- which Custom.wz paths may be served whole (resman.cpp), tested by tests/customwz_policy_test.cpp.
//
// MapleRoot mounts two file systems at the root: the game folder first, then ./Data. A lookup such as
// "Effect/BasicEff.img/NoRed1" misses in the game folder first, and resman.cpp's fallback used to answer that miss from
// Custom.wz before ./Data was tried. For an image that also exists in stock, the client then got the Custom.wz copy,
// which holds only the added leaves: the damage digit sets without "0".."9" (rc3-rc5) and the Arcane bosses' Mob info
// without maxHP (1.6.0.2), 2026-10-28. Now only images with no stock file are served whole; a stock image keeps its
// own data and gets the Custom.wz leaves merged in (CWzProperty::Serialize hook: added, or replaced since bNoReplace
// is false).
// ============================================================

#include <string>

namespace CustomWz {

// "Effect/BasicEff.img/NoRed1/0" -> "Effect/BasicEff.img"; a path with no ".img" segment (a directory) -> "".
inline std::wstring ImagePrefix(const std::wstring& path) {
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find(L'/', start);
        if (end == std::wstring::npos) end = path.size();
        const size_t len = end - start;
        if (len > 4 && path.compare(end - 4, 4, L".img") == 0) return path.substr(0, end);
        start = end + 1;
    }
    return std::wstring();
}

// The top directory of a path, for "<top>.wz" in the game folder: "Mob/8880150.img" -> "Mob".
inline std::wstring TopDir(const std::wstring& path) {
    const size_t slash = path.find(L'/');
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

}  // namespace CustomWz
