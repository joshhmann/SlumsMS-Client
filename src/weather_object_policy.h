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

// Which sway kinds weathersway builds frames for, from config.ini [weather]:
//   sway=false      -- nothing bends (plants, trees, signs, ropes and ladders)
//   ropesway=false  -- ropes and ladders stay as the map draws them; plants still bend
// Both default to true. `kind` is weather.cpp's SwayKind: 0 none, 1 foliage, 2 rope or
// ladder, 3 hanging, 4 tree. A rope or ladder left out keeps its own map layer, which is
// exactly how the client draws it with weather off.
inline bool WeatherSwayKindEnabled(unsigned char kind, bool swayAll, bool ropeSway) {
    if (kind == 0 || !swayAll) return false;
    if (kind == 2) return ropeSway;
    return true;
}
