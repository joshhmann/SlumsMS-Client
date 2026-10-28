# Ticket 121: plain-language crash report (research note)

Read-only research, 2026-10-28. Sources: client `/root/work/client/src-161-qa` (ca7c572), launcher
`/root/work/launcher-160-join` (e69327e), site/report service `/root/work/SlumsMS-site-161/tools/site` (afba0ba00).
"UNVERIFIED" marks things not checked against the exe, a real crash or Windows.

## Summary

- Today `crash.log` comes from a **vectored** (first-chance) handler. It records every AV/illegal/div0/stack-overflow and
  every C++ throw (E06D7363), up to 16 of each per process, all tagged `first_chance=1`. Most of these are handled
  by the game, so they are noise, not crashes.
- A real crash is the exception that reaches the **top-level (unhandled) filter**. We already hook
  `SetUnhandledExceptionFilter` (system.cpp) to time `AttachClientHooks`. Extend that hook: install our own
  last-chance filter, keep the game's filter as the next one in the chain, write `crash_reports/LATEST_CRASH.txt`, then
  pass control to the game's filter.
- Backstop: `SlumsMS.exe` (src/launcher.cpp) already waits for MapleStory.exe and reads its exit code. If the exit code
  is an NTSTATUS failure and no fresh report exists, it can write a minimal one. That covers fast-fail /GS (0xC0000409),
  some stack overflows, and CRT abort, none of which reach an in-process filter. Catch: SlumsMS.exe builds are not
  reproducible, so shipping this means shipping a new exe (see memory: "ship only slums.dll"). Keep it as phase 2.
- Classification is a pure header (`crash_classify.h`): exception code + faulting module basename + AV address +
  free address space + last game error code give a headline in plain words and one line of advice. It is unit-testable.
- Missing data: today add-on code catches a failed `GetObject` with `catch (...)` and silently falls back
  (hud_native.cpp:93-99, monsterbook.cpp:170-180, ~160 catch sites). A missing Custom.wz is not checked before
  `fs->item[L"Custom.wz"]` (resman.cpp:113). Add a required-asset check after `InitializeResMan` that names the file,
  plus a ring of the last failed lookups for the crash report.
- Launcher: the game starts through ShellExecuteEx with no process handle, so the launcher looks for a new
  `LATEST_CRASH.txt` at start-up and when its window gets focus again. It shows the report and sends it only when
  the player clicks Send. It sends to the existing site `/api/report` service with a new type `crash`.

## Existing code (file:line)

Client (`/root/work/client/src-161-qa`):
- `src/crash_diagnostics.cpp:24-88` is the `OnException` VEH. It filters codes at :26-28, keeps a 16-per-kind cap at
  :31, appends to `crash.log` with relative path and `CreateFileA` at :35, uses the hard-coded `first_chance=1` line
  at :39, does the EBP walk at :57-66 and scans the stack for code addresses at :69-83. Its module lookup
  (`GetModuleHandleExA` + `GetModuleFileNameA`, :14-23) logs the full path, which may contain the Windows user name.
  It is installed with `AddVectoredExceptionHandler(1, ...)` at :90-94. The C++ HRESULT read at :48-56 dereferences
  the thrown object's first DWORD. That is right for ZException (hr first), but for `_com_error` the first DWORD is a
  vptr (UNVERIFIED layout), so COM errors log nothing.
- `src/system.cpp:19-28` is the `SetUnhandledExceptionFilter_hook`. It only uses the call from ZExceptionHandler's
  constructor (return address 0x00796FDD) to attach client hooks, then passes the game's filter through. It is
  attached at :231. `InstallCrashDiagnostics()` runs at :227 (MAPLEROOT_INTEGRATION only). MessageBoxA/W hooks at
  :213-222 log `Dialog [title]: text` to integration.log.
- `src/debug.cpp:8-23`: `DebugMessage` opens `integration.log` with fopen "a" for every line (relative path, CRT,
  heap). Do not call it from the crash path.
- `src/mapleroot.cpp:17-28` holds `Trace`/`Fail` (Fail shows ErrorMessage then ExitProcess). `:44-50` logs SESSION_START
  with a PE timestamp. `src/mapleroot.h` declares `InstallCrashDiagnostics`.
