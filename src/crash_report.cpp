// ============================================================
// crash_report.cpp -- the plain-language crash report (ticket 121, MAPLEROOT_CRASH_REPORT).
//
// system.cpp's SetUnhandledExceptionFilter hook puts SlumsTopLevelFilter in front of the game's own filter
// (CrashReport_Wrap keeps the game's), so it runs only for a real crash, never for the first-chance noise crash.log
// collects. It writes crash_reports\LATEST_CRASH.txt (via a .tmp and a rename, so a half-written file never replaces
// a good one) and then always hands the exception on to the game's filter. Static buffers and kernel32 only: the heap
// may be what broke. Contents: the headline and advice (crash_classify.h), the raw code, module and offset, the
// build, free memory, and the last lines of integration.log with paths, dialog text and IPs removed (crash_scrub.h).
// Nothing is sent anywhere: the launcher shows the file and sends it only when the player asks.
// ============================================================

#include "pch.h"
#include "crash_report.h"
#include "crash_classify.h"
#include "crash_scrub.h"

#include <cstdio>
#include <cstring>

namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_next = nullptr;
volatile LONG g_field = 0;
volatile LONG g_lastCxx = 0;
volatile LONG g_inFilter = 0;
char g_missing[96] = {};

char g_report[24 * 1024];     // the whole report
char g_tail[16 * 1024];       // the end of integration.log
char g_line[1024];
size_t g_len = 0;

void Put(const char* s) {
    const size_t n = strlen(s);
    if (g_len + n < sizeof(g_report)) {
        memcpy(g_report + g_len, s, n);
        g_len += n;
        g_report[g_len] = 0;
    }
}

const char* Basename(const char* path) {
    const char* b = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') b = p + 1;
    }
    return b;
}

