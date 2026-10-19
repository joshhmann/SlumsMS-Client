#pragma once

#ifdef _DEBUG
#define DEBUG_MESSAGE(FORMAT, ...) DebugMessage(FORMAT, __VA_ARGS__)
#else
#define DEBUG_MESSAGE(FORMAT, ...)
#endif


void DebugMessage(const char* sFormat, ...);

void ErrorMessage(const char* sFormat, ...);
// Bounded diagnostics for the visual weather modules, retained in Release builds.
#ifndef LOG_ONCE
#define LOG_ONCE(...) do { static bool logged = false; if (!logged) { logged = true; DebugMessage(__VA_ARGS__); } } while (0)
#endif
#ifndef LOG_ONCE_PER_ID
#define LOG_ONCE_PER_ID(KEY, ...) do { \
    static int keys[64] = {}; static int count = 0; int key = (KEY); bool seen = false; \
    for (int i = 0; i < count; ++i) if (keys[i] == key) seen = true; \
    if (!seen && count < 64) { keys[count++] = key; DebugMessage(__VA_ARGS__); } \
} while (0)
#endif
