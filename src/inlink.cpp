#include "pch.h"
#include "hook.h"
#include "inlink_guard.h"
#include "wvs/util.h"
#include "ztl/ztl.h"

static HMODULE g_ownedCanvasModule = nullptr;


class CWzCanvas : public IWzCanvas {
public:
    typedef HRESULT(__stdcall* raw_Serialize_t)(IWzCanvas*, IWzArchive*);
    inline static raw_Serialize_t raw_Serialize_orig;

    HRESULT __stdcall raw_Serialize_hook(IWzArchive* pArchive) {
        HRESULT hr = raw_Serialize_orig(this, pArchive);
        if (FAILED(hr)) {
            return hr;
        }
        Ztl_variant_t vInlink = this->property->item[L"_inlink"];
        if (V_VT(&vInlink) == VT_BSTR) {
            ZXString<wchar_t> sFilePath(pArchive->absoluteUOL);
            sFilePath.ReleaseBuffer(sFilePath.Find(L".img") + 5);
            sFilePath.Cat(V_BSTR(&vInlink));
            this->property->item[L"_inlink"] = static_cast<const wchar_t*>(sFilePath);
        }
        return hr;
    }
};

void HandleLinkProperty(IWzCanvasPtr pCanvas) {
    FillLinkedCanvas(pCanvas);
}

static auto get_unknown_orig = reinterpret_cast<IUnknownPtr*(__cdecl*)(IUnknownPtr*, Ztl_variant_t&)>(0x00414ADA);
IUnknownPtr* __cdecl get_unknown_hook(IUnknownPtr* result, Ztl_variant_t& v) {
    get_unknown_orig(result, v);
    IWzCanvasPtr pCanvas;
    if (SUCCEEDED(result->QueryInterface(__uuidof(IWzCanvas), &pCanvas))) {
        HandleLinkProperty(pCanvas);
    }
    return result;
}

static auto Ztl_variant_t__GetUnknown = reinterpret_cast<IUnknown*(__thiscall*)(Ztl_variant_t*, bool, bool)>(0x004032B2);
IUnknown* __fastcall Ztl_variant_t__GetUnknown_hook(Ztl_variant_t* pThis, void* _EDX, bool fAddRef, bool fTryChangeType) {
    IUnknownPtr result = pThis->GetUnknown(fAddRef, fTryChangeType);
    IWzCanvasPtr pCanvas;
    if (SUCCEEDED(result.QueryInterface(__uuidof(IWzCanvas), &pCanvas))) {
        HandleLinkProperty(pCanvas);
    }
    return result;
}


void AttachClientInlink() {
    // This runs before PCOM starts. Track only the reference we acquire here,
    // so Canvas can be unloaded before the game's allocators are destroyed.
    if (!GetModuleHandleA("CANVAS.DLL")) {
        g_ownedCanvasModule = LoadLibraryA("CANVAS.DLL");
    }
    CWzCanvas::raw_Serialize_orig = reinterpret_cast<CWzCanvas::raw_Serialize_t>(GetAddressByPattern("CANVAS.DLL", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 EC 6C"));
    ATTACH_HOOK(CWzCanvas::raw_Serialize_orig, CWzCanvas::raw_Serialize_hook);
    ATTACH_HOOK(get_unknown_orig, get_unknown_hook);
    ATTACH_HOOK(Ztl_variant_t__GetUnknown, Ztl_variant_t__GetUnknown_hook); // for cases where nexon uses this instead of get_unknown
}

void ReleaseClientInlinkResources() {
    if (!g_ownedCanvasModule) return;
    if (!DetachHook(reinterpret_cast<void**>(&CWzCanvas::raw_Serialize_orig),
            CastHook(&CWzCanvas::raw_Serialize_hook))) {
        DebugMessage("SESSION_CLEANUP_FAILED reason=canvas-detach");
        return;
    }
    HMODULE module = g_ownedCanvasModule;
    g_ownedCanvasModule = nullptr;
    if (!FreeLibrary(module)) {
        DebugMessage("SESSION_CLEANUP_FAILED reason=canvas-release error=%lu", GetLastError());
    } else {
        DebugMessage("SESSION_CANVAS_LIBRARY_RELEASED");
    }
}
