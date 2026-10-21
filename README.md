# SlumsMS client source

Full client source: Kaentake (renamed SlumsMS), the MapleRoot companion (built into the same DLL), and the shop/weather/movement/mount-info additions. Existing Kaentake Git history and pinned Detours/WzLib submodules are retained. Matching server: [SlumsMS](https://github.com/joshhmann/SlumsMS).

## Build on Windows

Install Visual Studio 2022 with Desktop development with C++, x86 build tools and a Windows SDK, plus CMake and Git. The repository includes the Discord SDK import library and runtime, the original Intel `ijl15.dll`, and a pinned CppWinRT package; `dependencies.json` verifies these inputs.

```powershell
git submodule update --init --recursive
./scripts/build.ps1
```

To run all seven checks using your existing complete test client:

```powershell
./scripts/build.ps1 -TestClient 'C:/path/to/owned-v83-client'
```

Without external Data, only the five tests that do not require game assets are registered. Build errors stop the script before staging or tests; no old DLL is silently accepted. The script builds one CMake project with all accepted addon flags. Output in `build/Release`: `SlumsMS.exe` (starter), `slums.dll` (everything: Kaentake plus the MapleRoot companion), the original Intel `ijl15.dll`, `discord_game_sdk.dll` and `Custom.wz`. Players start `SlumsMS.exe`; it launches `MapleStory.exe` with `slums.dll`.

## Build stamp (version gate)

slums.dll tells the login server which build it is: CP `0x3738` (format 1, int build, string release) right before
every password packet (`src/buildstamp_proto.h`, sent from `CLogin::SendCheckPasswordPacket_hook`). The server refuses
logins below its `MIN_CLIENT_BUILD` with "Your game is out of date: open the SlumsMS launcher to update."; clients
without the stamp (1.4.1 and older) count as build 0.

- The numbers come from `VERSION` (repo root) on every configure: release = its text, build =
  major*10000 + minor*100 + patch (1.4.2 -> 10402; 1.4.2-rc1 is also 10402). `cmake/BuildStamp.cmake` checks them.
- A release bumps `VERSION` in its release commit, nothing else. The build prints
  `slums.dll build stamp: <release> (build <n>)` and writes `build/Release/slums-build.txt` (`release=`, `build=`);
  the server release sets `MIN_CLIENT_BUILD` to that build (SlumsMS `tools/release/set-min-client-build`).
- One-off builds may pass `-DSLUMS_RELEASE_OVERRIDE=` / `-DSLUMS_BUILD_NUMBER_OVERRIDE=` (remove with `-U`).
- `buildstamp_wire_format` (ctest) checks the bytes against the server's golden packet.

## Server address

`config.ini` `[config] host=` takes a host name (for example `slumsms.asslorde.com`) or an IPv4 address, and `port=` the login port. A name is resolved once, on the first connection.

## Runtime inputs

The game Data tree is intentionally external. The accepted checkpoint separately retains the base game executable, supporting runtime DLLs, launcher and the exact manually accepted built DLLs. Install into a private copy of the owned v83 client, apply the matching core and addon assets, and use the server/client release manifest. `config.example.ini` uses login port 8485; the earlier disposable tests used 8488.

Do not overwrite files while the game is open or write assets through a junction into a different client. Launch with the matching host and port. A client-only source build is not a server deployment.

## Preserved and added

MapleRoot gameplay remains in the companion code (`mapleroot/ijl15`, linked into `slums.dll`; it no longer ships as a separate `ijl15.dll` proxy): custom skills, long EXP, expanded equipment/quickslots and existing packet contracts. Kaentake owns startup/resolution and the Custom namespace. Additions include field SHOP, visual weather/time/lamps, direction bindings and multipart character-info mount fitting/name clipping. Optional weather gameplay modifiers and the separate 64-bit damage guide remain excluded.

Both repos use tag `baseline-20261018-r1`. `release/baseline.json` records accepted artifact hashes/flags; the protected release record pins both commits. Seven native tests and full companion/client compilation passed from this repo layout. Archived binaries passed the manual checks documented under docs/evidence, including normal exit and resource release. New builds still require the paired runtime/assets and appropriate smoke testing.

Upstream Kaentake usage and notices are preserved in docs/KAENTAKE-UPSTREAM.md. Original file-level notices and third-party submodules remain intact. The inherited small `src/Custom.wz` UI extension is retained; the large game Data files are not included.


## Parsec startup workaround

On the deployment PC, starting the client while connected to a Parsec virtual display produced `-2147467259` during graphics initialization, before login. Disconnecting Parsec, launching the game, then reconnecting allowed entry with the original checkpoint files. This is an observed workaround for that machine/session; the underlying driver cause is not confirmed. Resolution changes, a GPU preference override and removing the DirectX wrapper did not resolve it, and those trials were reverted. No driver reset or client code change was required.
