#pragma once
// ============================================================
// crash_classify.h -- a real crash, in plain words (ticket 121).
//
// No Windows, no client: tests/crash_classify_test.cpp checks it. crash_report.cpp fills CrashFacts from the
// EXCEPTION_POINTERS of a last-chance (unhandled) exception and writes the headline and advice into
// crash_reports/LATEST_CRASH.txt. The raw code and module are always printed under the headline: this is a best guess.
// Rules are checked in order; the first that matches wins (docs: research note 121, "Plain-words mapping table").
// ============================================================

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace CrashClassify {

struct CrashFacts {
    uint32_t code = 0;            // ExceptionCode
    uint32_t avAddr = 0;          // access violation: the address read or written
    char module[64] = {};         // basename of the faulting module ("" if unknown)
    bool moduleIsMain = false;    // MapleStory.exe itself
    uint32_t cxxHr = 0;           // E06D7363: the thrown HRESULT / ZException code, if known
    uint64_t availVirtual = ~0ull;// bytes of free virtual address space at the crash
    bool missingAsset = false;    // a data lookup failed just before
    char missingName[96] = {};
};

enum class Cause {
    OutOfMemory, StackOverflow, DiskRead, MissingData, GraphicsDriver, Overlay, Sound, DataLibrary, Addon,
    HeapCorruption, InvalidCode, MathError, Disconnected, InternalError, NullObject, AccessViolation, Unknown,
};

enum class ModuleClass { None, Graphics, Overlay, Sound, DataLibrary, Addon, System, Main, Other };

namespace detail {
inline bool StartsWithI(const char* s, const char* p) {
    for (; *p; ++s, ++p) {
        if (!*s || std::tolower(static_cast<unsigned char>(*s)) != std::tolower(static_cast<unsigned char>(*p))) return false;
    }
    return true;
}
inline bool EqualsI(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) return false;
    }
    return *a == *b;
}
inline bool AnyI(const char* m, const char* const* list, bool prefix) {
    for (; *list; ++list) {
        if (prefix ? StartsWithI(m, *list) : EqualsI(m, *list)) return true;
    }
    return false;
}
}  // namespace detail

// Module names: graphics drivers and renderers, in-game overlays, sound, the v83 data libraries, ours.
inline ModuleClass ClassOfModule(const char* m) {
    using namespace detail;
    if (!m || !*m) return ModuleClass::None;
    static const char* const kGraphics[] = {"d3d9", "d3d8", "dxgi", "nvd3dum", "nvwgf2um", "atiumdag", "aticfx32",
        "amdxc32", "igdumd32", "igdumdim32", "igd9", "gr2d_dx9", "gr2d_dx8", "dxvk", nullptr};
    static const char* const kOverlay[] = {"rtsshooks", "graphics-hook32", "discordhook", "gameoverlayrenderer",
        "overwolf", "parsec", "nvspcap", nullptr};
    static const char* const kSound[] = {"sound_dx8", "mss32", "dsound", nullptr};
    static const char* const kData[] = {"pcom.dll", "namespace.dll", "resman.dll", "shape2d.dll", nullptr};
    if (AnyI(m, kGraphics, true)) return ModuleClass::Graphics;
    if (AnyI(m, kOverlay, true)) return ModuleClass::Overlay;
    if (AnyI(m, kSound, true)) return ModuleClass::Sound;
    if (AnyI(m, kData, false)) return ModuleClass::DataLibrary;
    if (EqualsI(m, "slums.dll")) return ModuleClass::Addon;
    if (EqualsI(m, "ntdll.dll")) return ModuleClass::System;
    if (EqualsI(m, "maplestory.exe")) return ModuleClass::Main;
    return ModuleClass::Other;
}

