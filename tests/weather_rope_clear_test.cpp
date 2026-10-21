// Rope / ladder sway frames must keep exactly the source's own pixels and nothing else.
//
// The 1.4.x "ropes have a rectangle border" report: the CopyEx shear path cleared its
// frame a second time by stretching one "measured transparent" source pixel over it, and
// for connect/rope/0/1 and /0/3 (Henesys hunting ground) that pixel is 80% opaque in our
// Map.wz, so every swayed rope segment drew inside a box. Runs against a real client's
// Canvas.dll and Data/Map/Obj/connect.img.
#include "../src/pch.h"
#include "../src/ztl/ztl.h"
#include "../src/weathercanvas.h"
#include <filesystem>
#include <cstdio>
static ZAllocEx<ZAllocAnonSelector> testAllocator;
ZALLOCEX(ZAllocAnonSelector, &testAllocator)
extern "C" void* __stdcall ZtlTaskMemAllocImp(SIZE_T size) { return CoTaskMemAlloc(size); }
extern "C" void __stdcall ZtlTaskMemFreeImp(void* p) { CoTaskMemFree(p); }
#pragma comment(linker, "/EXPORT:ZtlTaskMemAllocImp=_ZtlTaskMemAllocImp@4")
#pragma comment(linker, "/EXPORT:ZtlTaskMemFreeImp=_ZtlTaskMemFreeImp@4")

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static unsigned AlphaOf(unsigned argb) { return argb >> 24; }

