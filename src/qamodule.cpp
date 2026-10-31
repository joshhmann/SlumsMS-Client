// ============================================================
// qamodule.cpp -- the QA client's runner channel (ticket 166, SLUMS_QA_MODULE builds only). Rules and the wire format
// are in qamodule_policy.h. Two one-way named pipes per client process; the runner connects to both.
//   events thread:   accepts the runner on ...-events, writes queued event lines; reconnects if the runner goes away.
//   commands thread: accepts the runner on ...-commands, reads lines, queues bursts for the render thread.
//   render thread (EndScene): fires settled ready signals, takes burst frames (GetRenderTargetData), hands them to
//   the writer thread, which saves qa-captures\<label>\NNNN.bmp and frames.tsv beside the game.
// Nothing here runs in a player build: the file is compiled only with SLUMS_QA_MODULE.
// ============================================================
#include "pch.h"
#include "qamodule.h"
#include "qamodule_patch.h"
#include "qamodule_policy.h"
#include "buildstamp_proto.h"
#include "debug.h"
#include "constants.h"   // wndman.h uses CONSTANTS_CENTER_STATUSBAR without including it
#include "wvs/wnd.h"
#include "wvs/wndman.h"

#include <d3d9.h>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace QaPolicy;

std::atomic<bool> g_started{false};
LARGE_INTEGER g_qpcFreq{}, g_qpcStart{};

uint64_t NowMs() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>((now.QuadPart - g_qpcStart.QuadPart) * 1000 / g_qpcFreq.QuadPart);
}

std::wstring GameDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (wchar_t* slash = wcsrchr(path, L'\\')) *slash = 0;
    return path;
}

// ---- events (client -> runner) ----------------------------------------------------------------------------------
std::mutex g_evMutex;
std::condition_variable g_evCv;
std::deque<std::string> g_evQueue;

void Emit(const char* name, const std::string& extra = std::string()) {
    const std::string line = EventLine(name, NowMs(), extra);
    {
        std::lock_guard<std::mutex> lock(g_evMutex);
        if (g_evQueue.size() < 1024) g_evQueue.push_back(line);
    }
    g_evCv.notify_one();
    DebugMessage("QA: %s", line.substr(0, line.size() - 1).c_str());
}

void EmitError(const std::string& msg) { Emit("error", ",\"msg\":\"" + JsonEscape(msg) + "\""); }

// ---- stock-byte guards (ticket 170) -----------------------------------------------------------------------------
// The QA module installs no patches; it rides the existing packet detour and reads the client's own window list.
// Both addresses it stands on are compared with qamodule_patch.h before anything uses them. This runs before the
// client's hooks go in (DllMain -> AttachSystemHooks -> QaModule_Start; AttachClientHooks comes later), so the
// bytes here are the stock ones. A mismatch switches that part off and says so, instead of sending wrong signals.
bool g_dispatchSiteOk = false;
bool g_fieldSiteOk = false;

void CheckHookSites() {
    g_dispatchSiteOk = QaPatch::Matches(reinterpret_cast<const unsigned char*>(QaPatch::kAddr_ProcessPacket),
                                        QaPatch::ProcessPacketBytes().data(), QaPatch::ProcessPacketBytes().size());
    if (!g_dispatchSiteOk) {
        EmitError("hook site 0x004965F1 (CClientSocket::ProcessPacket) is not the stock code this build expects: "
                  "ready signals and chat.received are off");
    }
    g_fieldSiteOk = QaPatch::Matches(reinterpret_cast<const unsigned char*>(QaPatch::kAddr_GetCurFieldID),
                                     QaPatch::GetCurFieldIDBytes().data(), QaPatch::GetCurFieldIDBytes().size());
    if (!g_fieldSiteOk) {
        EmitError("hook site 0x00A1238B (CWvsContext::GetCurFieldID) is not the stock code this build expects: "
                  "field.ready carries no map_id");
    }
}

