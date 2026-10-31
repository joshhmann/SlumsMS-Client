#pragma once
// qamodule.cpp: the QA client's runner channel (ticket 166). Built only with SLUMS_QA_MODULE; release builds get the
// empty inline versions below, so the call sites need no #ifdef.
#include <cstddef>
#include <cstdint>

struct IDirect3DDevice9;

#ifdef SLUMS_QA_MODULE
void QaModule_Start();   // system.cpp: opens the pipes once, on the hook-install thread
// packetdispatcher.cpp: called with the whole server packet (opcode first) once the client has handled it. The QA
// module reads the packet read-only for the chat lines and the screens a packet asks for.
void QaModule_OnPacket(uint16_t opcode, const void* packet, size_t size);
void QaModule_OnFrame(IDirect3DDevice9* dev);   // d3d9host.cpp: every EndScene, after the overlay
void QaModule_OnDeviceReset();                  // d3d9host.cpp: before Reset (drops the capture surface)
#else
inline void QaModule_Start() {}
inline void QaModule_OnPacket(uint16_t, const void*, size_t) {}
inline void QaModule_OnFrame(IDirect3DDevice9*) {}
inline void QaModule_OnDeviceReset() {}
#endif
