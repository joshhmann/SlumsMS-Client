// ============================================================
// wztrace.cpp -- the QA client's WZ lookup trace (ticket b251). SLUMS_QA_MODULE builds only (src/CMakeLists.txt), and
// even there it costs nothing until config.ini [debug] wztrace says 1 or 2:
//   wztrace=1  every FAILED WZ namespace lookup goes to wztrace.log next to the exe: UTC stamp, thread, HRESULT, path
//   wztrace=2  also keeps the last 128 lookups (misses and hits, with their HRESULT) and detours _CxxThrowException:
//              each throw logs its caller and dumps that ring, at most once per 2 s, then calls the original
// The log stops at 2 MB. docs/wztrace.md says how to read it.
//
// Why this exists: 211080100 (Rose Garden) ends the session through an uncaught C++ throw out of packet processing
// during the field change, and src/crash_diagnostics.cpp stops after 16 C++ events per process -- the owner's run had
// spent all 16 on startup and sound throws, so the one that matters was never written. The field data is complete;
// what is missing is which WZ lookup the client made last. A path plus an HRESULT names it.
//
// The namespace side lives in resman.cpp's existing raw__OnGetLocalObject_hook (one detour, already there); this file
// owns the file, the ring and the throw/MakeTile detours.
// ============================================================
#include "pch.h"

#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <mutex>
#include <string>

#include "constants.h"
#include "debug.h"
#include "hook.h"
#include "wztrace.h"
#include "wztrace_patch.h"
#include "wztrace_policy.h"

namespace {

using namespace WzTrace;

std::wstring GameDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (wchar_t* slash = wcsrchr(path, L'\\')) *slash = 0;
    return path;
}

const std::wstring& LogFile() {
    static const std::wstring path = GameDir() + L"\\" + kLogNameW;
    return path;
}

// One read per process: a QA client's config.ini does not change while it runs, and the hooks are installed once.
int Level() {
    static const int level = ClampLevel(GetPrivateProfileIntA("debug", "wztrace", 0, ".\\" CONSTANTS_CONFIG_NAME));
    return level;
}

QaPolicy::UtcTime NowStamp() {
    SYSTEMTIME s = {};
    GetSystemTime(&s);   // UTC, like every other log this client writes
    return QaPolicy::UtcTime{s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond, s.wMilliseconds};
}

std::uint64_t NowMs() {
    return GetTickCount64();
}

// ---- the log ----------------------------------------------------------------------------------------------------
std::mutex g_logMutex;   // the namespace hook runs on whichever thread loads; two lines must never interleave
bool g_capTold = false;  // the cap notice is written once per process (and again only if the log is emptied)

// Opened, appended and closed per line, like qa-captures\login-packets.log: the client that dies here dies without
// flushing anything, and a QA harness may delete the log between runs. Every write is cap-checked first.
void AppendRaw(const char* data, std::size_t size) {
    if (!data || size == 0) return;
    std::lock_guard<std::mutex> guard(g_logMutex);
    FILE* out = nullptr;
    if (_wfopen_s(&out, LogFile().c_str(), L"ab") != 0 || !out) return;
    _fseeki64(out, 0, SEEK_END);
    const long long size64 = _ftelli64(out);
    if (size64 < 0) {
        std::fclose(out);
        return;
    }
    if (UnderCap(static_cast<std::uint64_t>(size64), size)) {
        g_capTold = false;   // a log the harness emptied starts again
        std::fwrite(data, 1, size, out);
    } else if (!g_capTold) {
        const std::string notice = CapLine(NowStamp());
        std::fwrite(notice.data(), 1, notice.size(), out);
        g_capTold = true;
    }
    std::fclose(out);
}

void Append(const std::string& line) {
    AppendRaw(line.data(), line.size());
}

// ---- the ring ---------------------------------------------------------------------------------------------------
LookupRing g_ring;
std::mutex g_dumpMutex;   // holds the whole dump: two throwing threads must not splice two rings together
bool g_ringDumped = false;
std::uint64_t g_lastRingDumpMs = 0;

constexpr std::size_t kDumpBufferBytes = kRingSize * (32 + kMaxPathBytes + 2);

void DumpRingOnce() {
    std::lock_guard<std::mutex> guard(g_dumpMutex);
    const std::size_t count = g_ring.Count();
    if (count == 0) return;
    const std::uint64_t now = NowMs();
    if (!ShouldDumpRing(g_ringDumped, g_lastRingDumpMs, now)) return;
    g_ringDumped = true;
    g_lastRingDumpMs = now;
    static char dump[kDumpBufferBytes];
    const std::size_t used = g_ring.AppendDump(dump, sizeof dump);
    Append(RingBeginLine(NowStamp(), count));
    AppendRaw(dump, used);
    Append(RingEndLine(NowStamp()));
}

// ---- the throw hook ---------------------------------------------------------------------------------------------
// __stdcall, not __cdecl: the stock function ends in `ret 8` (0xA60BEE), so the callee pops its two arguments. A
// __cdecl detour would leave the caller's stack eight bytes off and crash the client on the first throw.
typedef void(__stdcall* CxxThrowFn)(void* object, void* throwInfo);
CxxThrowFn g_cxxThrow_original = nullptr;

