#pragma once
#include <cwchar>

// Animated emissive door glows keep the engine's original colour/animation.
// Scenery around them still participates in weather tinting.
inline bool WeatherOwnsObjectColor(const wchar_t* bank, const wchar_t* group, const wchar_t* category) {
    if (!bank || !group || !category) return true;
    if (_wcsnicmp(bank, L"ClassicFM_", 10) == 0) bank += 10;
    return !(_wcsicmp(bank, L"signboard") == 0 && _wcsicmp(group, L"market") == 0
        && (_wcsicmp(category, L"potal0") == 0 || _wcsicmp(category, L"potal1") == 0 || _wcsicmp(category, L"back") == 0));
}
