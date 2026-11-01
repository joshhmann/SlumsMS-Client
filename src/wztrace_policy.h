#pragma once
// ===================================================================================================================
// wztrace_policy.h -- the rules of the QA client's WZ lookup trace (ticket b251), header-only and Windows-free, so
// tests/wztrace_policy_test.cpp can check them without a client. wztrace.cpp is the Windows side: config.ini, the
// file, the namespace hook, the throw hook.
//
// Why: map 211080100 (Rose Garden) ends the session through an uncaught C++ throw out of packet processing during
// the field change and no crash dialog, and src/crash_diagnostics.cpp stops logging C++ events after 16 per process,
// so the throw that matters never got logged (rosegarden REPORT). The field data itself is complete. That leaves one
// question: which WZ lookup did the client make just before it threw? A path plus an HRESULT names it, so this file
// decides what gets written for a lookup, for a throw, and for the ring of lookups a throw is read against.
// ===================================================================================================================

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "qamodule_policy.h"   // QaPolicy::UtcTime, QaPolicy::LogStamp, QaPolicy::TruncateUtf8: one stamp, one cut

namespace WzTrace {

// ---- the switch -------------------------------------------------------------------------------------------------
// config.ini [debug] wztrace: 0 off (no hook at all), 1 failed lookups only, 2 also the ring and the throw hook.
constexpr int kMaxLevel = 2;
constexpr const char* kLogName = "wztrace.log";          // written next to the exe, like crash.log
constexpr const wchar_t* kLogNameW = L"wztrace.log";

inline int ClampLevel(int level) {
    if (level < 0) return 0;
    if (level > kMaxLevel) return kMaxLevel;
    return level;
}

// ---- the HRESULT filter -----------------------------------------------------------------------------------------
// The rule of src/crash_diagnostics.cpp:37-38 (the one that names the last C++ throw): a thrown object is only read
// as an HRESULT inside these ranges, so a string, a pointer or packet data that happens to sit first in the object is
// never printed as one. The 0x8003 facility is what makes this trace worth anything here -- 0x80030002 is a WZ item
// that was not found. 0x80010000 (RPC) and 0x80000000 (E_FAIL) are NOT in the list, exactly as in that rule.
inline bool IsHResult(std::uint32_t value) {
    return (value >= 0x20000000u && value <= 0x2200000Eu) || (value & 0xFFFF0000u) == 0x80040000u ||
        (value & 0xFFFF0000u) == 0x80070000u || (value & 0xFFFF0000u) == 0x80030000u;
}

// SUCCEEDED()/FAILED() on a value that did not come through an HRESULT type: the sign bit decides.
inline bool Succeeded(std::uint32_t hr) {
    return static_cast<std::int32_t>(hr) >= 0;
}

// ---- one lookup line --------------------------------------------------------------------------------------------
constexpr std::size_t kMaxPathBytes = 256;    // a WZ namespace path; the longest field path in 211080100 is ~60
constexpr std::size_t kMaxModuleName = 64;    // the module column of a throw line

// A path is one field of one line: a newline or a CR in a malformed namespace path would fake a second line, so every
// control byte becomes a space, and a long path is cut on a UTF-8 boundary (never mid-sequence: the log says UTF-8).
inline std::string LogPath(const std::string& path) {
    std::string out = path;
    for (char& c : out) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) c = ' ';
    }
    return QaPolicy::TruncateUtf8(out, kMaxPathBytes);
}

// "<stamp> thread=<tid> lookup hr=<hr> <path>" -- the hr tells a miss (80030002, the item was not found) from a
// failed namespace (80030001) from a COM fault, which is what narrows a throw down to the lookup that caused it.
inline std::string LookupLine(const QaPolicy::UtcTime& t, unsigned long threadId, std::uint32_t hr,
    const std::string& path) {
    char head[128];
    std::snprintf(head, sizeof head, "%s thread=%lu lookup hr=%08X ", QaPolicy::LogStamp(t).c_str(), threadId, hr);
    return std::string(head) + LogPath(path) + "\n";
}

// ---- one tile line ----------------------------------------------------------------------------------------------
// A Map/Tile lookup happens inside MakeTile, so its line marks where in a field's build the failing lookup sat. The
// three arguments are printed raw: their types are not provable from the stock code (research-119-120.md), and a wrong
// dereference in a QA build is worse than a raw dword -- small numbers are still readable as a layer and tile index.
inline std::string MakeTileLine(const QaPolicy::UtcTime& t, unsigned long threadId, std::uint32_t self,
    std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    char buf[192];
    std::snprintf(buf, sizeof buf, "%s thread=%lu makeTile this=%08X a0=%08X a1=%08X a2=%08X\n",
        QaPolicy::LogStamp(t).c_str(), threadId, self, a0, a1, a2);
    return std::string(buf);
}

// ---- one throw line ---------------------------------------------------------------------------------------------
// The return address of _CxxThrowException's caller, in crash_diagnostics.cpp's shape (address, module, offset), plus
// the HRESULT when the thrown object starts with one. `module` is only ever a file name, never a full path.
inline std::string ThrowLine(const QaPolicy::UtcTime& t, unsigned long threadId, std::uint32_t address,
    const std::string& module, std::uint32_t offset, bool hasHresult, std::uint32_t hresult) {
    char head[192];
    std::snprintf(head, sizeof head, "%s thread=%lu throw address=%08X module=%s offset=%08X ",
        QaPolicy::LogStamp(t).c_str(), threadId, address,
        QaPolicy::TruncateUtf8(module.empty() ? std::string("unknown") : module, kMaxModuleName).c_str(), offset);
    char tail[32];
    if (hasHresult) std::snprintf(tail, sizeof tail, "hr=%08X", hresult);
    else std::snprintf(tail, sizeof tail, "hr=none");
    return std::string(head) + tail + "\n";
}

