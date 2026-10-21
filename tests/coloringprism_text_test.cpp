// Coloring Prism (MAPLEROOT_COLOR_PRISM): the name/description override answers for 5782000's "name" and
// "desc" only, with the server's tools/colorprism/install_wz.py text, and leaves everything else stock.
#include "../src/coloringprism_text.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {

void Check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

bool Is(const char* got, const char* want) {
    return got && want && std::strcmp(got, want) == 0;
}

}  // namespace

int main() {
    try {
        Check(Is(ColorPrism_ItemTextOverride(5782000, "name"), "Coloring Prism"), "name");
        Check(Is(ColorPrism_ItemTextOverride(5782000, "desc"),
                 "Double-click to open the Coloring Prism and dye a Cash equip, its glow, your hair or your eyes. "
                 "Each confirmed dye or restore uses one prism."), "desc");
        Check(ColorPrism_ItemTextOverride(5782000, nullptr) == nullptr, "null key");
        Check(ColorPrism_ItemTextOverride(5782000, "") == nullptr, "empty key");
        Check(ColorPrism_ItemTextOverride(5782000, "Name") == nullptr, "key is case-sensitive");
        Check(ColorPrism_ItemTextOverride(5782000, "names") == nullptr, "key prefix");
        Check(ColorPrism_ItemTextOverride(5782000, "msg") == nullptr, "other key");
        Check(ColorPrism_ItemTextOverride(5782001, "name") == nullptr, "neighbour id");
        Check(ColorPrism_ItemTextOverride(5781999, "desc") == nullptr, "neighbour id");
        Check(ColorPrism_ItemTextOverride(1302000, "name") == nullptr, "an equip");
        Check(ColorPrism_ItemTextOverride(0, "name") == nullptr, "zero");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "coloringprism_text_test: ok\n";
    return 0;
}
