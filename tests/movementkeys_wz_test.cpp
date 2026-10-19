#include "../src/pch.h"
#include "../src/ztl/ztl.h"
#include "../src/movementkeys.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>

static void WriteIconBmp(const wchar_t* path, IWzCanvas* icon) {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + 32 * 32 * 4;
    info.biSize = sizeof(info);
    info.biWidth = 32;
    info.biHeight = 32;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    FILE* out = nullptr;
    if (_wfopen_s(&out, path, L"wb") || !out) return;
    fwrite(&file, sizeof(file), 1, out);
    fwrite(&info, sizeof(info), 1, out);
    for (int y = 31; y >= 0; --y)
        for (int x = 0; x < 32; ++x) {
            const unsigned pixel = icon->Getpixel(x, y);
            fwrite(&pixel, sizeof(pixel), 1, out);
        }
    fclose(out);
}

static ZAllocEx<ZAllocAnonSelector> testAllocator;
ZALLOCEX(ZAllocAnonSelector, &testAllocator)
extern "C" void* __stdcall ZtlTaskMemAllocImp(SIZE_T size) { return CoTaskMemAlloc(size); }
extern "C" void __stdcall ZtlTaskMemFreeImp(void* p) { CoTaskMemFree(p); }
#pragma comment(linker, "/EXPORT:ZtlTaskMemAllocImp=_ZtlTaskMemAllocImp@4")
#pragma comment(linker, "/EXPORT:ZtlTaskMemFreeImp=_ZtlTaskMemFreeImp@4")