// ---- the ring of the last lookups -------------------------------------------------------------------------------
// 128 entries covers one field load's namespace lookups and keeps the ring cheap: it is filled from the namespace
// hook on whatever thread looks a path up. Formatting a dumped entry does allocate, inside the lock, whose only
// other holder is a push of one short string -- nothing here re-enters the hooks, so the spin is bounded.
constexpr std::size_t kRingSize = 128;
constexpr unsigned kRingDumpIntervalMs = 2000;   // at most one ring per throw burst: a burst throws hundreds of times

// A dumped ring is read as: ring_begin count=128, ring[0] (oldest) .. ring[count-1] (newest), ring_end.
inline std::string RingBeginLine(const QaPolicy::UtcTime& t, std::size_t count) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s ring_begin count=%zu (the last WZ lookups, oldest first)\n",
        QaPolicy::LogStamp(t).c_str(), count);
    return std::string(buf);
}

inline std::string RingEntryLine(std::size_t index, std::uint32_t hr, const std::string& path) {
    char head[64];
    std::snprintf(head, sizeof head, "ring[%zu] hr=%08X ", index, hr);
    return std::string(head) + LogPath(path) + "\n";
}

inline std::string RingEndLine(const QaPolicy::UtcTime& t) {
    return QaPolicy::LogStamp(t) + " ring_end\n";
}

// Once per burst: the first throw always dumps, then the clock decides, so a load that throws in a loop cannot flood
// the log with rings.
inline bool ShouldDumpRing(bool dumpedBefore, std::uint64_t lastDumpMs, std::uint64_t nowMs) {
    if (!dumpedBefore) return true;
    return nowMs >= lastDumpMs && nowMs - lastDumpMs >= kRingDumpIntervalMs;
}

struct RingEntry {
    std::uint32_t hr = 0;
    std::string path;
};

// Newest wins: after kRingSize pushes the entry at slot 0 is the one kRingSize lookups old, and Snapshot()/AppendDump
// walk from there, so the last line of a dump is always the lookup the throw happened after.
class LookupRing {
public:
    void Push(std::uint32_t hr, const std::string& path) {
        Lock();
        m_entries[m_next].hr = hr;
        m_entries[m_next].path = path;
        m_next = (m_next + 1) % kRingSize;
        if (m_count < kRingSize) ++m_count;
        Unlock();
    }

    std::size_t Count() const {
        Lock();
        const std::size_t count = m_count;
        Unlock();
        return count;
    }

    void Clear() {
        Lock();
        for (RingEntry& entry : m_entries) {
            entry.hr = 0;
            entry.path.clear();
        }
        m_next = 0;
        m_count = 0;
        Unlock();
    }

    // Oldest first, for the dump and for the test.
    std::vector<RingEntry> Snapshot() const {
        std::vector<RingEntry> out;
        Lock();
        out.reserve(m_count);
        for (std::size_t i = 0; i < m_count; ++i) out.push_back(m_entries[Oldest(i)]);
        Unlock();
        return out;
    }

    // The same order written into a caller buffer, no allocation: this runs inside the throw hook, where an
    // allocation failure would turn a logged throw into a second throw. Returns the bytes written (NUL terminated,
    // always <= capacity - 1) and stops cleanly at the end of the buffer.
    std::size_t AppendDump(char* out, std::size_t capacity) const {
        if (!out || capacity == 0) return 0;
        std::size_t used = 0;
        out[0] = '\0';
        Lock();
        for (std::size_t i = 0; i < m_count; ++i) {
            const RingEntry& entry = m_entries[Oldest(i)];
            const std::string line = RingEntryLine(i, entry.hr, entry.path);
            if (used + line.size() + 1 > capacity) break;
            std::memcpy(out + used, line.data(), line.size());
            used += line.size();
            out[used] = '\0';
        }
        Unlock();
        return used;
    }

private:
    std::size_t Oldest(std::size_t i) const {
        return (m_next + kRingSize - m_count + i) % kRingSize;
    }
    void Lock() const {
        while (m_lock.test_and_set(std::memory_order_acquire)) {
        }
    }
    void Unlock() const { m_lock.clear(std::memory_order_release); }

    mutable std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
    RingEntry m_entries[kRingSize];
    std::size_t m_next = 0;    // the slot the next push overwrites: the oldest one once the ring is full
    std::size_t m_count = 0;
};

// ---- the size cap -----------------------------------------------------------------------------------------------
// A QA log has no business growing without end: at 2 MB the trace stops. The one final notice may push the file a
// little past the cap so the reader learns why the log ends, and after that nothing is written at all.
constexpr std::uint64_t kMaxLogBytes = 2u * 1024u * 1024u;

inline bool UnderCap(std::uint64_t currentBytes, std::size_t lineBytes) {
    return currentBytes <= kMaxLogBytes && lineBytes <= kMaxLogBytes - currentBytes;
}

inline std::string CapLine(const QaPolicy::UtcTime& t) {
    return QaPolicy::LogStamp(t) + " wztrace.log reached its 2 MB cap: the trace stops here\n";
}

// The first line of every session, so a log says which switch produced it and what the ring size was.
inline std::string SessionLine(const QaPolicy::UtcTime& t, int level) {
    char buf[128];
    std::snprintf(buf, sizeof buf, "%s wztrace level=%d ring=%zu max=%llu\n", QaPolicy::LogStamp(t).c_str(), level,
        kRingSize, static_cast<unsigned long long>(kMaxLogBytes));
    return std::string(buf);
}

}   // namespace WzTrace
