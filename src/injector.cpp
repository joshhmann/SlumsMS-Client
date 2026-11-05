#include "pch.h"
#include "hook.h"
#include "constants.h"
#include "server_endpoint.h"
#include "ztl/ztl.h"
#include <windows.h>

ZALLOC_GLOBAL
ZALLOCEX(ZAllocAnonSelector, 0x00BF0B00)
ZALLOCEX(ZAllocStrSelector<char>, 0x00BF0A90)
ZALLOCEX(ZAllocStrSelector<wchar_t>, 0x00BF0BA8)

extern "C" __declspec(dllexport) VOID DummyExport() {}

char* g_sServerHost = nullptr;
long g_nServerPort = 0;

void ProcessCommandLine() {
    if (!CONSTANTS_USE_COMMAND_LINE) {
        return;
    }
    char sBuffer[1024];
    long nPort;
    if (sscanf_s(GetCommandLineA(), "%s %d", sBuffer, sizeof(sBuffer), &nPort) == 2) {
        g_sServerHost = _strdup(sBuffer);
        g_nServerPort = nPort;
    }
}

void ProcessConfigFile() {
    if (!CONSTANTS_USE_CONFIG_FILE || g_sServerHost) {
        return;
    }
    // server.ini (layer-owned, per launcher channel) before config.ini (player-owned): server_endpoint.h, b301.
    ServerEndpoint::Endpoint endpoint;
    auto read = [](const char* file, const char* section, const char* key, char* buffer, size_t size) -> size_t {
        char path[MAX_PATH];
        sprintf_s(path, ".\\%s", file);
        return GetPrivateProfileStringA(section, key, "", buffer, static_cast<DWORD>(size), path);
    };
    if (!ServerEndpoint::Resolve(read, endpoint)) {
        return;
    }
    g_sServerHost = _strdup(endpoint.host);
    g_nServerPort = endpoint.port;
}


BOOL WINAPI DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        ProcessCommandLine();
        ProcessConfigFile();
        AttachSystemHooks();
        break;
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}