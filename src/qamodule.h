#pragma once
// qamodule.cpp: the QA client's runner channel (ticket 166). Built only with SLUMS_QA_MODULE; release builds get the
// empty inline versions below, so the call sites need no #ifdef.
#include <cstdint>

struct IDirect3DDevice9;

#ifdef SLUMS_QA_MODULE
void QaModule_Start();                          // system.cpp: opens the pipes once, on the hook-install thread
void QaModule_OnPacket(uint16_t opcode);        // packetdispatcher.cpp: after the client handled a server packet
void QaModule_OnFrame(IDirect3DDevice9* dev);   // d3d9host.cpp: every EndScene, after the overlay
void QaModule_OnDeviceReset();                  // d3d9host.cpp: before Reset (drops the capture surface)
#else
inline void QaModule_Start() {}
inline void QaModule_OnPacket(uint16_t) {}
inline void QaModule_OnFrame(IDirect3DDevice9*) {}
inline void QaModule_OnDeviceReset() {}
#endif