// The map the client is in, read the way the rest of the client reads it (bounce.cpp, lamps.cpp, weather.cpp).
// Returns false when the site check failed, so field.ready then omits map_id rather than reporting a wrong map.
bool CurMapId(int& mapId) {
    if (!g_fieldSiteOk) return false;
    mapId = reinterpret_cast<int(__thiscall*)(void*)>(QaPatch::kAddr_GetCurFieldID)(nullptr);
    return true;
}

// ---- windows (ticket 170) ---------------------------------------------------------------------------------------
// Once a frame the client's own window list (CWndMan::ms_lpWindow) is compared with the list from the last frame;
// a window that appeared is window.open, one that went away is window.closed. Nothing is patched and nothing is
// clicked. Only what the runner can act on is reported: UIWindow windows (they have a background UOL), windows named
// by class, and dialogs a packet asked for. Game logic and rendering share one thread in this client, so the list is
// stable between frames and this needs no lock.
//
// A window is keyed by its address AND its CreateWnd serial (CWnd+0x14, zeroed on Destroy), so a new window that
// reuses a freed address is still reported as a new window.
constexpr size_t kMaxTrackedWindows = 128;
constexpr size_t kMaxPendingDialogs = 8;
constexpr uint64_t kDialogQueueMs = 3000;   // a queued dialog name nobody used is stale after this

struct PendingDialog {
    const char* name;
    uint64_t expiresMs;
};

struct TrackedWindow {
    uintptr_t ptr;
    uint32_t createId;
    std::string name;
};

PendingDialog g_pendingDialogs[kMaxPendingDialogs];
size_t g_pendingCount = 0;
bool g_cuiwndSeen = false;      // the client's class test found at least one UI window
unsigned g_windowScanFrames = 0;
TrackedWindow g_windows[kMaxTrackedWindows];
size_t g_windowCount = 0;

void DropExpiredDialogs(uint64_t now) {
    size_t keep = 0;
    for (size_t i = 0; i < g_pendingCount; ++i) {
        if (g_pendingDialogs[i].expiresMs >= now) g_pendingDialogs[keep++] = g_pendingDialogs[i];
    }
    g_pendingCount = keep;
}

// The dialog a packet asked for; the next window with no name of its own takes it (see qamodule_policy.h).
void QueueDialog(const char* name) {
    if (!name) return;
    DropExpiredDialogs(NowMs());
    if (g_pendingCount == kMaxPendingDialogs) {   // wait too long and the oldest request is the stale one
        memmove(&g_pendingDialogs[0], &g_pendingDialogs[1], sizeof(g_pendingDialogs[0]) * (g_pendingCount - 1));
        --g_pendingCount;
    }
    g_pendingDialogs[g_pendingCount].name = name;
    g_pendingDialogs[g_pendingCount].expiresMs = NowMs() + kDialogQueueMs;
    ++g_pendingCount;
}

const char* TakeDialog() {
    DropExpiredDialogs(NowMs());
    if (g_pendingCount == 0) return nullptr;
    const char* name = g_pendingDialogs[0].name;
    memmove(&g_pendingDialogs[0], &g_pendingDialogs[1], sizeof(g_pendingDialogs[0]) * (g_pendingCount - 1));
    --g_pendingCount;
    return name;
}

uint32_t CreateIdOf(const CWnd* w) {
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(w) + 0x14);   // invresize_patch.h kOff_CreateId
}

uint32_t VtableOf(const CWnd* w) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(*reinterpret_cast<void* const*>(w))); }

// The background UOL a UIWindow window was made from (CUIWnd::m_sBackgrndUOL at +0x5AC, ZXString = one pointer).
const wchar_t* UolOf(const CWnd* w) {
    return *reinterpret_cast<const wchar_t* const*>(reinterpret_cast<const char*>(w) + 0x5AC);
}

