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
    return 0;
}
