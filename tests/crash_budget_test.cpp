// b334: crash.log kept only the first 16 C++ throws per run, and sound start-up used all 16, so a game error later in
// the session (the b331 kind) was never recorded.
#include "../src/crash_budget.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace CrashBudget;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        Check(IsGameErrorHr(0x21000002u) && IsGameErrorHr(0x2200000Eu) && IsGameErrorHr(0x20000000u), "game error range");
        Check(!IsGameErrorHr(0x80004005u) && !IsGameErrorHr(0x1FFFFFFFu) && !IsGameErrorHr(0x2200000Fu), "outside the range");

        Budget b;
        // Sound start-up: SOUND_DX8 throws plain C++ exceptions (no game HRESULT) until the cpp budget is gone.
        for (int i = 0; i < 40; ++i) b.Take(KindOf(false, true, 0));
        Check(!b.Take(KindOf(false, true, 0)), "the plain C++ budget still runs out");
        // A game error later in the session must still be recorded.
        Check(KindOf(false, true, 0x21000002u) == Kind::GameError, "a ZException-range throw is a game error");
        Check(b.Take(KindOf(false, true, 0x21000002u)), "a game error after start-up noise is still logged");
        // And fatal exceptions keep their own budget.
        Check(b.Take(KindOf(true, false, 0)), "a fatal exception after start-up noise is still logged");
        // The game-error budget is bounded too.
        Budget g;
        int taken = 0;
        for (int i = 0; i < 100; ++i) taken += g.Take(Kind::GameError) ? 1 : 0;
        Check(taken == 32, "game errors keep 32 entries");
    } catch (const std::exception& e) {
        std::cerr << "crash budget test FAILED: " << e.what() << "\n";
        return 1;
    }
    std::cout << "crash budget ok\n";
    return 0;
}
