# AGENTS.md: SlumsMS client work on Silvia (slums.dll + Custom.wz)

For any agent working on the SlumsMS client, whether it runs on Silvia itself or reaches it over ssh. The server
repo's `AGENTS.md` (joshhmann/SlumsMS) is the main handbook; its hard rules apply here too.

## Hard rules
- **Write only under `Z:\MSv83\SlumsMS-dev\`.** Never touch the owner's installs, `%APPDATA%`, `test-client\`, or
  anything outside this folder. Never kill processes; the one exception is a stuck QA client started from
  `Z:\MSv83\SlumsMS-dev\client-mcp\client` (check the path first).
- **Live is the owner's.** No launcher or client publishes, no Live server, Live DB or Live feed access. Server and
  TEST changes go through the server lane (Server Dev session).
- **Secrets never pass through you:** patch access key, dev key, Tauri signing key, cloud LLM keys, webhook URLs,
  DB passwords, `client-mcp\secrets\`. Refer to them by path only.
- **Never `git stash`.** One branch per patch. Commit work in progress instead.
- **Mod originals and other people's code: read and port, never run.**
- **A refused action stays refused.** If a rule or a permission check stops you, stop and report it.
- **The owner's go is needed for anything Live** and for any launcher channel publish, every time.

## Branches
- Live client code: `client-162-rc8` (1.6.2 layer, build 10721). Branch new work from it unless told otherwise.
- b251 trace work: `client-b251-wztrace` (v1) and `client-b251-codex` (v2-v4: E_POINTER read, stacks, fresh log
  per start, 64 MB cap).
- Client map integrity: ticket 181,
  `/root/work/SlumsMS-164-client-map-ticket/docs/tickets/181-client-map-integrity-and-crashes.md` on the 187 box.

## Build (Silvia, MSVC)
1. A NEW folder per build: `Z:\MSv83\SlumsMS-dev\client-<version>-<topic>\`.
2. Clone from an existing build folder (e.g. `client-1.6.2-rc5\src`), fetch your branch (a FULL `git bundle` if it
   comes from elsewhere), `git submodule update --init --recursive`, run `scripts\prepare-dependencies.ps1`.
3. Configure and build like `client-1.6.2-qawztrace4\prep-build-qawztrace4.ps1` (Visual Studio 17 2022, Win32,
   Release, `-DSLUMS_BUILD_NUMBER_OVERRIDE=10721`, the MAPLEROOT_* feature switches; add `-DSLUMS_QA_MODULE=ON`
   only for QA builds), then `ctest -C Release`. Inline PowerShell through ssh breaks on quotes: run a `.ps1` file.
4. Record `slums.dll` sha256 and `slums-build.txt`. Ship only `slums.dll` (and `Custom.wz` if it changed);
   `SlumsMS.exe` is not reproducible between builds.
5. Every fix gets a native test in `tests/` that fails before and passes after.

## Addresses and bytes
Patches are hooks, code caves and byte writes at v83 addresses. Verify every address and every byte against the stock
exe before patching (`client-1.5.0-fixes2\exe-copy\MapleStory.exe`, sha256 86e4e6be...ac90, file offset =
VA - 0x400000), quote the verified bytes in the code comment, and check them again at runtime before detouring.
Keep features switchable from `config.ini`; document keys in README and `config.example.ini`. Packets shared with the
server (LP 0x3737, CP 0x3738 build stamp) change together with the server, with golden tests on both sides.

## QA
- QA client: `Z:\MSv83\SlumsMS-dev\client-mcp\client` (run `SlumsMS.exe` as administrator), TEST server only
  (`config.ini` host=192.168.0.111). `Start-QA.cmd` starts the worker for scripted runs; one worker at a time.
- Crash traces: `config.ini` `[debug] wztrace=2`; each start writes a fresh `wztrace.log` next to the exe (older runs
  are `wztrace-<UTC>.log`, five kept). Read `throw ... hr=` lines with their `frame[...]` stacks; `docs/wztrace.md`.
- Maps on the server's crash-routing list are blocked on TEST; a TEST-only unblock is the server lane's call.
- Map data checks: `tools/wz/check_map_refs.py` (branch `client-162-rosegarden`) finds maps whose objects, tiles,
  backs, links or BGM the client cannot resolve.

## Tools (`Z:\MSv83\Tools`)
- `HaRepacker_win-x64`: open and edit WZ files (inspect nodes, export/import images).
- `HaCreator_win-x64`: map editor (view a map, its objects and layers).
- `SoloMapling-wisteria-Wz-Mod-Tool-Suite-main`: WZ mod tool suite; read its README before use.
Work on copies under `SlumsMS-dev`, never on a client's live WZ files.

## Hand back
Commits on your branch, a clean worktree, and a short REPORT.md next to the build: what changed, verified bytes,
build folder, DLL sha256, test counts, what still needs the owner or the server lane.
