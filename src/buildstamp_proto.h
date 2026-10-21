#pragma once
// slums.dll build stamp, header-only so the native tests can check the bytes without the client.
// CP 0x3738 CLIENT_BUILD_STAMP, sent right before every CP_CheckPassword (CLogin::SendCheckPasswordPacket_hook),
// on the same connection, so the login server always has it before it checks the password. After the opcode:
//   format(2) = 1, build(4), release(u16 len + ASCII)            little-endian, no reply
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
constexpr uint16_t kBuildStampFormat = 1;
constexpr int32_t  kSlumsBuildNumber = SLUMS_BUILD_NUMBER;
constexpr const char* kSlumsRelease  = SLUMS_RELEASE;

static_assert(kSlumsBuildNumber > 0, "the build number must be positive (0 means 'no stamp' to the server)");

// Writes the body after the opcode. Packet is COutPacket in the client, a byte recorder in the tests.
template <typename Packet>
void BuildStamp_EncodeValues(Packet& packet, int32_t build, const char* release) {
    packet.Encode2(kBuildStampFormat);
    packet.Encode4(static_cast<uint32_t>(build));
    packet.EncodeStr(release);
}

template <typename Packet>
void BuildStamp_Encode(Packet& packet) {
    BuildStamp_EncodeValues(packet, kSlumsBuildNumber, kSlumsRelease);
}
