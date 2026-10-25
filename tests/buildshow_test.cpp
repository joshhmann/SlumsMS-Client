// How the build is shown (buildshow_text.h): the window title and the login-screen label.
#include "../src/buildshow_text.h"
#include <cstring>
#include <cwchar>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
}  // namespace

int main() {
    try {
        char title[96];
        BuildShow::WindowTitle(title, sizeof(title), "1.6.0", 10600);
        Check(std::strcmp(title, "SlumsMS 1.6.0 (10600)") == 0, "window title");
        BuildShow::WindowTitle(title, sizeof(title), "1.6.0-rc1", 10600);
        Check(std::strcmp(title, "SlumsMS 1.6.0-rc1 (10600)") == 0, "rc title keeps the suffix");

        wchar_t label[96];
        BuildShow::LoginLabel(label, 96, "1.6.0", 10600);
        Check(std::wcscmp(label, L"SlumsMS 1.6.0 · build 10600") == 0, "login label with a middle dot");
        wchar_t small[8];
        BuildShow::LoginLabel(small, 8, "1.6.0", 10600);
        Check(std::wcslen(small) == 7 && std::wcsncmp(small, L"SlumsMS", 7) == 0, "truncates inside the buffer");
        std::cout << "buildshow: title and login label OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "buildshow_test FAILED: " << e.what() << "\n";
        return 1;
    }
}
