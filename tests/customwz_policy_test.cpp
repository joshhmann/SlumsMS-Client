// Custom.wz overlay: which part of a path names the image (customwz_policy.h).
#include "../src/customwz_policy.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace CustomWz;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        Check(ImagePrefix(L"Effect/BasicEff.img/NoRed1/0") == L"Effect/BasicEff.img", "digit path");
        Check(ImagePrefix(L"Effect/BasicEff.img") == L"Effect/BasicEff.img", "the image itself");
        Check(ImagePrefix(L"Item/Cash/0578.img/05782000/info/icon") == L"Item/Cash/0578.img", "nested directory");
        Check(ImagePrefix(L"Mob") == L"" && ImagePrefix(L"Item/Cash") == L"", "directories have no image");
        Check(ImagePrefix(L"UI/.img/x") == L"", "a bare .img name is not an image");
        Check(ImagePrefix(L"") == L"", "empty");
        Check(TopDir(L"Mob/8880150.img") == L"Mob" && TopDir(L"Mob") == L"", "top directory");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "custom wz policy: ok\n";
    return 0;
}
