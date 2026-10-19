# SlumsMS client source

Full accepted client source: Kaentake, the MapleRoot `ijl15` companion, and the shop/weather/movement/mount-info additions. Existing Kaentake Git history and pinned Detours/WzLib submodules are retained. Matching server: [SlumsMS](https://github.com/joshhmann/SlumsMS).

## Build on Windows

Install Visual Studio 2022 with Desktop development with C++, x86 build tools and a Windows SDK, plus CMake and Git. The repository includes the companion's required import/static libraries and Discord runtime, and a pinned CppWinRT package; `dependencies.json` verifies these inputs.

```powershell
git submodule update --init --recursive
./scripts/build.ps1
```

To run all seven checks using your existing complete test client:

```powershell
./scripts/build.ps1 -TestClient 'C:/path/to/owned-v83-client'
```

Without external Data, only the five tests that do not require game assets are registered. Build errors stop the script before staging or tests; no old DLL is silently accepted. The script builds both projects and enables all accepted addon flags. Output: `build-baseline/Release/Kaentake.dll`, `Kaentake.exe`, `Custom.wz`, and `mapleroot/build/Integrated/ijl15.dll`.

## Runtime inputs

The game Data tree is intentionally external. The accepted checkpoint separately retains the base game executable, supporting runtime DLLs, launcher and the exact manually accepted built DLLs. Install into a private copy of the owned v83 client, apply the matching core and addon assets, and use the server/client release manifest. `config.example.ini` uses login port 8485; the earlier disposable tests used 8488.

Do not overwrite files while the game is open or write assets through a junction into a different client. Launch with the matching host and port. A client-only source build is not a server deployment.

## Preserved and added

MapleRoot gameplay remains in the companion DLL: custom skills, long EXP, expanded equipment/quickslots and existing packet contracts. Kaentake owns startup/resolution and the Custom namespace. Additions include field SHOP, visual weather/time/lamps, direction bindings and multipart character-info mount fitting/name clipping. Optional weather gameplay modifiers and the separate 64-bit damage guide remain excluded.

Both repos use tag `baseline-20261018-r1`. `release/baseline.json` records accepted artifact hashes/flags; the protected release record pins both commits. Seven native tests and full companion/client compilation passed from this repo layout. Archived binaries passed the manual checks documented under docs/evidence, including normal exit and resource release. New builds still require the paired runtime/assets and appropriate smoke testing.

Upstream Kaentake usage and notices are preserved in docs/KAENTAKE-UPSTREAM.md. Original file-level notices and third-party submodules remain intact. The inherited small `src/Custom.wz` UI extension is retained; the large game Data files are not included.
