# Ticket 73: settings panel build and QA

Branch `client-160-settings`, code `c8202e1`, native UI test `1bf9ded`; based on client-1.5.0-fixes3 `85b1f5b`.
The window opens with Ctrl+F10 or the window menu, uses only supported config keys and current defaults, and keeps
legacy moveattack fallback. Save changes take effect on restart; Cancel writes nothing. No game endpoint,
resolution setting, signing key, launcher key, packet or fixed v83 patch address changes.

Build folder on Silvia: `Z:\MSv83\SlumsMS-dev\client-160-settings\src\build\Release`.
All current MAPLEROOT features enabled as in fixes3; MSVC Win32 Release, two build jobs / CL_MPCount=2.
Trial build stamp override: `1.6.0-dev`, build **10600**; the branch VERSION remains unchanged pending release work.

- Linux portable policy test: passes with g++17, -Wall -Wextra -Werror.
- Native configure/build: passes. Missing submodules were initialized only in this task's own clone.
- Native `ctest -C Release --output-on-failure`: **20/20 pass**.
- The new native window test creates only its own temporary owner/panel windows in the SSH test session. It checks
  legacy defaults, real Windows checkbox/INI Save, Cancel without writes, unrelated-host preservation, teardown and
  the runtime off switch. Config fixtures stay in the task build folder. It never starts a game.
- DLL SHA-256: `4408a63d7218957bccd6c446f18f494bac4bfc5086310cc54549f137aa35142e`.
  Local copy: `dist/client-160-settings/slums.dll`. Earlier 19-test build was superseded.

The first PowerShell build wrapper treated a harmless CMake warning as a terminating error. Native subprocess
exit codes are now checked directly, and stderr is captured into the task logs. Final build and CTest are green.

**Still pending:** open the panel in a real TEST game, check keyboard/focus and default font at supported sizes,
save/restart and confirm each option in gameplay, test Cancel and repeated open/close while connected, verify the
system menu/shortcut interaction with game keybinds, and compare the RC file hash diff before publication.
The client-MCP worker did not answer; it was not started or bypassed. No installed client DLL was replaced, no game
was launched, and no Test/RC/Live feed was published. This is a built candidate, not visually cleared for players.
