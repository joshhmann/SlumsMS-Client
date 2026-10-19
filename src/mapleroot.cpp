#include "pch.h"
#include "mapleroot.h"
#include "debug.h"
#include "hook.h"
#include "wvs/util.h"
#include <cstdio>
namespace {
using Version = unsigned(__cdecl*)();
using Initialize = BOOL(__cdecl*)(int, int);
using Resources = void(__cdecl*)(void*);
using Resolution = void(__cdecl*)(int, int);
using Shutdown = void(__cdecl*)();
Resources resources;
Resolution resolution;
Shutdown shutdownRoot;
void Trace(const char* text) {
    FILE* file = nullptr;
    if (fopen_s(&file, "integration.log", "a") == 0 && file) {
        fprintf(file, "[Kaentake] %s\n", text);
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
    HMODULE module = GetModuleHandleA("ijl15.dll");
    if (!module) module = LoadLibraryA("ijl15.dll");
    if (!module) Fail("Cannot load the integrated ijl15.dll");
    HMODULE kaentake = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&InitializeMapleRoot), &kaentake);
    SYSTEMTIME utc; GetSystemTime(&utc);
    DebugMessage("SESSION_START pid=%lu utc=%04u-%02u-%02uT%02u:%02u:%02uZ kaentake_build=%08lX mapleroot_build=%08lX",
        GetCurrentProcessId(), utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond,
        BuildTimestamp(kaentake), BuildTimestamp(module));
    VerifyKaentakeHooks("system and client bypass");
    auto version = reinterpret_cast<Version>(GetProcAddress(module, "MapleRoot_GetBridgeVersion"));
    auto initialize = reinterpret_cast<Initialize>(GetProcAddress(module, "MapleRoot_Initialize"));
    resources = reinterpret_cast<Resources>(GetProcAddress(module, "MapleRoot_InitializeResources"));
    resolution = reinterpret_cast<Resolution>(GetProcAddress(module, "MapleRoot_SetResolution"));
    shutdownRoot = reinterpret_cast<Shutdown>(GetProcAddress(module, "MapleRoot_Shutdown"));
    if (!version || version() != 0x00010001 || !initialize || !resources || !resolution || !shutdownRoot)
        Fail("The companion DLL has the wrong bridge version");
    Trace("Initializing preserved MapleRoot feature groups");
    if (!initialize(get_screen_width(), get_screen_height())) Fail("Feature initialization failed");
    Trace("MapleRoot feature groups initialized");
    Trace("SESSION_FEATURES_READY");
}
void InitializeMapleRootResources(void* app) {
    if (!resources) Fail("Resource bridge was not initialized");
    resources(app);
    Trace("MapleRoot Data namespace ready for Custom.wz overlay");
}
void UpdateMapleRootResolution(int width, int height) {
    if (resolution) resolution(width, height);
}
void ShutdownMapleRoot() {
    if (shutdownRoot) shutdownRoot();
}
