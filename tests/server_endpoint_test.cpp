// Which server the client connects to (server_endpoint.h, b301): server.ini before the player's config.ini.
#include "../src/server_endpoint.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

using Files = std::map<std::string, std::string>;  // "file|section|key" -> value

ServerEndpoint::Endpoint Resolve(const Files& files, bool expect = true) {
    auto read = [&](const char* file, const char* section, const char* key, char* buffer, size_t size) -> size_t {
        auto it = files.find(std::string(file) + "|" + section + "|" + key);
        buffer[0] = '\0';
        if (it == files.end() || size == 0) return 0;
        size_t n = it->second.copy(buffer, size - 1);
        buffer[n] = '\0';
        return n;
    };
    ServerEndpoint::Endpoint e;
    Check(ServerEndpoint::Resolve(read, e) == expect, "resolve result");
    return e;
}

const Files kLiveConfig = {{"config.ini|config|host", "slumsms.asslorde.com"}, {"config.ini|config|port", "8485"}};
}  // namespace

int main() {
    try {
        // Live install switched to the RC channel: the player's config.ini still says Live, the layer's server.ini TEST.
        Files rc = kLiveConfig;
        rc["server.ini|server|host"] = "192.168.0.111";
        rc["server.ini|server|port"] = "8489";
        auto e = Resolve(rc);
        Check(std::strcmp(e.host, "192.168.0.111") == 0 && e.port == 8489 && e.fromServerFile, "server.ini wins");

        // Back on a layer without server.ini (Live 1.6.2): config.ini exactly as before.
        e = Resolve(kLiveConfig);
        Check(std::strcmp(e.host, "slumsms.asslorde.com") == 0 && e.port == 8485 && !e.fromServerFile, "config.ini fallback");

        // A server.ini without a usable port is ignored as a whole (no host from one file and port from the other).
        for (const char* bad : {"", "abc", "0", "70000", "8489x"}) {
            Files f = kLiveConfig;
            f["server.ini|server|host"] = "192.168.0.111";
            if (*bad) f["server.ini|server|port"] = bad;
            e = Resolve(f);
            Check(std::strcmp(e.host, "slumsms.asslorde.com") == 0 && e.port == 8485, std::string("bad port ") + bad);
        }

        // A port without a host in server.ini is ignored too.
        Files f = kLiveConfig;
        f["server.ini|server|port"] = "8489";
        e = Resolve(f);
        Check(std::strcmp(e.host, "slumsms.asslorde.com") == 0 && e.port == 8485, "port without host");

        // config.ini host without port keeps the old behaviour (port 0); no host anywhere resolves nothing.
        e = Resolve({{"config.ini|config|host", "127.0.0.1"}});
        Check(std::strcmp(e.host, "127.0.0.1") == 0 && e.port == 0, "config host without port");
        Resolve({}, false);

        std::cout << "server_endpoint: server.ini before config.ini OK\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "server_endpoint_test FAILED: " << ex.what() << "\n";
        return 1;
    }
}
