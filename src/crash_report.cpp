// ============================================================
// crash_report.cpp -- the plain-language crash report (ticket 121). SCAFFOLD: the pure parts are done and tested
// (crash_classify.h, crash_scrub.h); the handler below records facts but does not write a report yet.
//
// Plan (research note 121, "Design"):
//  1. system.cpp hooks SetUnhandledExceptionFilter: install SlumsTopLevelFilter and remember the game's filter as
//     the next one; always chain to it, never swallow the exception.
//  2. InstallCrashReport creates a writer thread at start-up (fixed buffers, kernel32 only). The filter fills a
//     CrashFacts (code, faulting module basename via the PE headers, AV address, the last C++ throw code,
//     GlobalMemoryStatusEx), signals the thread and waits up to 5 s.
//  3. The thread writes crash_reports/LATEST_CRASH.tmp (headline, advice, raw code/module, build stamp, map id,
//     the last 50 scrubbed integration.log lines) and renames it to LATEST_CRASH.txt.
//  4. resman.cpp reports a missing Custom/add-on image by name (required_assets.cpp, later step).
// ============================================================

#include "pch.h"
#include "crash_report.h"
#include "crash_classify.h"
#include "crash_scrub.h"

namespace {
LPTOP_LEVEL_EXCEPTION_FILTER g_next = nullptr;
volatile LONG g_field = 0;
volatile LONG g_lastCxx = 0;
}  // namespace

void InstallCrashReport() {
    // TODO(121): create crash_reports\, start the writer thread, prune reports older than the last 5.
}

void CrashReport_SetNextFilter(LPTOP_LEVEL_EXCEPTION_FILTER next) { g_next = next; }
void CrashReport_NoteField(int fieldId) { InterlockedExchange(&g_field, fieldId); }
void CrashReport_NoteCxx(uint32_t hr) { InterlockedExchange(&g_lastCxx, static_cast<LONG>(hr)); }
void CrashReport_NoteMissing(const wchar_t* /*path*/) {
    // TODO(121): copy the name into a fixed buffer for CrashFacts::missingName.
}