// UOLs are ASCII; anything else is not a name we can map, so it becomes '?' rather than widening the event.
std::string NarrowUol(const wchar_t* w, size_t max) {
    std::string s;
    if (!w) return s;
    for (size_t i = 0; i < max && w[i]; ++i) s.push_back(w[i] < 0x80 ? static_cast<char>(w[i]) : '?');
    return s;
}

void PollWindows() {
    bool seen[kMaxTrackedWindows] = {};
    // Windows that appeared this frame, named after the walk so a dialog name is only handed to one of them and
    // never to a window that was already up.
    struct Fresh {
        CWnd* w;
        uint32_t createId;
        std::string uol;
        const char* name;
    };
    constexpr size_t kMaxFresh = 16;
    Fresh fresh[kMaxFresh];
    size_t freshCount = 0;

    auto pos = CWndMan::ms_lpWindow.GetHeadPosition();
    while (pos) {
        CWnd* w = CWndMan::ms_lpWindow.GetNext(pos);
        if (!w) continue;
        const uint32_t createId = CreateIdOf(w);
        if (createId == 0) continue;                              // not created yet (Destroy zeroes the serial)
        if (!w->IsKindOf(&CUIWnd::ms_RTTI_CUIWnd)) continue;      // HUD layers and children are not runner screens
        g_cuiwndSeen = true;

        size_t slot = kMaxTrackedWindows;
        for (size_t i = 0; i < g_windowCount; ++i) {
            if (g_windows[i].ptr == reinterpret_cast<uintptr_t>(w) && g_windows[i].createId == createId) {
                slot = i;
                break;
            }
        }
        if (slot != kMaxTrackedWindows) {
            seen[slot] = true;                                    // already reported open
            continue;
        }
        if (freshCount == kMaxFresh) continue;

        Fresh& f = fresh[freshCount++];
        f.w = w;
        f.createId = createId;
        f.uol = NarrowUol(UolOf(w), 128);
        f.name = WindowNameFromUol(f.uol);
        if (!f.name) f.name = WindowNameForVtable(VtableOf(w));
    }

    // Dialogs are one class for a party invite, a proposal, an npc dialog and a notice, so the name comes from the
    // packet that asked for the dialog. A window with no UOL of its own is the surest match, so those are served
    // first; only then the rest.
    if (!g_cuiwndSeen && ++g_windowScanFrames == 600) {   // ten seconds of frames with no UI window at all
        EmitError("the window list shows no UI windows: window.open and window.closed are off (class test failed)");
    }

    for (int pass = 0; pass < 2; ++pass) {
        const bool wantEmptyUol = pass == 0;
        for (size_t i = 0; i < freshCount; ++i) {
            if (fresh[i].name || fresh[i].uol.empty() != wantEmptyUol) continue;
            fresh[i].name = TakeDialog();
        }
    }

    for (size_t i = 0; i < freshCount; ++i) {
        const Fresh& f = fresh[i];
        // A window with no name and no UOL is HUD furniture the runner does not act on (the minimap, the status
        // bar's children). It is still remembered below, so it cannot take a dialog name queued a frame later.
        const bool report = f.name != nullptr || !f.uol.empty();
        if (report) {
            std::string extra = ",\"window\":\"" + JsonEscape(f.name ? f.name : "unknown") + "\"";
            if (!f.name) {
                char vtable[16] = {};
                snprintf(vtable, sizeof(vtable), "0x%08X", VtableOf(f.w));
                extra += ",\"vtable\":\"";
                extra += vtable;
                extra += "\",\"uol\":\"" + JsonEscape(f.uol) + "\"";
            }
            Emit("window.open", extra);
        }
        if (g_windowCount < kMaxTrackedWindows) {
            g_windows[g_windowCount].ptr = reinterpret_cast<uintptr_t>(f.w);
            g_windows[g_windowCount].createId = f.createId;
            g_windows[g_windowCount].name = report ? (f.name ? f.name : "unknown") : std::string();
            seen[g_windowCount] = true;
            ++g_windowCount;
        }
    }

    for (size_t i = 0; i < g_windowCount;) {
        if (seen[i]) {
            ++i;
            continue;
        }
        if (!g_windows[i].name.empty()) Emit("window.closed", ",\"window\":\"" + JsonEscape(g_windows[i].name) + "\"");
        g_windows[i] = g_windows[g_windowCount - 1];
        seen[i] = seen[g_windowCount - 1];
        --g_windowCount;
    }
}