// The last ~40 lines of integration.log, each scrubbed.
void PutLogTail() {
    HANDLE f = CreateFileA("integration.log", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size = {};
    GetFileSizeEx(f, &size);
    const LONGLONG cap = static_cast<LONGLONG>(sizeof(g_tail) - 1);
    const LONGLONG start = size.QuadPart > cap ? size.QuadPart - cap : 0;
    LARGE_INTEGER pos;
    pos.QuadPart = start;
    SetFilePointerEx(f, pos, nullptr, FILE_BEGIN);
    DWORD read = 0;
    ReadFile(f, g_tail, static_cast<DWORD>(cap), &read, nullptr);
    CloseHandle(f);
    g_tail[read] = 0;
    char* p = g_tail;
    if (start > 0) {   // drop the partial first line
        char* nl = strchr(p, '\n');
        p = nl ? nl + 1 : p;
    }
    int lines = 0;     // keep at most the last 40 lines
    for (char* q = p + strlen(p); q > p; --q) {
        if (q[-1] == '\n' && ++lines > 40) {
            p = q;
            break;
        }
    }
    Put("\r\n--- integration.log (last lines, personal data removed) ---\r\n");
    while (*p) {
        char* nl = strchr(p, '\n');
        const size_t n = nl ? static_cast<size_t>(nl - p) : strlen(p);
        char raw[sizeof(g_line)];
        const size_t m = n < sizeof(raw) - 1 ? n : sizeof(raw) - 1;
        memcpy(raw, p, m);
        raw[m] = 0;
        if (m && raw[m - 1] == '\r') raw[m - 1] = 0;
        CrashScrub::ScrubLine(raw, g_line, sizeof(g_line));
        Put(g_line);
        Put("\r\n");
        if (!nl) break;
        p = nl + 1;
    }
}

void WriteReport(EXCEPTION_POINTERS* ep) {
    CrashClassify::CrashFacts f;
    const EXCEPTION_RECORD* er = ep ? ep->ExceptionRecord : nullptr;
    f.code = er ? er->ExceptionCode : 0;
    const DWORD addr = er ? reinterpret_cast<DWORD>(er->ExceptionAddress) : 0;
    if (er && f.code == 0xC0000005u && er->NumberParameters >= 2) f.avAddr = static_cast<uint32_t>(er->ExceptionInformation[1]);
    HMODULE mod = nullptr;
    char path[MAX_PATH] = {};
    if (addr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(addr), &mod) && mod) {
        GetModuleFileNameA(mod, path, MAX_PATH);
        strncpy_s(f.module, Basename(path), _TRUNCATE);
    }
    f.moduleIsMain = mod && mod == GetModuleHandleA(nullptr);
    if (f.moduleIsMain) strcpy_s(f.module, "MapleStory.exe");
    f.cxxHr = static_cast<uint32_t>(g_lastCxx);
    MEMORYSTATUSEX ms = {sizeof(ms)};
    if (GlobalMemoryStatusEx(&ms)) f.availVirtual = ms.ullAvailVirtual;
    if (g_missing[0]) {
        f.missingAsset = true;
        strncpy_s(f.missingName, g_missing, _TRUNCATE);
    }

    char headline[192];
    CrashClassify::FormatHeadline(headline, sizeof(headline), f);
    g_len = 0;
    g_report[0] = 0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    sprintf_s(g_line, "SlumsMS crash report\r\nWhen: %04u-%02u-%02u %02u:%02u:%02u\r\nClient: %s (build %d)\r\n\r\n",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, SLUMS_RELEASE, SLUMS_BUILD_NUMBER);
    Put(g_line);
    Put("What happened: ");
    Put(headline);
    Put("\r\nWhat to try: ");
    Put(CrashClassify::Advice(CrashClassify::Classify(f)));
    sprintf_s(g_line, "\r\n\r\nDetails: code %08lX in %s at offset %08lX", static_cast<unsigned long>(f.code),
        f.module[0] ? f.module : "unknown", mod ? static_cast<unsigned long>(addr - reinterpret_cast<DWORD>(mod)) : 0ul);
    Put(g_line);
    if (f.code == 0xC0000005u && er) {
        sprintf_s(g_line, ", %s %08lX", er->ExceptionInformation[0] ? "writing" : "reading", static_cast<unsigned long>(f.avAddr));
        Put(g_line);
    }
    sprintf_s(g_line, "\r\nMap: %ld  Free address space: %llu MB  Last C++ error: %08lX\r\n", g_field,
        f.availVirtual == ~0ull ? 0ull : f.availVirtual >> 20, static_cast<unsigned long>(f.cxxHr));
    Put(g_line);
    PutLogTail();

    CreateDirectoryA("crash_reports", nullptr);
    HANDLE out = CreateFileA("crash_reports\\LATEST_CRASH.tmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(out, g_report, static_cast<DWORD>(g_len), &written, nullptr);
    FlushFileBuffers(out);
    CloseHandle(out);
    MoveFileExA("crash_reports\\LATEST_CRASH.tmp", "crash_reports\\LATEST_CRASH.txt", MOVEFILE_REPLACE_EXISTING);
}

LONG WINAPI SlumsTopLevelFilter(EXCEPTION_POINTERS* ep) {
    if (InterlockedCompareExchange(&g_inFilter, 1, 0) == 0) {
        __try {
            WriteReport(ep);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return g_next ? g_next(ep) : EXCEPTION_CONTINUE_SEARCH;   // the game's own filter decides what happens next
}

}  // namespace

void InstallCrashReport() {}

// From system.cpp's SetUnhandledExceptionFilter hook: remember the filter the game asked for and install ours.
LPTOP_LEVEL_EXCEPTION_FILTER CrashReport_Wrap(LPTOP_LEVEL_EXCEPTION_FILTER requested) {
    if (requested == &SlumsTopLevelFilter) return requested;
    g_next = requested;
    return &SlumsTopLevelFilter;
}

void CrashReport_SetNextFilter(LPTOP_LEVEL_EXCEPTION_FILTER next) { g_next = next; }
#ifdef MAPLEROOT_CRASH_SELFTEST
// TEST-only diagnostic (never in a release build): [clienthax] crashtest=<seconds> crashes the client that many
// seconds after it first enters a field, with a null write on a thread of its own, so QA can prove the report path.
static DWORD WINAPI CrashSelfTest(LPVOID seconds) {
    Sleep(static_cast<DWORD>(reinterpret_cast<uintptr_t>(seconds)) * 1000);
    *static_cast<volatile int*>(nullptr) = 0x5105;
    return 0;
}
#endif

void CrashReport_NoteField(int fieldId) {
    InterlockedExchange(&g_field, fieldId);
#ifdef MAPLEROOT_CRASH_SELFTEST
    static LONG armed = 0;
    if (fieldId > 0 && InterlockedCompareExchange(&armed, 1, 0) == 0) {
        const UINT seconds = GetPrivateProfileIntA("clienthax", "crashtest", 0, ".\\config.ini");
        if (seconds) CloseHandle(CreateThread(nullptr, 0, CrashSelfTest, reinterpret_cast<LPVOID>(static_cast<uintptr_t>(seconds)), 0, nullptr));
    }
#endif
}
void CrashReport_NoteCxx(uint32_t hr) { InterlockedExchange(&g_lastCxx, static_cast<LONG>(hr)); }
void CrashReport_NoteMissing(const wchar_t* path) {
    if (!path) return;
    size_t i = 0;
    for (; path[i] && i < sizeof(g_missing) - 1; ++i) g_missing[i] = path[i] < 128 ? static_cast<char>(path[i]) : '?';
    g_missing[i] = 0;
}
// The game folder is mounted before ./Data, so most misses are found a moment later: those are not the culprit.
void CrashReport_NoteFound(const wchar_t* path) {
    if (!path || !g_missing[0]) return;
    size_t i = 0;
    for (; path[i] && g_missing[i]; ++i)
        if (g_missing[i] != (path[i] < 128 ? static_cast<char>(path[i]) : '?')) return;
    if (!path[i] && !g_missing[i]) g_missing[0] = 0;
}
