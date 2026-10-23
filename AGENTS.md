# AGENTS.md: SlumsMS client (slums.dll + Custom.wz)

Read the main handbook first: `AGENTS.md` in the server repo (joshhmann/SlumsMS, branch `test-1.5.0-rc6` or newer).
Its hard rules apply here: live is the owner's, secrets never pass through you, the owner's desktop is read-only
except `Z:\MSv83\SlumsMS-dev\`, and a refused action stays refused.

- **Branches:** the current client line is `client-1.5.0-fixes3`. Make your branch from it.
- **Build** on Silvia (`crims@192.168.0.186`, key `~/.ssh/id_rsprox_desktop`) in a NEW folder under
  `Z:\MSv83\SlumsMS-dev\` (copy the steps used for `client-1.5.0-fixes3`: clone, CMake + MSVC Release, `ctest`).
  Record the DLL's sha256. Native tests live in `tests/` and run on Silvia; add one for every fix.
- **Addresses:** patches are hooks, code caves and byte writes at v83 addresses. Read stock bytes from the exe copy
  (`Z:\MSv83\SlumsMS-dev\client-1.5.0-fixes2\exe-copy\MapleStory.exe`, file offset = VA - 0x400000) before changing
  anything. Keep features switchable from `config.ini` where you can; document keys in README and `config.example.ini`.
- **Server agreements:** packets shared with the server (e.g. LP 0x3737 bounce sync, CP 0x3738 build stamp) have golden
  tests on both sides. Change both together and keep old clients/servers working (versioned packets).
- **Test** with a COPY of the RC client under `SlumsMS-dev`, never the owner's installs. A locked DLL: ship yours as
  `slums.dll.new`. Startup error -2147467259 on Silvia = Parsec main display, not your bug.
- **Publish** RC builds with the server repo's `tools/release/stage-client` + `publish-client` to the RC channel
  (`/srv/slums-patch/launcher-rc`). Hash-diff first: only intended files may differ. Live publishes are the owner's go,
  and a Live client must point at slumsms.asslorde.com:8485.
