#include "pch.h"
#include "debug.h"
#include <windows.h>
#include <strsafe.h>
#include <cstdio>


void DebugMessage(const char* pszFormat, ...) {
    char pszDest[1024];
    size_t cbDest = 1024 * sizeof(char);
    va_list argList;
    va_start(argList, pszFormat);
    StringCbVPrintfA(pszDest, cbDest, pszFormat, argList);
#ifdef MAPLEROOT_INTEGRATION
    FILE* file = nullptr;
    if (fopen_s(&file, "integration.log", "a") == 0 && file) {
        fprintf(file, "[Kaentake] %s\n", pszDest);
        fclose(file);
    }
#endif
    OutputDebugStringA(pszDest);
    va_end(argList);
}

void ErrorMessage(const char* pszFormat, ...) {
    char pszDest[1024];
    size_t cbDest = 1024 * sizeof(char);
    va_list argList;
    va_start(argList, pszFormat);
    StringCbVPrintfA(pszDest, cbDest, pszFormat, argList);
    MessageBoxA(nullptr, pszDest, "Error", MB_ICONERROR);
    va_end(argList);
}