int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 4) return 2;
    {
        unsigned char keys[256]{};
        unsigned char bindings[90 * 5]{};
        const int scans[] = {17, 31, 30, 32}; // W, S, A, D
        const int virtualKeys[] = {'W', 'S', 'A', 'D'};
        const int arrows[] = {0xC8, 0xD0, 0xCB, 0xCD};
        for (int i = 0; i < 4; ++i) {
            if (MapVirtualKeyW(scans[i], MAPVK_VSC_TO_VK_EX) != virtualKeys[i]) return 14;
            keys[scans[i]] = 0x80;
            auto* binding = bindings + scans[i] * 5;
            binding[0] = 4;
            const int action = 28 + i;
            std::memcpy(binding + 1, &action, sizeof(action));
        }
        ApplyMovementKeyBindings(keys, bindings);
        for (int i = 0; i < 4; ++i)
            if (keys[scans[i]] || !(keys[arrows[i]] & 0x80)) return 13;
    }
    {
        unsigned char keys[256]{};
        unsigned char bindings[90 * 5]{};
        auto bind = [&](int scan, int type, int action) {
            bindings[scan * 5] = static_cast<unsigned char>(type);
            std::memcpy(bindings + scan * 5 + 1, &action, sizeof(action));
        };
        bind(17, 4, 28); // Valid but not pressed: must not synthesize movement.
        bind(18, 1, 28); keys[18] = 0x80; // Same action number, different type.
        bind(19, 4, 32); keys[19] = 0x80; // Unsupported action.
        bind(88, 4, 31); keys[88] = 0x80; // Last supported bindable scan.
        bind(89, 4, 29); keys[89] = 0x80; // Outside bindable range.
        keys[0xCB] = 0x80; // Physical arrow remains usable alongside assigned key.
        ApplyMovementKeyBindings(keys, bindings);
        if (keys[0xC8] || keys[0xD0] || !keys[0xCB] || !keys[0xCD] || keys[88]
                || !keys[18] || !keys[19] || !keys[89]) return 17;
    }
    SetDllDirectoryW(argv[1]);
    HMODULE pcom = LoadLibraryW((std::filesystem::path(argv[1]) / L"PCOM.dll").c_str());
    if (!pcom) return 3;
    auto init = reinterpret_cast<HRESULT(__cdecl*)()>(GetProcAddress(pcom, "PcInitModule"));
    if (!init || FAILED(init())) return 4;
    FARPROC functions[5] = {GetProcAddress(pcom, "PcCreateObject"), nullptr, nullptr, nullptr,
                            GetProcAddress(pcom, "PcRootNameSpace")};
    _g_ppfnPCOMAPIs = functions;

    try {
        IWzNameSpacePtr root, ui;
        IWzResManPtr rm;
        IWzFileSystemPtr fs;
        PcCreateObject<IWzNameSpacePtr>(L"NameSpace", root, nullptr);
        PcCreateObject<IWzNameSpacePtr>(L"NameSpace", ui, nullptr);
        PcCreateObject<IWzResManPtr>(L"ResMan", rm, nullptr);
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
        PcSetRootNameSpace(root);
        rm->rootNameSpace = root;
        rm->SetResManParam(static_cast<RESMAN_PARAM>(RC_AUTO_REPARSE | RC_AUTO_SERIALIZE), -1, -1);
        IWzWritableNameSpacePtr writable = root;
        Ztl_variant_t mounted;
        writable->AddObject(L"UI", static_cast<IUnknown*>(ui), &mounted);
        ui = mounted.GetUnknown();
        if (std::filesystem::exists(std::filesystem::path(argv[1]) / L"Data/UI/UIWindow.img")) {
            fs->Init((std::filesystem::path(argv[1]) / L"Data/UI").c_str());
            ui->Mount(L"/", fs, 0);
        } else {
            fs->Init(argv[1]);
            IWzPackagePtr package;
            PcCreateObject<IWzPackagePtr>(L"NameSpace#Package", package, nullptr);
            IWzSeekableArchivePtr archive = fs->item[L"UI.wz"].GetUnknown();
            package->Init(L"83", L"UI", archive);
            ui->Mount(L"/", package, 0);
        }

        IWzPropertyPtr window = rm->GetObjectA(L"UI/UIWindow.img").GetUnknown();
        if (!window) return 5;
        IWzCanvasPtr attack = window->item[L"KeyConfig/icon/0"].GetUnknown();
        if (!attack || attack->width != 32 || attack->height != 32) return 6;
        IWzCanvasPtr blank = window->item[L"KeyConfig/icon/10"].GetUnknown();
        if (!blank) return 10;
        InjectMovementKeyIcons(window, L"UI/UIWindow.img");
        if (argc >= 3) {
            const int previewId = argc == 4 ? _wtoi(argv[3]) : 28;
            const auto previewPath = std::wstring(L"KeyConfig/icon/") + std::to_wstring(previewId);
            IWzCanvasPtr preview = window->item[previewPath.c_str()].GetUnknown();
            if (preview) WriteIconBmp(argv[2], preview);
        }
        for (int id = 28; id <= 31; ++id) {
            auto path = std::wstring(L"KeyConfig/icon/") + std::to_wstring(id);
            IWzCanvasPtr arrow = window->item[path.c_str()].GetUnknown();
            if (!arrow || arrow->width != 32 || arrow->height != 32) return 7;
            if (arrow->cx != attack->cx || arrow->cy != attack->cy) return 11;
            if (arrow->Getpixel(8, 24) != blank->Getpixel(4, 4) ||
                arrow->Getpixel(16, 27) != blank->Getpixel(4, 4)) return 15;
            unsigned different = 0;
            unsigned borderDifferent = 0;
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 32; ++x)
                {
                    different += arrow->Getpixel(x, y) != blank->Getpixel(x, y);
                    if (x >= 3 && x <= 28 && y >= 2 && y <= 29) {
                        const unsigned pixel = arrow->Getpixel(x, y);
                        if (pixel != blank->Getpixel(4, 4) && pixel != 0xFFFDF6F9) return 16;
                    }
                    if (x < 3 || x > 28 || y < 3 || y > 28)
                        borderDifferent += arrow->Getpixel(x, y) != blank->Getpixel(x, y);
                }
            if (different < 40) return 8;
            if (borderDifferent) return 12;
        }
        std::puts("PASS: four 32x32 direction icons decoded from the actual UI data");
        std::fflush(stdout);
        // PCOM's standalone shutdown is intermittent outside MapleStory.
        // All checks have completed; let the OS release its COM allocations.
        std::_Exit(0);
    } catch (const _com_error& e) {
        std::printf("COM 0x%08X\n", e.Error());
        return 9;
    }
}