- `src/wvs/exception.h:5-28` lists the game's own error codes (`EC_NOT_ENOUGH_MEMORY 0x22000003`,
  `EC_NO_DATA_PACKAGE 0x22000004`, `EC_INVALID_GAME_DATA 0x22000006`, disconnect codes 0x2100000x...). These are
  ZException HRESULTs.
- `src/resman.cpp:81-160` is `InitializeResMan_hook`: the Custom namespace, loading `Custom.wz` at :101-115 (no check
  that it exists), the override walk at :117-147, and `SESSION_OVERLAY_READY` at :156-159. `:27-42` is the
  `raw__OnGetLocalObject_hook` in NameSpace.dll, which sees every failed lookup (`FAILED(hr)`). Use this to record
  "last missing paths".
- `mapleroot/ijl15/MapleHook.cpp:150-198` mounts the game folder and `./Data` (loose .img) into the root namespace.
- Map id: `src/bounce.cpp:34,51-57` calls `CWvsContext__GetCurFieldID` (0x00A1238B) under `__try`. CUserLocal pointer
  is at :59-63. `src/resolution.cpp:117-122` has the `set_stage_hook` (every stage change). Cache the field id there.
- Build stamp: `src/CMakeLists.txt:29-32` (`SLUMS_BUILD_NUMBER`, `SLUMS_RELEASE`, `slums-build.txt`) and
  `src/buildstamp_proto.h:11-20`.
- CMake options: `CMakeLists.txt:3-14`. The integration block is at `src/CMakeLists.txt:92-97` (crash_diagnostics.cpp
  built at :93). The SlumsMS.exe stub target is at :124-143.
- Config reads use `GetPrivateProfileIntA("debug", "attackdiag", 0, ".\\" CONSTANTS_CONFIG_NAME)` at attackdiag.cpp:212.
  The `[debug]` section is not in `config.example.ini` (its sections: general, discord, clienthax, weather,
  graphics, config).
- `src/launcher.cpp:7-36` is the SlumsMS.exe stub: `DetourCreateProcessWithDllExA`, `WaitForSingleObject`,
  `GetExitCodeProcess`, `SESSION_PROCESS_EXIT pid code` at :31.
- Tests: `tests/CMakeLists.txt` uses plain `add_executable` + `add_test`. Style is `tests/buildshow_test.cpp` (Check()
  that throws, main returns 0).

Launcher (`/root/work/launcher-160-join/tauri`):
- `core/src/client_build.rs:1-60` reads `slums-build.txt` from the game folder and "never fails". It is the model for a
  `crash_report.rs` reader. `read_client_build` is used in `app/src/launcher.rs:521`.
- `core/src/play.rs:74-110`: `launch` = ShellExecuteExW with `SEE_MASK_NOASYNC`, no `NOCLOSEPROCESS`, so there is no
  process handle and the launcher does not see the exit.
- `core/src/site.rs:56-215`: a shared reqwest client, `endpoint()`, cookie session (`/api/login/me`,
  `/api/market/me`). `app/src/launcher/account.rs` holds the signed-in token. Commands are registered at
  `app/src/main.rs:64`. Settings struct is at `core/src/settings.rs:34`. UI dialogs (`<dialog>.showModal`) are in
  `ui/renderer.mjs` (~:195-237, :321-332).
- `AGENTS.md`: tests are `npm test` and `cargo test -p slums-core`. The app crate builds on Windows only.

Receiving end (option): `/root/work/SlumsMS-site-161/tools/site/report_service.py`. This is the existing "Report a
bug" receiver: POST `/api/report`, JSON lines in `/var/lib/slums-feedback/reports.jsonl`, IP only as a salted hash,
`MAX_BODY = 8 KB`, `details` up to 2000 chars, types `{bug, feature}`, rate limits 5/h and 20/day per IP. Adding type
`crash` (larger body, e.g. 24 KB, and a `report` field) is the smallest server change. The game server
(`/root/work/SlumsMS-1602`) is not needed.

## Design

### 1. Real crash vs first-chance noise
- Keep the VEH for diagnostics, but stop it filling crash.log with handled throws. New config
  `[crash] firstchance=0|1|2`: 0 = off, 1 (default) = only fatal-candidate codes, no E06D7363, 2 = today's behaviour.
  Rename the line tag to `first_chance=1 (may be handled)` and write module basenames only.
