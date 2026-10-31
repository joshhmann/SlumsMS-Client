// A linked canvas that cannot be filled in must cost a tile, not the client.
//
// Every WzLib call HandleLinkProperty makes (GetObjectA, GetSnapshot, Create, AddRawCanvas)
// throws _com_error when it fails, and the function runs inside the map load, so before this
// guard any one of them left the process instead of the canvas — the same shape as b103, where
// a linked minimap canvas with no origin threw E_POINTER and closed the client on entry.
// Runs against a real client's PCOM.dll, Canvas.dll and Data/Map.
#include "../src/pch.h"
#include "../src/ztl/ztl.h"
#include "../src/inlink_guard.h"
#include <filesystem>
#include <cstdio>
static ZAllocEx<ZAllocAnonSelector> testAllocator;
ZALLOCEX(ZAllocAnonSelector, &testAllocator)
extern "C" void* __stdcall ZtlTaskMemAllocImp(SIZE_T size) { return CoTaskMemAlloc(size); }
extern "C" void __stdcall ZtlTaskMemFreeImp(void* p) { CoTaskMemFree(p); }
#pragma comment(linker, "/EXPORT:ZtlTaskMemAllocImp=_ZtlTaskMemAllocImp@4")
#pragma comment(linker, "/EXPORT:ZtlTaskMemFreeImp=_ZtlTaskMemFreeImp@4")

// The guard reads the resman through this accessor in a test binary (the stock one reads an
// address inside the game exe).
static IWzResManPtr g_rm;
static IWzResManPtr& TestResMan() {
    return g_rm;
}

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

static unsigned long long LitPixels(IWzCanvasPtr c) {
    unsigned long long lit = 0;
    for (int y = 0; y < (int)c->height; ++y)
        for (int x = 0; x < (int)c->width; ++x)
            lit += ((unsigned)c->Getpixel(x, y) >> 24) != 0;
    return lit;
}

static IWzCanvasPtr NewCanvas(int w, int h) {
    IWzCanvasPtr c;
    PcCreateObject<IWzCanvasPtr>(L"Canvas", c, nullptr);
    if (c) c->Create(w, h, 0, CANVAS_PIXFORMAT::CP_A8R8G8B8);
    return c;
}

static void SetLink(IWzCanvasPtr c, const wchar_t* name, const wchar_t* value) {
    c->property->item[name] = value;
}

