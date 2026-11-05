#pragma once
// Which login server the client connects to (b301).
//
// config.ini is player-owned: the launcher writes it only when it is missing, so an install keeps the server of the
// channel it was first installed from (an RC install on Live's 192.168.0.164, or back). Each client layer now ships
// server.ini, which the launcher replaces like any other layer file:
//
//   [server]
//   host=192.168.0.111
//   port=8489
//
// server.ini wins when it names both a host and a usable port; otherwise config.ini [config] host/port is read exactly
// as before (older layers, hand-made folders). Header-only, no Windows: the reader is passed in (GetPrivateProfileStringA
// in injector.cpp, a table in tests/server_endpoint_test.cpp).
#include <cstddef>
#include <cstdlib>

namespace ServerEndpoint {

constexpr const char* kServerFile = "server.ini";
constexpr const char* kConfigFile = "config.ini";

struct Endpoint {
    char host[256] = {};
    long port = 0;
    bool fromServerFile = false;
};

// Read(file, section, key, buffer, size) returns the length copied (0 = missing or empty), like
// GetPrivateProfileStringA with an empty default.
template <typename Read>
bool Resolve(Read read, Endpoint& out) {
    char port[32] = {};
    Endpoint e;
    if (read(kServerFile, "server", "host", e.host, sizeof(e.host)) > 0 &&
        read(kServerFile, "server", "port", port, sizeof(port)) > 0) {
        char* end = nullptr;
        long n = std::strtol(port, &end, 10);
        if (end != port && *end == '\0' && n > 0 && n <= 65535) {
            e.port = n;
            e.fromServerFile = true;
            out = e;
            return true;
        }
    }
    e = Endpoint();
    if (read(kConfigFile, "config", "host", e.host, sizeof(e.host)) == 0) {
        return false;
    }
    if (read(kConfigFile, "config", "port", port, sizeof(port)) > 0) {
        e.port = std::atol(port);
    }
    out = e;
    return true;
}

}  // namespace ServerEndpoint
