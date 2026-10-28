// Ticket 121: the crash classifier (crash_classify.h) and the report scrubber (crash_scrub.h).
#include "../src/crash_classify.h"
#include "../src/crash_scrub.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace CrashClassify;

namespace {
void Check(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}
CrashFacts F(uint32_t code, const char* module = "", uint32_t hr = 0) {
    CrashFacts f;
    f.code = code;
    std::strncpy(f.module, module, sizeof(f.module) - 1);
    f.moduleIsMain = std::strcmp(module, "MapleStory.exe") == 0;
    f.cxxHr = hr;
    return f;
}
std::string Scrub(const char* s) {
    char buf[256];
    CrashScrub::ScrubLine(s, buf, sizeof(buf));
    return buf;
}
}  // namespace

int main() {
    try {
        Check(Classify(F(0xC0000005, "nvd3dumx.dll")) == Cause::GraphicsDriver, "driver AV");
        Check(Classify(F(0xC0000005, "Gr2D_DX9.dll")) == Cause::GraphicsDriver, "v83 renderer AV");
        Check(Classify(F(0xC0000005, "RTSSHooks.dll")) == Cause::Overlay, "overlay");
        Check(Classify(F(0xC0000005, "slums.dll")) == Cause::Addon, "our dll");
        Check(Classify(F(0xC0000005, "mss32.dll")) == Cause::Sound, "sound");
        Check(Classify(F(0xC0000005, "PCOM.dll")) == Cause::DataLibrary, "data library");
        Check(Classify(F(0xC00000FD, "MapleStory.exe")) == Cause::StackOverflow, "stack");
        Check(Classify(F(0xC0000374, "ntdll.dll")) == Cause::HeapCorruption, "heap");
        Check(Classify(F(0xE06D7363, "KERNELBASE.dll", 0x21000003)) == Cause::Disconnected, "disconnect");
        Check(Classify(F(0xE06D7363, "KERNELBASE.dll", 0x22000005)) == Cause::MissingData, "ZException missing data");
        Check(Classify(F(0xE06D7363, "KERNELBASE.dll", 0x12345678)) == Cause::InternalError, "other throw");
        CrashFacts nul = F(0xC0000005, "MapleStory.exe");
        nul.avAddr = 0x14;
        Check(Classify(nul) == Cause::NullObject, "null object");
        nul.avAddr = 0x12345678;
        Check(Classify(nul) == Cause::AccessViolation, "plain AV");
        CrashFacts oom = F(0xC0000005, "nvd3dumx.dll");   // low memory outranks the driver
        oom.availVirtual = 10ull << 20;
        Check(Classify(oom) == Cause::OutOfMemory, "OOM first");
        Check(ClassOfModule("") == ModuleClass::None && ClassOfModule("random.dll") == ModuleClass::Other, "module classes");

        char h[160];
        FormatHeadline(h, sizeof(h), F(0xC0000005, "nvd3dumx.dll"));
        Check(std::string(h) == "The graphics driver crashed (nvd3dumx.dll).", std::string("headline: ") + h);
        CrashFacts miss = F(0xE06D7363, "KERNELBASE.dll", 0x22000004);
        miss.missingAsset = true;
        std::strcpy(miss.missingName, "Custom.wz/String/MonsterBookEx.img");
        FormatHeadline(h, sizeof(h), miss);
        Check(std::string(h) == "A game data file is missing or damaged (Custom.wz/String/MonsterBookEx.img).", h);
        Check(std::strlen(Advice(Cause::Overlay)) > 0, "advice");

        Check(Scrub("C:\\Users\\Josh\\Games\\MS\\slums.dll") == "<user>\\Games\\MS\\slums.dll", Scrub("C:\\Users\\Josh\\Games\\MS\\slums.dll"));
        Check(Scrub("d:/users/someone/x") == "<user>/x", "forward slashes");
        Check(Scrub("connect 192.168.0.164:8484 ok") == "connect <ip>:8484 ok", Scrub("connect 192.168.0.164:8484 ok"));
        Check(Scrub("version 1.6.0.2 build") == "version 1.6.0.2 build", "a four-part version is not an IP");
        Check(Scrub("[SlumsMS] Dialog: hi Momo, meet me in FM") == "[dialog line removed]", "dialog");
        char tiny[8];
        Check(CrashScrub::ScrubLine("abcdefghijk", tiny, sizeof(tiny)) == 7 && std::string(tiny) == "abcdefg", "truncates");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "crash report: ok\n";
    return 0;
}
