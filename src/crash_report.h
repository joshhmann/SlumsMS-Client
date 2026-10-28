#pragma once
// crash_report.cpp (ticket 121, MAPLEROOT_CRASH_REPORT): on a real (last-chance) crash, write
// crash_reports/LATEST_CRASH.txt with a plain-language cause (crash_classify.h) and a scrubbed log tail
// (crash_scrub.h). The launcher shows it and sends it only when the player clicks Send.
#include <cstdint>
#include <windows.h>

void InstallCrashReport();
LPTOP_LEVEL_EXCEPTION_FILTER CrashReport_Wrap(LPTOP_LEVEL_EXCEPTION_FILTER requested);   // system.cpp: ours in front of the game's                                         // from DllMain setup: paths, writer thread, buffers
void CrashReport_SetNextFilter(LPTOP_LEVEL_EXCEPTION_FILTER next);  // system.cpp's SetUnhandledExceptionFilter hook
void CrashReport_NoteField(int fieldId);                            // resolution.cpp set_stage_hook: the current map
void CrashReport_NoteCxx(uint32_t hr);                              // crash_diagnostics.cpp: the last C++ throw code
void CrashReport_NoteMissing(const wchar_t* path);                  // resman.cpp: a data lookup that failed