- The VEH also keeps a tiny static "recent context" ring with no I/O: the last 4 C++ throw HRESULTs (ZException hr) and
  the last 4 fatal-candidate codes and addresses. The crash report quotes them as "events just before".
- **Last chance:** in `SetUnhandledExceptionFilter_hook`, when the caller passes the game's filter (first call from
  0x00796FDD), store it as `g_nextFilter` and install `SlumsTopLevelFilter` instead. Later calls (CRT, other DLLs,
  `SetUnhandledExceptionFilter(NULL)` from CRT abort or /GS paths) replace `g_nextFilter` and leave ours in place.
  `SlumsTopLevelFilter` writes the report, then returns `g_nextFilter ? g_nextFilter(ep) : EXCEPTION_CONTINUE_SEARCH`.
  What the stock ZExceptionHandler filter does (dialog? dump? silent exit?) is UNVERIFIED. Read it in the exe copy
  before the next step.
- Not a crash, so no report: `EXCEPTION_BREAKPOINT` under a debugger (the UEF is never called with a debugger
  attached), and normal ExitProcess after a game error dialog. For the last case, log a short
  `LATEST_EXIT.txt` only when the game's terminate dialog fires (MessageBox hook + last EC_* code). It is optional.
- Unhandled C++ throws: the VC CRT `terminate()` → `abort()` may exit with code 3 without calling the UEF
  (UNVERIFIED for this exe's CRT). The SlumsMS.exe backstop catches that.

### 2. Writing safely inside a crashing process
- Prepare at start-up (InstallCrashReport, called next to InstallCrashDiagnostics):
  - The absolute folder `<game>\crash_reports\` from `GetModuleFileNameW(nullptr)` → static `wchar_t[MAX_PATH]`.
    Run `CreateDirectoryW`. Do not depend on the current directory (the crash.log path is relative today).
  - Static buffers: 32 KB for the report text and 16 KB for the integration.log tail. No heap use in the crash path.
  - A **writer thread** created up front (`CreateThread`, 256 KB stack) that waits on an auto-reset event. The
    crashing thread copies `EXCEPTION_RECORD` + key `CONTEXT` registers + thread id into a static struct, calls
    `SetEvent`, then `WaitForSingleObject(done, 5000)`. This works even on stack overflow (the crashing thread has
    almost no stack) and avoids deep calls on a broken stack. Also call `SetThreadStackGuarantee(64 KB)` on the main
    thread. Run `InterlockedCompareExchange` re-entry guard. A second crash inside the writer → skip and chain.
- In the writer, use only kernel32 calls: `CreateFileW(...tmp, CREATE_ALWAYS)`, `WriteFile`, `FlushFileBuffers`,
  `MoveFileExW(tmp → LATEST_CRASH.txt, MOVEFILE_REPLACE_EXISTING)`. Then `CopyFileW` to
  `crash-YYYYMMDD-HHMMSS.txt`. Prune to the newest 10 at the **next start-up**, not in the handler. Format with
  `StringCchPrintfA` into the static buffer. No `fopen`/`DebugMessage` (CRT locks, heap).
- Module of the faulting address without the loader lock: `VirtualQuery(addr)` → `AllocationBase` → read the PE
  header and export directory `Name` under `__try`. That gives e.g. "nvd3dum.dll". The EXE has no exports, so match
  it to the main module base. Fall back to `GetModuleFileNameW` (takes the loader lock; acceptable behind the 5 s
  timeout). Keep basenames only.
- Map id: a `volatile LONG g_lastFieldId` set in `set_stage_hook` after the stage changes (with the stage kind: login,
  field, cash shop from RTTI), refreshed by the existing `CurrentFieldId()` under `__try`. Do not call game functions
  in the handler.
- integration.log tail: `CreateFileW(FILE_SHARE_READ|FILE_SHARE_WRITE)`. Seek to the end minus 16 KB, read, keep the
  last 50 `\n`-separated lines. Scrub each line (below) while copying.
- Address space: `GlobalMemoryStatusEx` → `ullAvailVirtual`. Under 64 MB means "ran out of memory" even when the
  code is an AV (common for 32-bit Maple). Is the exe LARGEADDRESSAWARE? UNVERIFIED. Record `ullTotalVirtual`.

### 3. Report format (`crash_reports/LATEST_CRASH.txt`)
```
SlumsMS crash report v1
What happened: The graphics driver crashed (nvd3dum.dll).
What to try: Update your graphics driver; if it keeps happening, turn off overlays (Discord, Steam, RTSS, OBS).
When (UTC): 2026-10-28T19:03:11Z      Uptime: 00:42:10
Client: 1.6.1 build 10601 (slums.dll PE 6720ABCD)
Where: field 100000000 (stage: field)
Code: C0000005 access violation, read of 00000080
Module: nvd3dum.dll +0012A3F0   (thread 1234, main=yes)
Memory: 1.2 GB free of 2.0 GB address space
Events just before: C++ throw hr=22000004 x1; AV in slums.dll+0004A110 (handled) x2
Missing game files this session: UI/CashShop.img/Base/backgrnd (cashshop)
Stack (module+offset): slums.dll+0004A110, MapleStory.exe+0035F2A1, ...
--- last 50 lines of integration.log ---
...
```
The key: value header lines let the launcher parse "What happened" and "When" without guessing.

### 4. Named error for missing Custom/add-on WZ
- `resman.cpp` before :113: check `GetFileAttributesW(<game>\Custom.wz)`. If missing, write
  `MISSING_ASSET Custom.wz` and show one plain dialog ("SlumsMS: the game file Custom.wz is missing. Open the
  launcher and click Repair."). Then continue on stock data, or stop, per `[crash] missingasset=warn|stop` (default
  warn). Today a missing Custom.wz probably throws `_com_error` from the `item` getter (UNVERIFIED).
- `required_assets.h`: a table `{feature, path}` per compiled-in addon (cash shop UI img, monster book
  `String/MonsterBookEx.img`, weather/lamp images, coloring prism UI...). After `SESSION_OVERLAY_READY`, try
  `get_rm()->GetObjectA` once for each path in try/catch. A missing entry logs
  `MISSING_ASSET feature=<f> path=<p>`, adds to a static list (quoted in the crash report and a one-time notice), and
  the feature turns itself off instead of falling back silently. The exact path list per feature needs a pass over
  each addon (UNVERIFIED which ones are truly required versus optional).
- Context ring: in `raw__OnGetLocalObject_hook` on `FAILED(hr)`, copy `sPath` (truncated to 96 wchar) into a static ring
  of 8. Failed lookups may be frequent and normal (optional nodes), so the report labels them "recent failed
  lookups", not "cause". Measure the frequency on TEST first (UNVERIFIED).

### 5. Launcher flow (consent first)
- `core/src/crash_report.rs` (pure, testable on Linux):
  - `find_latest(install_dir) -> Option<CrashReport { path, modified, what, when, build, text }>`. It caps file size at
    64 KB and never fails, like client_build.rs.
  - `parse_header(text)` reads the key: value lines.
  - `redact(text, home, user)` runs a second scrub in Rust: user profile paths, `%USERNAME%`, anything like `\Users\<x>\`,
    IPv4 addresses, `Dialog [..]:` bodies.
  - `is_new(modified, settings.crash_seen)`.
- `core/src/site.rs`: `send_crash(base, token: Option<&str>, report_text, note) -> Result<u64>` → POST `/api/report`
  `{type:"crash", title:<what happened>, report:<text>, details:<player note>, character:<signed-in character or "">}`.
- Settings: `crash_seen_ms: u64` (newest report already shown or dismissed). No "always send" option in v1.
- Commands (app/src/launcher.rs + main.rs:64): `crash_report_latest` (returns the parsed report and the redacted text
  that will be sent), `crash_report_dismiss`, `crash_report_send { sha256, note }` (the backend re-reads and redacts
  the file, then sends only if the hash matches what the UI showed, so what is sent is what was seen),
  `open_crash_folder`.
- Check at start-up, after Play when the window gets focus again (throttle 30 s), and from Settings > "Last crash
  report".
- UI (index.html + renderer.mjs + style.css): a card that reads "The game closed unexpectedly last time: <what
  happened>." with buttons [See report] [Not now]. The dialog shows the full redacted text read-only, a "What were
  you doing? (optional)" box, the sentence "This sends the text above to the SlumsMS staff. Nothing else is sent.",
  and buttons [Send to staff] [Close] [Open folder]. Send is enabled only while the dialog is open. On success:
  "Sent, thank you (report #id)."

## Plain-words mapping table

Order matters. The first matching rule wins. Module names are basenames, case-insensitive.

| Facts | Headline ("What happened") | Advice |
|---|---|---|
| any code, `ullAvailVirtual < 64 MB`; or C0000017; or C++ hr 8007000E / E_OUTOFMEMORY; or EC 22000003 | The game ran out of memory. | Close other programs, lower the resolution; tell staff if it keeps happening (memory leak). |
| C00000FD | The game ran out of stack (endless loop in the game or an add-on). | Please send the report. |
| C0000006 (in-page error) | The game could not read one of its files from disk. | Check the disk or antivirus; run Repair. |
| EC 22000004 / 22000005 / 22000006 (ZException) or `MISSING_ASSET` just before | A game data file is missing or damaged: <name if known>. | Run Repair in the launcher. |
| C0000005 in d3d9.dll, d3d8.dll, dxgi.dll, nvd3dum*.dll, nvwgf2um*.dll, atiumdag.dll, aticfx32.dll, amdxc32.dll, igdumd32.dll, igdumdim32.dll, igd9*.dll, Gr2D_DX9.dll, Gr2D_DX8.dll, d3d9 replacement (dxvk) | The graphics driver crashed (<module>). | Update the graphics driver; turn off overlays; try windowed mode. |
| any fault in a known overlay/hook module: RTSSHooks.dll, graphics-hook32.dll (OBS), DiscordHook.dll, GameOverlayRenderer.dll (Steam), Overwolf*, parsec*, nvspcap.dll | Another program running inside the game crashed it (<module>, e.g. an overlay). | Turn that overlay off for MapleStory. |
| fault in Sound_DX8.dll, mss32.dll, dsound.dll | The sound system crashed. | Check the audio device; try turning sound off. |
| fault in PCOM.dll, NameSpace.dll, ResMan.dll, Shape2D.dll | The game crashed while reading its data files. | Run Repair; send the report. |
| fault in slums.dll | A SlumsMS add-on crashed (<feature if mapped>). | Please send the report; this is our bug. |
| C0000374 (heap corruption) or fault in ntdll.dll | The game's memory got corrupted. | Please send the report (often a bug, sometimes an overlay). |
| C0000409 (seen only by the SlumsMS.exe backstop) | The game stopped itself after a safety check failed. | Please send the report. |
| C000001D / C0000096 | The game tried to run invalid code. | Run Repair; send the report. |
| C0000094 / C000008E / C0000090 | The game hit a maths error (divide by zero). | Please send the report. |
| E06D7363 unhandled, hr in 21000000-21000006 | The connection to the server was lost. | Check the internet connection; try again. |
| E06D7363 unhandled, other | The game hit an internal error (code <hr>). | Please send the report. |
| C0000005 in MapleStory.exe, address < 0x10000 | The game tried to use something that was not loaded (null object). | Please send the report. |
| C0000005 elsewhere | The game crashed (memory access error in <module>). | Please send the report. |
| exit code 3 with no report (backstop) | The game stopped after an internal error. | Please send integration.log. |

Driver and overlay module names are from general Windows knowledge (UNVERIFIED for our players' machines). The v83
DLL names (PCOM, NameSpace, Gr2D_DX9, Sound_DX8, mss32, Shape2D, ResMan) are the usual v83 folder set; confirm them
against the game folder listing.

## Risks

- **Writing files in a crash handler:** heap or loader lock may be held by the crashing thread, the stack may be
  exhausted, and the disk may be full. Mitigations: pre-created writer thread, static buffers, kernel32 only, 5 s
  timeout, tmp+rename so a half-written file never replaces a good one, re-entry guard, and always chain to the
  game's filter.
- **Chaining the UEF:** if the stock filter calls ExitProcess or shows a dialog, our report must be written first.
  Our filter must not swallow the exception (no `EXCEPTION_EXECUTE_HANDLER` unless chaining is impossible). Third-party
  DLLs (overlays) also call `SetUnhandledExceptionFilter`. Our hook keeps us on top, but a hook placed before ours
  (injected earlier) can still win.
- **Fast-fail / /GS / some stack overflows** never reach in-process handlers. Only the SlumsMS.exe backstop sees them.
  That needs a new SlumsMS.exe, which conflicts with "ship only slums.dll" (non-reproducible exe), so get the owner's
  decision.
- **Privacy:** the current crash.log writes full module paths (`C:\Users\<name>\...`). The new report uses basenames
  only. integration.log lines can hold dialog text (names in party/trade dialogs), character ids (`w0_c123` from
  invresize), D3D9 adapter names (hardware only, fine) and paths. Scrub in the client (paths, Dialog bodies) and again
  in the launcher (Rust, tested). No account name, password, IP, token or cookie may appear. The consent dialog says a
  character id may be included. Nothing uploads without Send, and the shown text is hash-checked against the sent text.
- **Abuse of the receiver:** a public `/api/report` with a bigger body means spam risk. Keep per-IP limits, cap
  crash size at 24 KB, and optionally require the session cookie for type `crash`.
- **Noise reduction hides evidence:** changing crash.log defaults could hide data QA relies on. Keep `firstchance=2`
  available and keep QA configs on 2.
- False classification: a headline is a best guess. Always print the raw code and module under it.

## Scaffold plan

Client (branch from the current client line; all behind `MAPLEROOT_CRASH_REPORT`, which requires MAPLEROOT_INTEGRATION):
- `CMakeLists.txt`: `option(MAPLEROOT_CRASH_REPORT "Write crash_reports/LATEST_CRASH.txt with a plain-language cause on a real crash" OFF)`
  with a FATAL_ERROR if INTEGRATION is off. `src/CMakeLists.txt`: `target_sources(injector PRIVATE crash_report.cpp
  required_assets.cpp)` + `target_compile_definitions(injector PRIVATE MAPLEROOT_CRASH_REPORT)`. Turn it on in the
  QA/RC presets (CMakePresets.json).
- `src/crash_classify.h` (header-only, no windows.h): `struct CrashFacts { uint32_t code; uint32_t avKind;
  uint32_t avAddr; char module[64]; bool moduleIsMain; uint32_t cxxHr; uint64_t availVirtual; bool missingAsset;
  char missingName[96]; }`. `enum class Cause {...}`. `Cause Classify(const CrashFacts&)`.
  `const char* Headline(Cause)`, `const char* Advice(Cause)`, `ModuleClass ClassOfModule(const char*)`.
  `void FormatHeadline(char*, size_t, const CrashFacts&)`.
- `src/crash_scrub.h` (header-only): `size_t ScrubLine(const char* in, char* out, size_t cap)` handles user profile
  paths, Dialog bodies and IPv4. Shared rules are mirrored in Rust.
- `src/crash_report.cpp/.h`: `InstallCrashReport()` (paths, buffers, writer thread, stack guarantee, prune old
  reports). `LONG WINAPI SlumsTopLevelFilter(EXCEPTION_POINTERS*)`. `void CrashReport_SetNextFilter(LPTOP_LEVEL_EXCEPTION_FILTER)`.
  `void CrashReport_NoteField(int fieldId, int stageKind)`. `void CrashReport_NoteCxx(uint32_t hr)`.
  `void CrashReport_NoteMissing(const wchar_t* path)`. Plus the writer thread proc, `ModuleOfAddress` via PE exports,
  and `TailIntegrationLog`.
- `src/system.cpp`: extend `SetUnhandledExceptionFilter_hook` to install ours and remember the next filter.
- `src/crash_diagnostics.cpp`: honour `[crash] firstchance`, write basenames, and feed `CrashReport_NoteCxx` and the
  fatal-event ring.
- `src/resolution.cpp` `set_stage_hook`: `CrashReport_NoteField(CurrentFieldId(), kind)`.
- `src/resman.cpp`: Custom.wz existence check + named dialog. Record failed lookups in `raw__OnGetLocalObject_hook`.
  Call `CheckRequiredAssets()` after SESSION_OVERLAY_READY.
- `src/required_assets.h/.cpp`: per-feature table, `CheckRequiredAssets()`, `bool AssetMissingFor(const char* feature)`.
- Test trigger (QA only): `[debug] crashtest=av|null|oom|stack|cxx|driver` with Ctrl+Alt+Shift+F12 at the login screen,
  compiled only with a separate `MAPLEROOT_CRASH_TEST` option so it never ships Live.
- Config/docs: `config.example.ini` gets `[crash] report=1`, `firstchance=1`, `missingasset=warn` (+ the `[debug]`
  crashtest key in README only).
- Tests: `tests/crash_classify_test.cpp` (every table row, order of rules, unknown module, OOM overriding AV),
  `tests/crash_scrub_test.cpp` (user path, Dialog body, IPv4, truncation), and register both in `tests/CMakeLists.txt`.
- Phase 2 (owner's call): the `src/launcher.cpp` backstop. After `GetExitCodeProcess`, if `(code & 0xC0000000) ==
  0xC0000000 || code == 3` and `LATEST_CRASH.txt` is older than the process start, write a minimal report using the
  same classifier.

Launcher (`/root/work/launcher-160-join`):
- `tauri/core/src/crash_report.rs` (+ `lib.rs` mod) with `find_latest`, `parse_header`, `redact`, `is_new`, and unit
  tests (fixtures with a user path, a Dialog line, CRLF, oversize, missing file).
- `tauri/core/src/site.rs`: `send_crash`. `settings.rs`: `crash_seen_ms` (serde default).
- `tauri/app/src/launcher.rs` + `main.rs:64`: commands `crash_report_latest`, `crash_report_dismiss`,
  `crash_report_send`, `open_crash_folder`, and a check on start-up and window focus.
- `tauri/ui/index.html`, `renderer.mjs`, `style.css`, `bridge.js`: the crash card + `crash-dialog`. Add an `npm test`
  case that Send is disabled until the dialog shows the report.

Site (option, read-only here): `tools/site/report_service.py` accepts type `crash` with a `report` field (≤ 24 KB, body
cap raised for that type only), and adds a test case in `test_report.py`. `report_export.py` shows crash reports
separately.

## Test plan

1. Native unit tests on Silvia (`ctest`): classifier table and scrubber.
2. QA build with `MAPLEROOT_CRASH_REPORT` + `MAPLEROOT_CRASH_TEST` on TEST, in a COPY of the RC client under
   `Z:\MSv83\SlumsMS-dev\`:
   - crashtest=av in field → LATEST_CRASH.txt says "the game crashed / SlumsMS add-on". The map id matches. The build
     matches slums-build.txt. 50 log lines are present. No `C:\Users\` text.
   - crashtest=oom (reserve address space until it fails, then AV) → "ran out of memory".
   - crashtest=stack (recursion) → stack overflow report written (proves the writer thread).
   - crashtest=cxx (unhandled _com_error) → report, or, if the CRT aborts, note exit code 3 (decides whether phase 2
     is needed).
   - crashtest=driver is hard to force. Instead, unit-test the module mapping and inject a fake module name in a
     test hook.
   - Rename Custom.wz → named dialog + `MISSING_ASSET` line; the game continues (warn) or stops (stop).
   - Normal play for 30 min with `firstchance=1`: crash.log has no E06D7363 lines. No LATEST_CRASH.txt after a normal
     exit or a disconnect dialog.
3. Launcher: `cargo test -p slums-core` (crash_report.rs), `npm test`. On Silvia with a --data-dir trial: a report
   appears once and "Not now" hides it. Send posts to the TEST site only, and only after the click. Use a network
   capture or service log to show nothing is sent without the click.
4. Site: `test_report.py` crash type, size limit, rate limit.

## Effort

Medium (the ticket's S-M is fair if phase 2 and the full asset table are left out):
- Client handler + classifier + scrub + tests: ~1.5 days. Required-asset table across addons: ~0.5-1 day.
- Launcher core + commands + UI + tests: ~1 day. Site crash type: ~0.25 day.
- TEST forced-crash pass: ~0.5 day. Phase 2 SlumsMS.exe backstop: +0.5 day, plus the owner's call on shipping a new exe.

## Open questions / unverified

- What the stock ZExceptionHandler top-level filter does (read it near 0x00796FDD in the exe copy).
- Whether an unhandled `_com_error` / ZException reaches the UEF or ends in CRT abort (exit 3).
- Whether a missing Custom.wz throws today, and what HRESULT a missing .img gives.
- `_com_error` object layout for logging its HRESULT (vptr first, so the current read is wrong for it).
- Whether MapleStory.exe is LARGEADDRESSAWARE (sets the OOM threshold).
- How often NameSpace lookups fail during normal play (cost of the failed-lookup ring).
- The receiver choice (site report_service vs a new endpoint) is the owner's.