// The thrown object's first dword, when that reads and only then when it falls in an HRESULT range. POD only, no C++
// objects: __try is illegal in a function that needs unwinding (C2712), and this is the one place a bad pointer could
// fault. Everything else about the object stays unread -- a string or a packet buffer must never reach the log.
bool ReadThrownHresult(const void* object, std::uint32_t* out) {
    __try {
        const std::uint32_t value = *static_cast<const std::uint32_t*>(object);
        if (!IsHResult(value)) return false;
        *out = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// crash_diagnostics.cpp's column shape: address, module file name, offset into that module.
void ModuleOf(std::uint32_t address, std::string* module, std::uint32_t* offset) {
    *module = "unknown";
    *offset = address;
    HMODULE mod = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(static_cast<std::uintptr_t>(address)), &mod) || !mod) {
        return;
    }
    char path[MAX_PATH] = {};
    if (GetModuleFileNameA(mod, path, MAX_PATH) == 0) return;
    const char* name = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') name = p + 1;
    }
    *module = name;
    *offset = address - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(mod));
}

void __stdcall CxxThrow_hook(void* object, void* throwInfo) {
    try {
        // _ReturnAddress() is the caller's address: the detour jumps here in place of the function's first bytes, so
        // the return address on this frame belongs to whoever called _CxxThrowException.
        const std::uint32_t caller = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        std::uint32_t hresult = 0;
        const bool hasHresult = ReadThrownHresult(object, &hresult);
        std::string module;
        std::uint32_t offset = 0;
        ModuleOf(caller, &module, &offset);
        Append(ThrowLine(NowStamp(), GetCurrentThreadId(), caller, module, offset, hasHresult, hresult));
        DumpRingOnce();
    } catch (...) {
        // A trace that throws has to stay quieter than the throw it was watching: the original still runs.
    }
    g_cxxThrow_original(object, throwInfo);
}

// ---- the tile hook ----------------------------------------------------------------------------------------------
typedef void*(__thiscall* MakeTileFn)(void* self, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2);
MakeTileFn g_makeTile_original = nullptr;

void* __thiscall MakeTile_hook(void* self, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    // The three arguments are logged RAW, never dereferenced: research-119-120.md could not type them from the stock
    // code alone (the string-pool ids they come from are not decodable offline), and a wrong deref would crash a QA
    // client -- the one thing this trace must not do. Small numbers here are the layer and tile indices, so the line
    // still says which tile was built just before a Map/Tile lookup missed.
    try {
        Append(MakeTileLine(NowStamp(), GetCurrentThreadId(), static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(self)),
            a0, a1, a2));
    } catch (...) {
    }
    return g_makeTile_original(self, a0, a1, a2);
}

// ---- install ----------------------------------------------------------------------------------------------------
bool StockBytes(std::uint32_t address, const unsigned char* expected, std::size_t length) {
    return WzTracePatch::Matches(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(address)),
        expected, length);
}

}   // namespace

void WzTrace_Install() {
    const int level = Level();
    if (level < 1) return;   // off: no file, no hook, nothing to undo
    Append(SessionLine(NowStamp(), level));
    DebugMessage("wztrace: level %d, %s next to the exe (cap 2 MB)", level, kLogName);
    if (level < 2) return;

    if (!StockBytes(WzTracePatch::kAddr_CxxThrowException, WzTracePatch::CxxThrowExceptionBytes().data(),
            WzTracePatch::kCxxThrowExceptionLength)) {
        DebugMessage("wztrace: _CxxThrowException at 0x%08X is not the stock bytes; throws and the ring stay off",
            WzTracePatch::kAddr_CxxThrowException);
    } else {
        g_cxxThrow_original = reinterpret_cast<CxxThrowFn>(
            static_cast<std::uintptr_t>(WzTracePatch::kAddr_CxxThrowException));
        ATTACH_HOOK(g_cxxThrow_original, CxxThrow_hook);
    }

    if (!StockBytes(WzTracePatch::kAddr_MakeTile, WzTracePatch::MakeTileBytes().data(),
            WzTracePatch::kMakeTileLength)) {
        DebugMessage("wztrace: MakeTile at 0x%08X is not the stock bytes; tile lines stay off",
            WzTracePatch::kAddr_MakeTile);
    } else {
        g_makeTile_original = reinterpret_cast<MakeTileFn>(
            static_cast<std::uintptr_t>(WzTracePatch::kAddr_MakeTile));
        ATTACH_HOOK(g_makeTile_original, MakeTile_hook);
    }
}

void WzTrace_NoteLookup(const wchar_t* path, long hr) {
    const int level = Level();
    if (level < 1 || !path) return;
    try {
        char buffer[kMaxPathBytes * 4 + 8];
        // Bounded source length: a path longer than the cap is cut by LogPath anyway, and an unbounded -1 conversion
        // would fail outright (and log nothing) on a path wider than this buffer.
        const int wide = static_cast<int>(wcsnlen(path, kMaxPathBytes + 1));
        const int n = WideCharToMultiByte(CP_UTF8, 0, path, wide, buffer, static_cast<int>(sizeof buffer), nullptr,
            nullptr);
        if (n <= 1) return;   // empty path: not a lookup worth a line
        const std::string utf8(buffer, static_cast<std::size_t>(n - 1));
        const std::uint32_t value = static_cast<std::uint32_t>(hr);
        if (level >= 2) g_ring.Push(value, LogPath(utf8));
        if (!Succeeded(value)) Append(LookupLine(NowStamp(), GetCurrentThreadId(), value, utf8));
    } catch (...) {
    }
}
