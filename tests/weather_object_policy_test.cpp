#include "../src/weather_object_policy.h"
int main() {
    if (WeatherOwnsObjectColor(L"ClassicFM_signboard", L"market", L"potal1")) return 1;
    if (WeatherOwnsObjectColor(L"signboard", L"market", L"potal0")) return 2;
    if (WeatherOwnsObjectColor(L"CLASSICFM_SIGNBOARD", L"MARKET", L"POTAL1")) return 3;
    if (!WeatherOwnsObjectColor(L"ClassicFM_signboard", L"market", L"number")) return 4;
    if (WeatherOwnsObjectColor(L"ClassicFM_signboard", L"market", L"back")) return 5;
    if (!WeatherOwnsObjectColor(L"ClassicFM_acc1", L"grassySoil", L"nature")) return 6;
    if (!WeatherOwnsObjectColor(nullptr, nullptr, nullptr)) return 7;
    if (!WeatherOwnsObjectColor(L"signboard", L"another", L"potal1")) return 8;
    // [weather] sway / ropesway kill-switches
    if (!WeatherSwayKindEnabled(2, true, true)) return 9;     // defaults: ropes sway
    if (WeatherSwayKindEnabled(2, true, false)) return 10;    // ropesway=false
    if (!WeatherSwayKindEnabled(1, true, false)) return 11;   // ...plants still bend
    if (!WeatherSwayKindEnabled(4, true, false)) return 12;
    if (WeatherSwayKindEnabled(1, false, true)) return 13;    // sway=false: nothing
    if (WeatherSwayKindEnabled(2, false, true)) return 14;
    if (WeatherSwayKindEnabled(0, true, true)) return 15;     // not a sway kind
    return 0;
}