std::wstring PipeName(const wchar_t* suffix) {
    return L"\\\\.\\pipe\\slums-qa-" + std::to_wstring(GetCurrentProcessId()) + L"-" + suffix;
}

void EventsThread() {
    const std::wstring name = PipeName(L"events");
    for (;;) {
        HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 64 * 1024, 0, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            DebugMessage("QA: could not open the events pipe (error %lu)", GetLastError());
            return;
        }
        const BOOL connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected) {
            // Greet with what this client is, then stream the queue (events queued before the runner came stay).
            const std::string hello = EventLine("hello", NowMs(),
                ",\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"release\":\"" + JsonEscape(kSlumsRelease) +
                "\",\"build\":" + std::to_string(kSlumsBuildNumber));
            DWORD wrote = 0;
            bool ok = WriteFile(pipe, hello.data(), static_cast<DWORD>(hello.size()), &wrote, nullptr) != FALSE;
            while (ok) {
                std::string line;
                {
                    std::unique_lock<std::mutex> lock(g_evMutex);
                    g_evCv.wait(lock, [] { return !g_evQueue.empty(); });
                    line = std::move(g_evQueue.front());
                    g_evQueue.pop_front();
                }
                ok = WriteFile(pipe, line.data(), static_cast<DWORD>(line.size()), &wrote, nullptr) != FALSE;
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

// ---- bursts (render thread takes frames, writer thread saves them) ------------------------------------------------
struct Burst {
    std::string label;
    int fps = 0;
    int ms = 0;
};

std::mutex g_burstMutex;
std::deque<Burst> g_burstQueue;      // commands thread -> render thread

struct Frame {
    std::string label;
    int index = 0;
    uint64_t atMs = 0;               // since the burst began
    int w = 0, h = 0;
    std::vector<uint8_t> bgra;       // top-down rows, w*4 bytes each
    bool last = false;
};
std::mutex g_frameMutex;
std::condition_variable g_frameCv;
std::deque<Frame> g_frameQueue;      // render thread -> writer thread

void WriterThread() {
    const std::wstring root = GameDir() + L"\\qa-captures";
    CreateDirectoryW(root.c_str(), nullptr);
    for (;;) {
        Frame f;
        {
            std::unique_lock<std::mutex> lock(g_frameMutex);
            g_frameCv.wait(lock, [] { return !g_frameQueue.empty(); });
            f = std::move(g_frameQueue.front());
            g_frameQueue.pop_front();
        }
        const std::wstring dir = root + L"\\" + std::wstring(f.label.begin(), f.label.end());
        CreateDirectoryW(dir.c_str(), nullptr);
        if (!f.bgra.empty()) {
            wchar_t file[32];
            swprintf_s(file, L"\\%04d.bmp", f.index);
            FILE* out = nullptr;
            if (_wfopen_s(&out, (dir + file).c_str(), L"wb") == 0 && out) {
                uint8_t header[54];
                BmpHeader(header, f.w, f.h);
                fwrite(header, 1, sizeof header, out);
                for (int y = f.h - 1; y >= 0; --y) fwrite(&f.bgra[static_cast<size_t>(y) * f.w * 4], 1, static_cast<size_t>(f.w) * 4, out);
                fclose(out);
            }
            FILE* tsv = nullptr;
            if (_wfopen_s(&tsv, (dir + L"\\frames.tsv").c_str(), L"ab") == 0 && tsv) {
                fprintf(tsv, "%04d\t%llu\n", f.index, static_cast<unsigned long long>(f.atMs));
                fclose(tsv);
            }
        }
        if (f.last) {
            Emit("burst.done", ",\"label\":\"" + f.label + "\",\"frames\":" + std::to_string(f.bgra.empty() ? f.index : f.index + 1));
        }
    }
}

void CommandsThread() {
    const std::wstring name = PipeName(L"commands");
    for (;;) {
        HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, 4096, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            DebugMessage("QA: could not open the commands pipe (error %lu)", GetLastError());
            return;
        }
        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::string pending;
            char buf[512];
            DWORD got = 0;
            while (ReadFile(pipe, buf, sizeof buf, &got, nullptr) && got > 0) {
                pending.append(buf, got);
                size_t nl;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, nl);
                    pending.erase(0, nl + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;
                    const Command c = ParseCommand(line);
                    if (c.kind == Command::Ping) {
                        Emit("pong");
                    } else if (c.kind == Command::Burst) {
                        std::lock_guard<std::mutex> lock(g_burstMutex);
                        g_burstQueue.push_back({c.label, c.fps, c.ms});
                    } else {
                        EmitError(c.error + ": " + line.substr(0, 80));
                    }
                }
                if (pending.size() > 4096) pending.clear();   // a runner that never sends a newline
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

// ---- render-thread state -----------------------------------------------------------------------------------------
// Ready signals armed by handled packets; each fires after kSettleFrames more frames.
std::mutex g_signalMutex;
std::vector<std::pair<const char*, int>> g_armed;   // signal, frames still to draw

struct ActiveBurst {
    bool on = false;
    Burst b;
    uint64_t startMs = 0, nextMs = 0;
    int taken = 0;
} g_active;

IDirect3DSurface9* g_sysmem = nullptr;
UINT g_sysW = 0, g_sysH = 0;
D3DFORMAT g_sysFmt = D3DFMT_UNKNOWN;

void ReleaseSysmem() {
    if (g_sysmem) g_sysmem->Release();
    g_sysmem = nullptr;
    g_sysW = g_sysH = 0;
    g_sysFmt = D3DFMT_UNKNOWN;
}

// Copies the current back buffer; false (with one error event) when the format is not 32-bit.
bool Grab(IDirect3DDevice9* dev, Frame& f) {
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &rt)) || !rt) return false;
    D3DSURFACE_DESC desc = {};
    rt->GetDesc(&desc);
    if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) {
        rt->Release();
        static bool told = false;
        if (!told) { told = true; EmitError("back buffer is not 32-bit (format " + std::to_string(desc.Format) + ")"); }
        return false;
    }
    if (!g_sysmem || g_sysW != desc.Width || g_sysH != desc.Height || g_sysFmt != desc.Format) {
        ReleaseSysmem();
        if (FAILED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &g_sysmem, nullptr))) {
            rt->Release();
            return false;
        }
        g_sysW = desc.Width;
        g_sysH = desc.Height;
        g_sysFmt = desc.Format;
    }
    const bool ok = SUCCEEDED(dev->GetRenderTargetData(rt, g_sysmem));
    rt->Release();
    if (!ok) return false;
    D3DLOCKED_RECT lr = {};
    if (FAILED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;
    f.w = static_cast<int>(desc.Width);
    f.h = static_cast<int>(desc.Height);
    f.bgra.resize(static_cast<size_t>(f.w) * f.h * 4);
    for (int y = 0; y < f.h; ++y) {
        uint8_t* row = &f.bgra[static_cast<size_t>(y) * f.w * 4];
        memcpy(row, static_cast<const uint8_t*>(lr.pBits) + static_cast<size_t>(y) * lr.Pitch, static_cast<size_t>(f.w) * 4);
        for (int x = 0; x < f.w; ++x) row[x * 4 + 3] = 0xFF;   // X8R8G8B8 leaves alpha undefined
    }
    g_sysmem->UnlockRect();
    return true;
}