inline Cause Classify(const CrashFacts& f) {
    constexpr uint64_t kLowMemory = 64ull << 20;
    const ModuleClass mc = f.moduleIsMain ? ModuleClass::Main : ClassOfModule(f.module);
    const bool cxx = f.code == 0xE06D7363u;
    if (f.availVirtual < kLowMemory || f.code == 0xC0000017u || (cxx && (f.cxxHr == 0x8007000Eu || f.cxxHr == 0x22000003u)))
        return Cause::OutOfMemory;
    if (f.code == 0xC00000FDu) return Cause::StackOverflow;
    if (f.code == 0xC0000006u) return Cause::DiskRead;
    if (f.missingAsset || (cxx && f.cxxHr >= 0x22000004u && f.cxxHr <= 0x22000006u)) return Cause::MissingData;
    if (f.code == 0xC0000005u && mc == ModuleClass::Graphics) return Cause::GraphicsDriver;
    if (mc == ModuleClass::Overlay) return Cause::Overlay;
    if (mc == ModuleClass::Sound) return Cause::Sound;
    if (mc == ModuleClass::DataLibrary) return Cause::DataLibrary;
    if (mc == ModuleClass::Addon) return Cause::Addon;
    if (f.code == 0xC0000374u || mc == ModuleClass::System) return Cause::HeapCorruption;
    if (f.code == 0xC000001Du || f.code == 0xC0000096u) return Cause::InvalidCode;
    if (f.code == 0xC0000094u || f.code == 0xC000008Eu || f.code == 0xC0000090u) return Cause::MathError;
    if (cxx && f.cxxHr >= 0x21000000u && f.cxxHr <= 0x21000006u) return Cause::Disconnected;
    if (cxx) return Cause::InternalError;
    if (f.code == 0xC0000005u && mc == ModuleClass::Main && f.avAddr < 0x10000u) return Cause::NullObject;
    if (f.code == 0xC0000005u) return Cause::AccessViolation;
    return Cause::Unknown;
}

inline const char* Headline(Cause c) {
    switch (c) {
    case Cause::OutOfMemory: return "The game ran out of memory.";
    case Cause::StackOverflow: return "The game ran out of stack (an endless loop in the game or an add-on).";
    case Cause::DiskRead: return "The game could not read one of its files from disk.";
    case Cause::MissingData: return "A game data file is missing or damaged.";
    case Cause::GraphicsDriver: return "The graphics driver crashed.";
    case Cause::Overlay: return "Another program running inside the game crashed it (an overlay).";
    case Cause::Sound: return "The sound system crashed.";
    case Cause::DataLibrary: return "The game crashed while reading its data files.";
    case Cause::Addon: return "A SlumsMS add-on crashed.";
    case Cause::HeapCorruption: return "The game's memory got corrupted.";
    case Cause::InvalidCode: return "The game tried to run invalid code.";
    case Cause::MathError: return "The game hit a maths error (divide by zero).";
    case Cause::Disconnected: return "The connection to the server was lost.";
    case Cause::InternalError: return "The game hit an internal error.";
    case Cause::NullObject: return "The game tried to use something that was not loaded.";
    case Cause::AccessViolation: return "The game crashed (memory access error).";
    default: return "The game crashed.";
    }
}

inline const char* Advice(Cause c) {
    switch (c) {
    case Cause::OutOfMemory: return "Close other programs and lower the resolution. Tell staff if it keeps happening.";
    case Cause::DiskRead: return "Check the disk or your antivirus, then run Repair in the launcher.";
    case Cause::MissingData:
    case Cause::DataLibrary:
    case Cause::InvalidCode: return "Run Repair in the launcher. If it happens again, send the report.";
    case Cause::GraphicsDriver: return "Update your graphics driver, turn off overlays, or try windowed mode.";
    case Cause::Overlay: return "Turn that overlay off for MapleStory.";
    case Cause::Sound: return "Check your audio device, or try with the sound off.";
    case Cause::Disconnected: return "Check your internet connection and try again.";
    default: return "Please send the report from the launcher. This helps us fix it.";
    }
}

// "The graphics driver crashed (nvd3dumx.dll)." -- the module goes in brackets when it adds something.
inline void FormatHeadline(char* out, size_t cap, const CrashFacts& f) {
    const Cause c = Classify(f);
    const char* h = Headline(c);
    const bool withModule = f.module[0] && (c == Cause::GraphicsDriver || c == Cause::Overlay || c == Cause::AccessViolation);
    const bool withName = c == Cause::MissingData && f.missingName[0];
    size_t n = std::strlen(h);
    if ((withModule || withName) && n && h[n - 1] == '.') {
        std::snprintf(out, cap, "%.*s (%s).", static_cast<int>(n - 1), h, withName ? f.missingName : f.module);
    } else {
        std::snprintf(out, cap, "%s", h);
    }
}

}  // namespace CrashClassify
