#include "pch.h"
#include "mapleroot.h"
#include <windows.h>
#include <cstdio>

namespace {
#ifdef MAPLEROOT_RECV_TRACE
constexpr LONG kMaxEvents = 4000;   // diagnostic build: keep logging until the session ends
#else
constexpr LONG kMaxEvents = 16;
#endif
volatile LONG reporting = 0;
volatile LONG cppEvents = 0;
volatile LONG fatalEvents = 0;
void AppendLine(HANDLE file, const char* text) {
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
}
void AppendAddress(HANDLE file, const char* label, DWORD address) {
    HMODULE module = nullptr;
    char path[MAX_PATH] = {}, line[512] = {};
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(address), &module);
    if (module) GetModuleFileNameA(module, path, MAX_PATH);
    sprintf_s(line, "%s address=%08lX module=%s offset=%08lX\n", label, address, module ? path : "unknown",
        module ? address - reinterpret_cast<DWORD>(module) : 0);
    AppendLine(file, line);
}
LONG CALLBACK OnException(EXCEPTION_POINTERS* pointers) {
    const DWORD code = pointers->ExceptionRecord->ExceptionCode;
    const bool fatalCandidate = code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_STACK_OVERFLOW;
    const bool cpp = code == 0xE06D7363;
    if ((!fatalCandidate && !cpp) || InterlockedCompareExchange(&reporting, 1, 0))
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(cpp ? &cppEvents : &fatalEvents) > kMaxEvents) {
        InterlockedExchange(&reporting, 0);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    HANDLE file = CreateFileA("crash.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        char line[256] = {};
        sprintf_s(line, "EXCEPTION pid=%lu thread=%lu code=%08lX first_chance=1\n", GetCurrentProcessId(), GetCurrentThreadId(), code);
        AppendLine(file, line);
        AppendAddress(file, "instruction", pointers->ContextRecord->Eip);
        if (code == EXCEPTION_ACCESS_VIOLATION && pointers->ExceptionRecord->NumberParameters >= 2) {
            sprintf_s(line, "memory operation=%lu address=%08lX\n", pointers->ExceptionRecord->ExceptionInformation[0],
                pointers->ExceptionRecord->ExceptionInformation[1]);
            AppendLine(file, line);
        }
        // Record only known HRESULT-style exception codes, never packet data or strings.
        if (cpp && pointers->ExceptionRecord->NumberParameters >= 2) {
            DWORD value = 0;
            __try { value = *reinterpret_cast<DWORD*>(pointers->ExceptionRecord->ExceptionInformation[1]); }
            __except(EXCEPTION_EXECUTE_HANDLER) { value = 0; }
            if ((value >= 0x20000000 && value <= 0x2200000E) || (value & 0xFFFF0000) == 0x80040000 ||
                (value & 0xFFFF0000) == 0x80070000 || (value & 0xFFFF0000) == 0x80030000) {
                sprintf_s(line, "exception_hresult=%08lX\n", value); AppendLine(file, line);
            }
        }
        DWORD frame = pointers->ContextRecord->Ebp;
        for (int i = 0; i < 12 && frame; ++i) {
            DWORD next = 0, ret = 0;
            __try {
                next = reinterpret_cast<DWORD*>(frame)[0]; ret = reinterpret_cast<DWORD*>(frame)[1];
            } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
            AppendAddress(file, "return", ret);
            if (next <= frame || next - frame > 0x100000) break;
            frame = next;
        }
        // Optimized engine frames often omit EBP. Log only plausible code
        // addresses from the stack, never raw stack contents or object data.
        if (fatalCandidate) {
            int emitted = 0;
            for (int i = 0; i < 128 && emitted < 20; ++i) {
                DWORD candidate = 0;
                __try { candidate = reinterpret_cast<DWORD*>(pointers->ContextRecord->Esp)[i]; }
                __except(EXCEPTION_EXECUTE_HANDLER) { break; }
                MEMORY_BASIC_INFORMATION memory = {};
                if (VirtualQuery(reinterpret_cast<void*>(candidate), &memory, sizeof(memory)) &&
                    memory.State == MEM_COMMIT && (memory.Protect & 0xF0) &&
                    !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
                    AppendAddress(file, "stack_code_candidate", candidate);
                    ++emitted;
                }
            }
        }
        CloseHandle(file);
    }
    InterlockedExchange(&reporting, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}
}
void InstallCrashDiagnostics() {
    if (!AddVectoredExceptionHandler(1, OnException)) {
        OutputDebugStringA("MapleRoot integration crash diagnostics could not be installed");
    }
}
