#pragma once
// ============================================================
// d3d9host_policy.h -- what the D3D9 host does when the device cannot be created (1.6.0, ticket 39/90).
//
// Header-only so tests/d3d9host_policy_test.cpp can check it without Windows; d3d9host.cpp uses it.
//
// The client renders through d3d8to9 (d3d8.dll imports d3d9!Direct3DCreate9), so every device it makes is an
// IDirect3D9::CreateDevice call. With a virtual display (Parsec, Sunshine, Virtual Desktop) as the primary, the
// client fails at start with -2147467259 (E_FAIL). Which call fails is not known yet; d3d9host.cpp logs every
// failing IDirect3D9 check and the adapters. If CreateDevice itself fails, it retries: the other adapters with the
// same flags, then every adapter (the requested one first) with software vertex processing.
// ============================================================

#include <cstdint>
#include <vector>

namespace D3D9Host {

constexpr std::uint32_t kSoftwareVP = 0x20;   // D3DCREATE_SOFTWARE_VERTEXPROCESSING
constexpr std::uint32_t kHardwareVP = 0x40;   // D3DCREATE_HARDWARE_VERTEXPROCESSING
constexpr std::uint32_t kMixedVP    = 0x80;   // D3DCREATE_MIXED_VERTEXPROCESSING

struct Attempt {
    unsigned adapter;
    std::uint32_t behavior;
    bool operator==(const Attempt& o) const { return adapter == o.adapter && behavior == o.behavior; }
};

inline std::uint32_t WithSoftwareVP(std::uint32_t behavior) {
    return (behavior & ~(kHardwareVP | kMixedVP)) | kSoftwareVP;
}

// The retries after CreateDevice(requested, behavior) failed, in order. Never repeats the failed call.
inline std::vector<Attempt> FallbackAttempts(unsigned requested, unsigned adapterCount, std::uint32_t behavior) {
    std::vector<Attempt> out;
    for (unsigned a = 0; a < adapterCount; ++a) {
        if (a != requested) out.push_back({a, behavior});
    }
    const std::uint32_t sw = WithSoftwareVP(behavior);
    if (sw != behavior) {
        if (requested < adapterCount) out.push_back({requested, sw});
        for (unsigned a = 0; a < adapterCount; ++a) {
            if (a != requested) out.push_back({a, sw});
        }
    }
    return out;
}

// A readable name for the HRESULTs D3D9 returns, for the log.
inline const char* HrName(std::uint32_t hr) {
    switch (hr) {
    case 0x00000000: return "S_OK";
    case 0x80004005: return "E_FAIL";
    case 0x8007000E: return "E_OUTOFMEMORY";
    case 0x80070057: return "E_INVALIDARG";
    case 0x88760818: return "D3DERR_WRONGTEXTUREFORMAT";
    case 0x88760827: return "D3DERR_DRIVERINTERNALERROR";
    case 0x88760866: return "D3DERR_NOTFOUND";
    case 0x88760868: return "D3DERR_DEVICELOST";
    case 0x88760869: return "D3DERR_DEVICENOTRESET";
    case 0x8876086A: return "D3DERR_NOTAVAILABLE";
    case 0x8876086C: return "D3DERR_INVALIDCALL";
    case 0x8876017C: return "D3DERR_OUTOFVIDEOMEMORY";
    case 0x88760870: return "D3DERR_DEVICEREMOVED";
    default: return "?";
    }
}

}  // namespace D3D9Host
