#pragma once

namespace integration {
struct HookResult {
    long error;
    const char* phase;
    explicit operator bool() const { return error == 0; }
};

// Ops owns platform calls. Keep transaction ownership explicit: failed Begin
// belongs to another caller; Commit consumes its transaction even on error.
template <class Ops>
HookResult ChangeHook(Ops& ops, bool install, void** target, void* detour) {
    if (!target || !*target || !detour) return {87, "validate"};
    long error = ops.begin();
    if (error) return {error, "begin"};
    error = ops.update();
    if (error) { ops.abort(); return {error, "update"}; }
    error = install ? ops.attach(target, detour) : ops.detach(target, detour);
    if (error) { ops.abort(); return {error, install ? "attach" : "detach"}; }
    error = ops.commit();
    return {error, error ? "commit" : "complete"};
}
}
