#include "pch.h"
#include "mapleroot.h"
#include "debug.h"
#include "hook.h"
#include "wvs/util.h"
#include <cstdio>
// The MapleRoot companion is linked into slums.dll (it used to be a separate ijl15.dll loaded through this bridge).
extern "C" {
unsigned __cdecl MapleRoot_GetBridgeVersion();
BOOL __cdecl MapleRoot_Initialize(int width, int height);
void __cdecl MapleRoot_InitializeResources(void* app);
void __cdecl MapleRoot_SetResolution(int width, int height);
void __cdecl MapleRoot_Shutdown();
void __cdecl MapleRoot_SetModule(HMODULE module);
}
namespace {
void Trace(const char* text) {
    FILE* file = nullptr;
    if (fopen_s(&file, "integration.log", "a") == 0 && file) {
        fprintf(file, "[SlumsMS] %s\n", text);
        fclose(file);
    }
}
void Fail(const char* text) {
    Trace(text);
    ErrorMessage("MapleRoot integration: %s", text);
    ExitProcess(ERROR_DLL_INIT_FAILED);
}
}
void VerifyKaentakeHooks(const char* group) {
    if (GetKaentakeHookFailureCount() != 0) {
        DebugMessage("Required hook group failed: %s", group);
        Fail("A required Kaentake hook failed; see integration.log");
    }
}
unsigned long BuildTimestamp(HMODULE module) {
    if (!module) return 0;
    auto bytes = reinterpret_cast<unsigned char*>(module);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(bytes + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp;
}
void InitializeMapleRoot() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&InitializeMapleRoot), &self);
    MapleRoot_SetModule(self);
    SYSTEMTIME utc; GetSystemTime(&utc);
    DebugMessage("SESSION_START pid=%lu utc=%04u-%02u-%02uT%02u:%02u:%02uZ slums_build=%08lX",
        GetCurrentProcessId(), utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond,
        BuildTimestamp(self));
    VerifyKaentakeHooks("system and client bypass");
    if (MapleRoot_GetBridgeVersion() != 0x00010001) Fail("The MapleRoot companion has the wrong bridge version");
    Trace("Initializing preserved MapleRoot feature groups");
    if (!MapleRoot_Initialize(get_screen_width(), get_screen_height())) Fail("Feature initialization failed");
    Trace("MapleRoot feature groups initialized");
    Trace("SESSION_FEATURES_READY");
}
void InitializeMapleRootResources(void* app) {
    MapleRoot_InitializeResources(app);
    Trace("MapleRoot Data namespace ready for Custom.wz overlay");
}
void UpdateMapleRootResolution(int width, int height) {
    MapleRoot_SetResolution(width, height);
}
void ShutdownMapleRoot() {
    MapleRoot_Shutdown();
}