void PushFrame(Frame&& f) {
    {
        std::lock_guard<std::mutex> lock(g_frameMutex);
        g_frameQueue.push_back(std::move(f));
    }
    g_frameCv.notify_one();
}

}  // namespace

void QaModule_Start() {
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true)) return;
    QueryPerformanceFrequency(&g_qpcFreq);
    QueryPerformanceCounter(&g_qpcStart);
    CheckHookSites();   // before the client's hooks go in: these must be the stock bytes
    std::thread(EventsThread).detach();
    std::thread(CommandsThread).detach();
    std::thread(WriterThread).detach();
    DebugMessage("QA module: pipes slums-qa-%lu-events / -commands (QA build %s, never for players)",
        GetCurrentProcessId(), kSlumsRelease);
}

void QaModule_OnPacket(uint16_t opcode, const void* packet, size_t size) {
    if (!g_started) return;
    const auto* bytes = static_cast<const unsigned char*>(packet);
    // A dialog the runner must expect: queued before the window shows up (qamodule_policy.h DialogForPacket).
    QueueDialog(DialogForPacket(opcode, bytes, size));
    if (!g_dispatchSiteOk) return;
    // A line the client puts in its chat log: map chat, buddy/party/guild, whisper, spouse, notice, yellow tip.
    const ChatLine chat = ParseChatPacket(opcode, bytes, size);
    if (chat.ok) {
        Emit("chat.received",
            ",\"kind\":" + std::to_string(chat.kind) + ",\"text\":\"" + JsonEscape(TruncateUtf8(chat.text, kMaxChatText)) + "\"");
    }
    const char* signal = SignalForOpcode(opcode);
    if (!signal) return;
    std::lock_guard<std::mutex> lock(g_signalMutex);
    for (auto& armed : g_armed) {
        if (armed.first == signal) { armed.second = kSettleFrames; return; }   // re-armed: wait again from now
    }
    g_armed.emplace_back(signal, kSettleFrames);
}

