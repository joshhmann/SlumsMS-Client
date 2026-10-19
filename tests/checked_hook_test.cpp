#include "../src/checked_hook.h"
#include <iostream>
#include <stdexcept>
#include <string>

struct FakeOps {
    std::string fail, calls;
    long step(const char* name) { calls += std::string(name) + ";"; return fail == name ? 123 : 0; }
    long begin() { return step("begin"); }
    long update() { return step("update"); }
    long attach(void**, void*) { return step("attach"); }
    long detach(void**, void*) { return step("detach"); }
    long commit() { return step("commit"); }
    void abort() { calls += "abort;"; }
};
void require(bool value) { if (!value) throw std::runtime_error("transaction contract failed"); }
int main() {
    int targetValue = 0, hookValue = 0;
    void* target = &targetValue;
    for (bool install : {false, true}) {
        const std::string operation = install ? "attach" : "detach";
        for (const std::string phase : {std::string(""), std::string("begin"), std::string("update"), operation, std::string("commit")}) {
            FakeOps ops; ops.fail = phase;
            auto result = integration::ChangeHook(ops, install, &target, &hookValue);
            require(static_cast<bool>(result) == phase.empty());
            if (!phase.empty()) require(result.error == 123 && result.phase == phase);
            std::string expected = "begin;";
            if (phase != "begin") {
                expected += "update;";
                if (phase == "update") expected += "abort;";
                else {
                    expected += operation + ";";
                    expected += phase == operation ? "abort;" : "commit;";
                }
            }
            require(ops.calls == expected);
        }
    }
    FakeOps ops; void* nullTarget = nullptr;
    require(!integration::ChangeHook(ops, true, nullptr, &hookValue));
    require(!integration::ChangeHook(ops, true, &nullTarget, &hookValue));
    require(!integration::ChangeHook(ops, true, &target, nullptr));
    require(ops.calls.empty());
    std::cout << "PASS: attach/detach success, each failure phase, ownership and null validation\n";
}
