#include "../src/avatar_features.h"
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    for (bool mapleRootChairs : {false, true}) {
        bool storageReady = false;
        std::string order;
        integration::InstallAvatarFeatures(mapleRootChairs,
            [&] { storageReady = true; order += "storage;"; },
            [&] { if (!storageReady) throw std::runtime_error("chair storage missing"); order += "chairs;"; },
            [&] { if (!storageReady) throw std::runtime_error("item effect storage missing"); order += "effects;"; });
        const std::string expected = mapleRootChairs ? "storage;effects;" : "storage;chairs;effects;";
        if (order != expected) throw std::runtime_error("avatar feature dependency order changed");
    }
    bool effectsRan = false;
    try {
        integration::InstallAvatarFeatures(true,
            [] { throw std::runtime_error("storage hook failure"); }, [] {}, [&] { effectsRan = true; });
    } catch (const std::runtime_error&) {}
    if (effectsRan) throw std::runtime_error("effects installed after storage initialization failed");
    std::cout << "PASS: avatar storage precedes item effects in both modes; MapleRoot chair owner retained\n";
}