struct Stats { unsigned long long nLit = 0, nAlphaSum = 0; };
static Stats Measure(IWzCanvasPtr c) {
    Stats s;
    const int w = c->width, h = c->height;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const unsigned a = AlphaOf((unsigned)c->Getpixel(x, y));
            s.nLit += a != 0;
            s.nAlphaSum += a;
        }
    return s;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    SetDllDirectoryW(argv[1]);
    HMODULE pcom = LoadLibraryW((std::filesystem::path(argv[1]) / L"PCOM.dll").c_str());
    if (!pcom) return 3;
    auto init = reinterpret_cast<HRESULT(__cdecl*)()>(GetProcAddress(pcom, "PcInitModule"));
    if (!init || FAILED(init())) return 4;
    FARPROC api[5] = {GetProcAddress(pcom, "PcCreateObject"), nullptr, nullptr, nullptr, GetProcAddress(pcom, "PcRootNameSpace")};
    _g_ppfnPCOMAPIs = api;
    try {
        IWzNameSpacePtr root; IWzResManPtr rm; IWzFileSystemPtr fs;
        PcCreateObject<IWzNameSpacePtr>(L"NameSpace", root, nullptr);
        PcCreateObject<IWzResManPtr>(L"ResMan", rm, nullptr);
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
        PcSetRootNameSpace(root); rm->rootNameSpace = root;
        rm->SetResManParam(static_cast<RESMAN_PARAM>(RC_AUTO_REPARSE | RC_AUTO_SERIALIZE), -1, -1);
        fs->Init((std::filesystem::path(argv[1]) / L"Data").c_str()); root->Mount(L"/", fs, 0);

        // 1. The 0x00FFFFFF sentinel really zeroes a canvas that already holds pixels.
        {
            IWzCanvasPtr c; PcCreateObject<IWzCanvasPtr>(L"Canvas", c, nullptr);
            c->Create(24, 24, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);
            c->raw_DrawRectangle(0, 0, 24, 24, 0xFF804020);
            CHECK(Measure(c).nLit == 24 * 24, "opaque fill did not take");
            c->raw_DrawRectangle(0, 0, 24, 24, 0x00FFFFFF);
            CHECK(Measure(c).nLit == 0, "0x00FFFFFF sentinel left %llu lit pixels", Measure(c).nLit);
        }

        // 2. The old second clear, for the record: stretching the tabled pixel (4,12) of
        // rope/0/3 over a cleared canvas is what boxed the Henesys ropes.
        IWzCanvasPtr rope3 = rm->GetObjectA(L"Map/Obj/connect.img/rope/0/3/0").GetUnknown();
        if (!rope3) return 5;
        {
            IWzCanvasPtr c; PcCreateObject<IWzCanvasPtr>(L"Canvas", c, nullptr);
            c->Create(16, 128, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);
            c->raw_DrawRectangle(0, 0, 16, 128, 0x00FFFFFF);
            c->CopyEx(0, 0, rope3, CANVAS_ALPHATYPE::CA_OVERWRITE, 16, 128, 4, 12, 1, 1);
            std::printf("legacy stretch-clear from rope/0/3 (4,12): pixel 0,0 = %08X (alpha %u)\n",
                        (unsigned)c->Getpixel(0, 0), AlphaOf((unsigned)c->Getpixel(0, 0)));
        }

        // 3. A swayed rope / ladder frame holds exactly its source's pixels, whatever the
        // clear-pixel table says. Sprites: the Henesys rope segments (bad table row) and
        // unlisted ones whose corner is opaque or faint in our data.
        const wchar_t* aSprites[] = {
            L"Map/Obj/connect.img/rope/0/1/0", L"Map/Obj/connect.img/rope/0/3/0",
            L"Map/Obj/connect.img/rope/67/1/0", L"Map/Obj/connect.img/rope/20/1/0",
            L"Map/Obj/connect.img/ladder/10/0/0", L"Map/Obj/connect.img/ladder/0/1/0",
        };
        int nTested = 0;
        for (const wchar_t* path : aSprites) {
            IWzCanvasPtr src;
            try { src = rm->GetObjectA(path).GetUnknown(); } catch (const _com_error&) {}
            if (!src) { std::printf("skip (absent): %ls\n", path); continue; }
            const Stats sSrc = Measure(src);
            for (int f = -1; f <= 1; ++f) {
                PROJECT cfg{};
                cfg.fLean = 0.0108f * (float)f;      // rope fMax: SWAY_LEAN_MAX * 0.18
                cfg.fSquash = 1.0f; cfg.fAlpha = 1.0f; cfg.nBlur = 0;
                cfg.bKeepColour = true;
                cfg.nPadOverride = 6;
                cfg.nClearX = 4; cfg.nClearY = 12;   // the bad row the table carried
                cfg.nBaseOffset = 40; cfg.nRigidDepth = 20;
                cfg.bHangFromTop = true;
                cfg.nPivotRow = 0;
                POINT anchor{};
                IWzCanvasPtr out = WeatherCanvas_Project(src, cfg, &anchor, nullptr);
                CHECK(out != nullptr, "%ls lean %d: no frame", path, f);
                if (!out) continue;
                const Stats s = Measure(out);
                CHECK(s.nLit == sSrc.nLit && s.nAlphaSum == sSrc.nAlphaSum,
                      "%ls lean %d: frame has %llu lit px / alpha %llu, source %llu / %llu (box around the sprite)",
                      path, f, s.nLit, s.nAlphaSum, sSrc.nLit, sSrc.nAlphaSum);
                // Every pad column must be empty.
                const int w = out->width, h = out->height;
                unsigned long long nPadLit = 0;
                for (int y = 0; y < h; ++y) {
                    nPadLit += AlphaOf((unsigned)out->Getpixel(0, y)) != 0;
                    nPadLit += AlphaOf((unsigned)out->Getpixel(w - 1, y)) != 0;
                }
                CHECK(nPadLit == 0, "%ls lean %d: %llu lit pixels on the frame's outer columns", path, f, nPadLit);
            }
            ++nTested;
        }
        CHECK(nTested >= 2, "only %d rope sprites found", nTested);
        if (g_fail) { std::fflush(stdout); std::_Exit(1); }
        std::printf("PASS: %d rope/ladder sprites x 3 leans keep exactly their own pixels; sentinel clear zeroes\n", nTested);
        std::fflush(stdout); std::_Exit(0);
    } catch (const _com_error& e) { std::printf("COM %08X\n", e.Error()); return 11; }
}
