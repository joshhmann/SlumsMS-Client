#pragma once
// wztrace.cpp: the QA client's WZ lookup trace (ticket b251). Built only with SLUMS_QA_MODULE; release builds get
// the empty inline versions below, so the call sites need no #ifdef and cost nothing. The switch is config.ini
// [debug] wztrace (0 off: not even a hook, 1 failed lookups, 2 also the ring of recent lookups and every throw).
#ifdef SLUMS_QA_MODULE
// hook.h AttachClientHooks: reads the switch and, at level 2 only, detours _CxxThrowException and MakeTile.
void WzTrace_Install();
// resman.cpp's namespace hook: every local-object lookup the WZ manager answers, with its HRESULT. Called after the
// original, so the HRESULT is the one the client acts on. Cheap when off: one cached integer compare.
void WzTrace_NoteLookup(const wchar_t* path, long hr);
#else
inline void WzTrace_Install() {}
inline void WzTrace_NoteLookup(const wchar_t*, long) {}
#endif
