#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "mountinfo.h"
#include "mountinfo_layout.h"
#include "mountinfo_render.h"
#include "wvs/wnd.h"
#include "wvs/util.h"
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <cstring>

namespace {
auto SetLayer = reinterpret_cast<void(__thiscall*)(void*)>(0x008FF863);
auto Update = reinterpret_cast<void(__thiscall*)(void*)>(0x009010BA);
auto Draw = reinterpret_cast<void(__thiscall*)(void*, const RECT*)>(0x00901185);
using Part = MountPreviewPart;
struct Frame { std::vector<Part> parts; int delay; IWzCanvasPtr composed; };
std::vector<Frame> frames;
MountLayout layout{};
int cachedId = 0;
void* hiddenUi = nullptr;
void* hiddenAvatar = nullptr;
void* lastFrameUi = nullptr;
int lastFrame = -1;
bool closing = false;
int State(void* ui) { return *reinterpret_cast<int*>(static_cast<char*>(ui) + 0x660); }
int MountId(void* ui) { return *reinterpret_cast<int*>(static_cast<char*>(ui) + 0x6CC); }
bool HasMount(void* ui) { return *reinterpret_cast<int*>(static_cast<char*>(ui) + 0x6C8) != 0; }
void* Preview(void* ui) { return *reinterpret_cast<void**>(static_cast<char*>(ui) + 0x65C); }
void LoadAnimation(int id) {
    if (id == cachedId) return;
    frames.clear(); cachedId = id; lastFrame = -1;
    if (id <= 0) return;
    for (const wchar_t* pose : {L"stand1", L"stand2", L"walk1", L"fly"}) {
        MountBounds bounds{};
        bool first = true;
        for (int i = 0; i < 256; ++i) {
            wchar_t path[160];
            swprintf_s(path, L"Character/TamingMob/%08d.img/%s/%d", id, pose, i);
            IWzPropertyPtr node;
            try { node = get_rm()->GetObjectA(path).GetUnknown(); }
            catch (const _com_error&) { break; }
            if (!node) break;
            auto delayValue = node->item[L"delay"];
            Frame frame{{}, (std::max)(16, (std::min)(get_int32(delayValue, 100), 5000)), nullptr};
            IEnumVARIANTPtr entries = node->_NewEnum;
            for (int count = 0; count < 128; ++count) {
                Ztl_variant_t key;
                ULONG fetched = 0;
                if (FAILED(entries->Next(1, &key, &fetched)) || !fetched) break;
                if (V_VT(&key) != VT_BSTR) continue;
                const wchar_t* name = V_BSTR(&key);
                wchar_t* end = nullptr;
                wcstol(name, &end, 10);
                if (!*name || !end || *end) continue; // delay/metadata are not artwork.
                IWzCanvasPtr canvas;
                try { canvas = node->item[name].GetUnknown(); } catch (const _com_error&) { continue; }
                if (!canvas) continue;
                int order = 1;
                try {
                    IWzPropertyPtr property = canvas->property;
                    auto z = property->item[L"z"];
                    if (V_VT(&z) == VT_BSTR) {
                        if (_wcsicmp(V_BSTR(&z), L"tamingMobRear") == 0) order = 0;
                        else if (_wcsicmp(V_BSTR(&z), L"tamingMobFront") == 0) order = 2;
                    } else order = get_int32(z, 1);
                } catch (const _com_error&) {}
                Part part{canvas, static_cast<int>(canvas->width), static_cast<int>(canvas->height),
                    static_cast<int>(canvas->cx), static_cast<int>(canvas->cy), order};
                if (part.w <= 0 || part.h <= 0) continue;
                AlignMountPart(part);
                MountBounds b{double(-part.cx), double(-part.cy), double(part.w - part.cx), double(part.h - part.cy)};
                if (first) { bounds = b; first = false; }
                else {
                    bounds.left = (std::min)(bounds.left, b.left); bounds.top = (std::min)(bounds.top, b.top);
                    bounds.right = (std::max)(bounds.right, b.right); bounds.bottom = (std::max)(bounds.bottom, b.bottom);
                }
                frame.parts.push_back(std::move(part));
            }
            if (frame.parts.empty()) break;
            std::stable_sort(frame.parts.begin(), frame.parts.end(), [](const Part& a, const Part& b) { return a.order < b.order; });
            frames.push_back(std::move(frame));
        }
        if (!frames.empty()) {
            layout = FitMountPreview(bounds);
            IWzCanvasPtr background = get_rm()->GetObjectA(L"UI/UIWindow.img/UserInfo/backgrnd5").GetUnknown();
            if (!background) { frames.clear(); return; }
            for (auto& frame : frames) frame.composed = ComposeMountPreview(background, frame.parts, layout);
            unsigned parts = 0; for (const auto& frame : frames) parts += unsigned(frame.parts.size());
            DebugMessage("MOUNT_PREVIEW_ALIGNED id=%d frames=%u parts=%u scale=%.4f", id, unsigned(frames.size()), parts, layout.scale);
            return;
        }
    }
    DebugMessage("MOUNT_PREVIEW_FALLBACK id=%d", id);
}
bool Ready(void* ui) {
    if (closing || State(ui) != 2 || !HasMount(ui)) return false;
    try { LoadAnimation(MountId(ui)); } catch (const _com_error&) { frames.clear(); }
    return !frames.empty() && layout.scale > 0;
}
int CurrentFrame() {
    unsigned duration = 0;
    for (const auto& f : frames) duration += f.delay;
    if (!duration) return 0;
    auto tick = GetTickCount64() % duration;
    for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
        if (tick < static_cast<unsigned>(frames[i].delay)) return i;
        tick -= frames[i].delay;
    }
    return 0;
}
void HideStock(void* ui, bool force) {
    void* avatar = Preview(ui);
    if (!avatar || (!force && ui == hiddenUi && avatar == hiddenAvatar)) return;
    reinterpret_cast<void(__thiscall*)(void*, int)>(0x00456F18)(avatar, 0);
    hiddenUi = ui; hiddenAvatar = avatar;
}
void __fastcall SetLayerHook(void* ui, void*) {
    SetLayer(ui);
    if (Ready(ui)) HideStock(ui, true);
    else { hiddenUi = hiddenAvatar = nullptr; }
}
void __fastcall UpdateHook(void* ui, void*) {
    Update(ui);
    if (!Ready(ui)) { lastFrame = -1; return; }
    HideStock(ui, false);
    int frame = CurrentFrame();
    if (lastFrameUi != ui || lastFrame != frame) {
        lastFrameUi = ui; lastFrame = frame;
        reinterpret_cast<void(__thiscall*)(void*, const RECT*)>(0x009E04C9)(ui, nullptr);
    }
}
std::string FitName(const char* name) {
    std::string text(name);
    IWzFontPtr fonts[2];
    auto font = reinterpret_cast<IWzFontPtr*(__cdecl*)(IWzFontPtr*, int)>(0x0098A707);
    for (int i = 0; i < 2; ++i) font(std::addressof(fonts[i]), i);
    auto fits = [&](const std::string& s) {
        for (auto& f : fonts) {
            if (!f || f->CalcTextWidth(Ztl_bstr_t(s.c_str()), Ztl_variant_t()) > 88) return false;
        }
        return true;
    };
    if (fits(text)) return text;
    while (!text.empty()) {
        const char* previous = CharPrevExA(CP_ACP, text.c_str(), text.c_str() + text.size(), 0);
        text.resize(static_cast<size_t>(previous - text.c_str()));
        while (!text.empty() && text.back() == ' ') text.pop_back();
        if (fits(text + "..")) return text + "..";
    }
    return {};
}
struct BorrowName {
    char** slot; char* original;
    BorrowName(char** target, char* replacement) : slot(target), original(*target) { *slot = replacement; }
    ~BorrowName() { *slot = original; }
};
void __fastcall DrawHook(void* ui, void*, const RECT* rect) {
    if (closing || State(ui) != 2 || !HasMount(ui)) { Draw(ui, rect); return; }
    char** slot = reinterpret_cast<char**>(static_cast<char*>(ui) + 0x6C4);
    std::string fitted;
    bool shorten = false;
    if (*slot && **slot) {
        try { fitted = FitName(*slot); shorten = fitted != *slot; }
        catch (const _com_error&) {}
    }
    if (shorten) {
        ZXString<char> temporary(fitted.c_str());
        BorrowName borrowed(slot, static_cast<char*>(temporary));
        Draw(ui, rect);
    } else Draw(ui, rect);
    if (!Ready(ui)) return;
    try {
        IWzCanvasPtr canvas = static_cast<CWnd*>(ui)->GetCanvas();
        if (!canvas) return;
        // One complete background+mount blit prevents stale pixels and partially drawn bodies.
        canvas->Copy(9, 182, frames[CurrentFrame()].composed);
    } catch (const _com_error&) {}
}
}
void ShutdownMountInfo() {
    closing = true;
    decltype(frames)().swap(frames);
    hiddenUi = hiddenAvatar = lastFrameUi = nullptr;
    DebugMessage("MOUNT_INFO_RESOURCES_RELEASED");
}
void AttachMountInfoMod() {
    // Verified entry signatures on the unpacked MapleRoot game executable.
    struct Anchor { uintptr_t address; const char* bytes; size_t size; };
    const Anchor anchors[] = {
        {0x008FF863, "\xB8\xE8\x6E\xAD\x00\xE8", 6},
        {0x009010BA, "\x56\x8B\xF1\xE8\xBC\xF5\x0D\x00", 8},
        {0x00901185, "\xB8\xC7\x77\xAD\x00\xE8", 6},
        {0x00456F18, "\x53\x55\x56\x8B\x74\x24\x10", 7},
        {0x009E04C9, "\x33\xC0\x39\x44\x24\x04\x56", 7},
        {0x0098A707, "\xB8\x8E\x1A\xAE\x00\xE8", 6}
    };
    for (const auto& a : anchors) {
        if (memcmp(reinterpret_cast<const void*>(a.address), a.bytes, a.size)) {
            ErrorMessage("Mount info: unsupported binary at %08X", unsigned(a.address)); return;
        }
    }
    if (!ATTACH_HOOK(SetLayer, SetLayerHook) || !ATTACH_HOOK(Update, UpdateHook) || !ATTACH_HOOK(Draw, DrawHook)) {
        ErrorMessage("Mount info: failed to attach combined preview/name hooks"); return;
    }
    DebugMessage("MOUNT_INFO_READY renderer=navel-composed-v3");
}
