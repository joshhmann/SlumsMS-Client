#pragma once
// ============================================================
// crash_budget.h -- which first-chance exceptions crash.log still records (b334).
//
// crash_diagnostics.cpp writes a stack for the first few exceptions of each kind and then stays quiet, so a client
// that throws thousands of harmless C++ exceptions cannot fill the disk. Header-only so tests/crash_budget_test.cpp
// checks it without the client.
// ============================================================

#include <cstdint>

namespace CrashBudget {

enum class Kind { Fatal, Cpp, GameError, Count };

// A game error is a C++ throw whose HRESULT is in the ZException range the client's own errors use
// (0x20000000 patch, 0x21000000-0x21000006 disconnect, 0x22000000-0x2200000E terminate; wvs/exception.h).
inline bool IsGameErrorHr(uint32_t hr) { return hr >= 0x20000000u && hr <= 0x2200000Eu; }

inline Kind KindOf(bool fatalCandidate, bool cpp, uint32_t hr) {
    if (fatalCandidate) return Kind::Fatal;
    if (cpp) return Kind::Cpp;
    return Kind::Fatal;
}

constexpr int kLimit[static_cast<int>(Kind::Count)] = {16, 16, 16};

struct Budget {
    int used[static_cast<int>(Kind::Count)] = {};
    // True while this kind may still be written to crash.log.
    bool Take(Kind kind) {
        int& n = used[static_cast<int>(kind)];
        if (n >= kLimit[static_cast<int>(kind)]) return false;
        ++n;
        return true;
    }
};

}  // namespace CrashBudget
