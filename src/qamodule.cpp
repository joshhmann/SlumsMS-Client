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
#include "qamodule_policy.h"
#include "buildstamp_proto.h"
#include "debug.h"

#include <d3d9.h>
#include <atomic>
#include <condition_variable>
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
    std::thread(EventsThread).detach();
    std::thread(CommandsThread).detach();
    std::thread(WriterThread).detach();
    DebugMessage("QA module: pipes slums-qa-%lu-events / -commands (QA build %s, never for players)",
        GetCurrentProcessId(), kSlumsRelease);
}

void QaModule_OnPacket(uint16_t opcode) {
    if (!g_started) return;
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
                Emit(g_armed[i].first);
                g_armed.erase(g_armed.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }
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