// The body this change replaced (src/inlink.cpp at d028206), kept here so the suite proves the
// difference instead of assuming the guard is why a broken link no longer escapes: same calls,
// same order, no checks, no catch.
static void LegacyFill(IWzCanvasPtr pCanvas) {
    const wchar_t* asLinkProperty[] = {L"_inlink", L"_outlink", L"source"};
    for (size_t i = 0; i < sizeof(asLinkProperty) / sizeof(asLinkProperty[0]); ++i) {
        Ztl_variant_t vLink = pCanvas->property->item[asLinkProperty[i]];
        if (V_VT(&vLink) != VT_BSTR) {
            continue;
        }
        IWzCanvasPtr pSource;
        IUnknownPtr pUnknown = g_rm->GetObjectA(V_BSTR(&vLink)).GetUnknown();
        if (!pUnknown || FAILED(pUnknown->QueryInterface(&pSource))) {
            continue;
        }
        int nWidth, nHeight, nFormat, nMagLevel;
        pSource->GetSnapshot(&nWidth, &nHeight, nullptr, nullptr, (CANVAS_PIXFORMAT*)&nFormat, &nMagLevel);
        pCanvas->Create(nWidth, nHeight, nMagLevel, nFormat);
        pCanvas->AddRawCanvas(0, 0, pSource->rawCanvas[0][0]);
        IWzVector2DPtr pOrigin = pCanvas->property->item[L"origin"].GetUnknown();
        if (pOrigin) {
            pCanvas->cx = pOrigin->x;
            pCanvas->cy = pOrigin->y;
        }
        break;
    }
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
        g_rm = rm;
        SetLinkedCanvasResMan(&TestResMan);
        rm->SetResManParam(static_cast<RESMAN_PARAM>(RC_AUTO_REPARSE | RC_AUTO_SERIALIZE), -1, -1);
        fs->Init((std::filesystem::path(argv[1]) / L"Data").c_str()); root->Mount(L"/", fs, 0);

        // 1. Without any link property a canvas is left alone.
        {
            IWzCanvasPtr c = NewCanvas(3, 5);
            CHECK(c && c->width == 3 && c->height == 5, "setup canvas");
            CHECK(!FillLinkedCanvas(c), "a canvas with no link property reported a fill");
            CHECK(c->width == 3 && c->height == 5, "a canvas with no link property was resized to %ux%u",
                  c->width, c->height);
            CHECK(!FillLinkedCanvas(nullptr), "null canvas reported a fill");
        }

        // 2. A link to a real canvas copies its pixels (the job the hook exists for).
        {
            IWzCanvasPtr src = rm->GetObjectA(L"Map/Obj/connect.img/rope/0/3/0").GetUnknown();
            CHECK(src != nullptr, "rope/0/3/0 missing from the client's Data");
            if (src) {
                IWzCanvasPtr c = NewCanvas(1, 1);
                SetLink(c, L"_inlink", L"Map/Obj/connect.img/rope/0/3/0");
                CHECK(FillLinkedCanvas(c), "a resolvable link did not fill");
                CHECK(c->width == src->width && c->height == src->height,
                      "linked canvas is %ux%u, source is %ux%u", c->width, c->height, src->width, src->height);
                unsigned long long same = 0, lit = 0;
                for (int y = 0; y < (int)src->height; ++y)
                    for (int x = 0; x < (int)src->width; ++x) {
                        if (c->Getpixel(x, y) == src->Getpixel(x, y)) ++same;
                        lit += ((unsigned)src->Getpixel(x, y) >> 24) != 0;
                    }
                CHECK(lit != 0, "source sprite is empty, nothing proved");
                CHECK(same == (unsigned long long)src->width * src->height,
                      "linked canvas copied %llu of %u pixels", same, src->width * src->height);
                // The target above has no origin property at all - the b103 shape - and the fill
                // still happened instead of the GetUnknown throwing. A target that carries the
                // source's origin takes cx/cy from it, as the game expects.
                IWzCanvasPtr cOrigin = NewCanvas(1, 1);
                SetLink(cOrigin, L"_inlink", L"Map/Obj/connect.img/rope/0/3/0");
                cOrigin->property->Add(L"origin", src->property->item[L"origin"]);
                CHECK(FillLinkedCanvas(cOrigin), "a link onto a target with an origin did not fill");
                CHECK(cOrigin->cx == src->cx && cOrigin->cy == src->cy,
                      "origin %d,%d became %d,%d", src->cx, src->cy, cOrigin->cx, cOrigin->cy);
            }
        }

        // 3. The hazard the guard is for: the same lookup throws when the path is not there.
        //    Pre-guard, that exception left HandleLinkProperty and closed the client.
        {
            bool threw = false;
            try {
                rm->GetObjectA(L"Map/Obj/no_such_set.img/rope/0/3/0");
            } catch (const _com_error&) {
                threw = true;
            }
            CHECK(threw, "GetObjectA on a missing path did not throw; the guard's premise is untested");
        }

        // 4. Broken links: no throw, and the canvas is untouched.
        {
            // bLegacyThrows marks the links where the pre-fix body left the function as an
            // exception; those are the fail-before half of this test.
            struct { const wchar_t* pLink; bool bLegacyThrows; } asBroken[] = {
                {L"Map/Obj/no_such_set.img/rope/0/3/0", true},   // whole image missing
                // A missing node inside a real image resolves to nothing and the pre-fix
                // body just carried on; only a lookup that fails (a missing image here) throws.
                {L"Map/Obj/connect.img/rope/999/0", false},
                {L"Map/Obj/connect.img/rope", false},            // resolves to a property, not a canvas
                {L"", false},                                    // empty string
            };
            for (const auto& broken : asBroken) {
                const wchar_t* pLink = broken.pLink;
                bool bLegacyThrew = false;
                try {
                    IWzCanvasPtr legacy = NewCanvas(2, 2);
                    SetLink(legacy, L"_inlink", pLink);
                    LegacyFill(legacy);
                } catch (const _com_error&) {
                    bLegacyThrew = true;
                }
                CHECK(bLegacyThrew == broken.bLegacyThrows,
                      "the pre-guard body threw=%d on \"%ls\", expected %d", (int)bLegacyThrew, pLink, (int)broken.bLegacyThrows);
                IWzCanvasPtr c = NewCanvas(2, 2);
                SetLink(c, L"_inlink", pLink);
                bool ok = true;
                bool filled = false;
                try {
                    filled = FillLinkedCanvas(c);
                } catch (const _com_error& e) {
                    ok = false;
                    std::printf("FAIL: link \"%ls\" threw 0x%08lX out of FillLinkedCanvas\n", pLink, (unsigned long)e.Error());
                }
                CHECK(ok, "link \"%ls\" escaped as an exception", pLink);
                CHECK(!filled, "link \"%ls\" reported a fill", pLink);
                CHECK(c->width == 2 && c->height == 2 && LitPixels(c) == 0,
                      "link \"%ls\" left the canvas at %ux%u with %llu lit pixels", pLink, c->width, c->height, LitPixels(c));
            }
        }

        // 5. The map case this guard was written for: Ristonia's allInvisibleTile draws linked
        //    placeholders (enH1/0 -> enH0/0). The raw_Serialize hook turns the stored relative
        //    link into an absolute one; that rewrite is what this reproduces here.
        {
            IWzCanvasPtr placeholder = rm->GetObjectA(L"Map/Tile/allInvisibleTile.img/enH1/0").GetUnknown();
            IWzCanvasPtr head = rm->GetObjectA(L"Map/Tile/allInvisibleTile.img/enH0/0").GetUnknown();
            CHECK(placeholder != nullptr, "enH1/0 missing from the client's Data");
            CHECK(head != nullptr, "enH0/0 missing from the client's Data");
            if (placeholder && head) {
                const unsigned long long storedPixels = LitPixels(placeholder);
                SetLink(placeholder, L"_inlink", L"Map/Tile/allInvisibleTile.img/enH0/0");
                CHECK(FillLinkedCanvas(placeholder), "enH1/0 did not fill from enH0/0");
                CHECK(placeholder->width == head->width && placeholder->height == head->height,
                      "enH1/0 is %ux%u, enH0/0 is %ux%u", placeholder->width, placeholder->height, head->width, head->height);
                CHECK(LitPixels(placeholder) == LitPixels(head),
                      "enH1/0 has %llu lit pixels, enH0/0 has %llu", LitPixels(placeholder), LitPixels(head));
                std::printf("allInvisibleTile enH1/0: stored %llu lit pixels, filled %ux%u from enH0/0\n",
                            storedPixels, placeholder->width, placeholder->height);
            }
        }
    } catch (const _com_error& e) {
        std::printf("FAIL: unexpected _com_error 0x%08lX: %ls\n", (unsigned long)e.Error(), e.ErrorMessage());
        ++g_fail;
    }

    std::printf(g_fail ? "inlink guard: %d failure(s)\n" : "inlink guard: ok\n", g_fail);
    return g_fail ? 1 : 0;
}