void QaModule_OnFrame(IDirect3DDevice9* dev) {
    if (!g_started) return;
    {
        std::lock_guard<std::mutex> lock(g_signalMutex);
        for (size_t i = 0; i < g_armed.size();) {
            if (--g_armed[i].second <= 0) {
                const char* name = g_armed[i].first;
                int mapId = 0;
                std::string extra;
                if (std::strcmp(name, "field.ready") == 0 && CurMapId(mapId)) {
                    extra = ",\"map_id\":" + std::to_string(mapId);
                }
                Emit(name, extra);
                g_armed.erase(g_armed.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }
    PollWindows();
    if (!g_active.on) {
        std::lock_guard<std::mutex> lock(g_burstMutex);
        if (g_burstQueue.empty()) return;
        g_active.b = g_burstQueue.front();
        g_burstQueue.pop_front();
        g_active.on = true;
        g_active.startMs = g_active.nextMs = NowMs();
        g_active.taken = 0;
        Emit("burst.start", ",\"label\":\"" + g_active.b.label + "\",\"fps\":" + std::to_string(g_active.b.fps) +
            ",\"ms\":" + std::to_string(g_active.b.ms));
    }
    const uint64_t now = NowMs();
    const uint64_t endMs = g_active.startMs + static_cast<uint64_t>(g_active.b.ms);
    if (now >= g_active.nextMs && now < endMs) {
        Frame f;
        f.label = g_active.b.label;
        f.index = g_active.taken;
        f.atMs = now - g_active.startMs;
        if (Grab(dev, f)) {
            ++g_active.taken;
            PushFrame(std::move(f));
        }
        g_active.nextMs += 1000u / static_cast<unsigned>(g_active.b.fps);
        if (g_active.nextMs < now) g_active.nextMs = now;   // a slow frame skips shots instead of bunching them
    }
    if (now >= endMs) {
        Frame done;                    // no pixels: tells the writer the burst is over, with the count in index
        done.label = g_active.b.label;
        done.index = g_active.taken;
        done.last = true;
        PushFrame(std::move(done));
        g_active.on = false;
    }
}

void QaModule_OnDeviceReset() { ReleaseSysmem(); }
