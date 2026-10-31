#pragma once
// slums.dll build stamp, header-only so the native tests can check the bytes without the client.
// CP 0x3738 CLIENT_BUILD_STAMP, sent right before every CP_CheckPassword (CLogin::SendCheckPasswordPacket_hook),
// on the same connection, so the login server always has it before it checks the password. After the opcode:
//   format(2) = 2, build(4), release(u16 len + ASCII), caps(4)   little-endian, no reply
// Format 1 (1.4.2 to 1.6.1-rc4) had no caps; the server still accepts it and falls back to the build number.
// caps says which custom data sets this client can load, checked at login (bypass.cpp ClientDataCaps):
//   bit 0  kCapCustomNpcData  Npc/9990000.img (Slums-chan) and Npc/0002985-0002999.img (Lazy Penguins) all resolve.
//   bit 31 kCapQaBuild        a QA client (SLUMS_QA_MODULE, ticket 166): TEST and RC accept it, live refuses it.
// The server sends custom NPC spawns only to clients with bit 0: a client without the img leaves the game on the
// spawn (COM error 0x80030002), 2026-10-28.
// build is monotonic (VERSION major*10000 + minor*100 + patch, cmake/BuildStamp.cmake); the server refuses logins
// below MIN_CLIENT_BUILD. Clients built before this (1.4.1 and older) never send it and count as build 0.
// The server's net.server.handlers.login.ClientBuildStampHandler reads these bytes.
#include <cstdint>

#ifndef SLUMS_BUILD_NUMBER
#error "SLUMS_BUILD_NUMBER is set by cmake/BuildStamp.cmake"
#endif
#ifndef SLUMS_RELEASE
#error "SLUMS_RELEASE is set by cmake/BuildStamp.cmake"
#endif

constexpr uint16_t kBuildStampOpcode = 0x3738;
constexpr uint16_t kBuildStampFormat = 2;
constexpr uint32_t kCapCustomNpcData = 1u << 0;
constexpr uint32_t kCapQaBuild       = 1u << 31;
#ifdef SLUMS_QA_MODULE
constexpr bool kSlumsQaBuild = true;
#else
constexpr bool kSlumsQaBuild = false;
#endif
constexpr int32_t  kSlumsBuildNumber = SLUMS_BUILD_NUMBER;
constexpr const char* kSlumsRelease  = SLUMS_RELEASE;

static_assert(kSlumsBuildNumber > 0, "the build number must be positive (0 means 'no stamp' to the server)");

// Writes the body after the opcode. Packet is COutPacket in the client, a byte recorder in the tests.
template <typename Packet>
void BuildStamp_EncodeValues(Packet& packet, int32_t build, const char* release, uint32_t caps) {
    packet.Encode2(kBuildStampFormat);
    packet.Encode4(static_cast<uint32_t>(build));
    packet.EncodeStr(release);
    packet.Encode4(caps);
}

template <typename Packet>
void BuildStamp_Encode(Packet& packet, uint32_t caps) {
    BuildStamp_EncodeValues(packet, kSlumsBuildNumber, kSlumsRelease, caps);
}

// The custom NPC imgs bit 0 stands for, as resource paths (Npc/9990000.img, Npc/0002985.img .. Npc/0002999.img).
inline constexpr int kCustomNpcIds[] = {9990000, 2985, 2986, 2987, 2988, 2989, 2990, 2991, 2992, 2993, 2994, 2995,
    2996, 2997, 2998, 2999};
