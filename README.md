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
  major*10000 + (minor+1)*100 + patch*10 (1.6.2 -> 10720; 1.6.2-rc1 is also 10720). Up to 1.6.0 it was
  major*10000 + minor*100 + patch; the change (b205) keeps every new build above the 1.6.1 candidates (10700, 10710).
  `cmake/BuildStamp.cmake` checks them.
- A release bumps `VERSION` in its release commit, nothing else. The build prints
  `slums.dll build stamp: <release> (build <n>)` and writes `build/Release/slums-build.txt` (`release=`, `build=`);
  the server release sets `MIN_CLIENT_BUILD` to that build (SlumsMS `tools/release/set-min-client-build`).
- One-off builds may pass `-DSLUMS_RELEASE_OVERRIDE=` / `-DSLUMS_BUILD_NUMBER_OVERRIDE=` (remove with `-U`).
- `buildstamp_wire_format` (ctest) checks the bytes against the server's golden packet.
- Players see the same numbers: the window title is `SlumsMS <release> (<build>)`, the login screen's bottom-right
  corner shows `SlumsMS <release> · build <build>` (`src/buildshow.cpp`), and slums.dll carries a version resource
  (FileVersion `1.6.2.10720`, ProductVersion the release, custom `SlumsBuild` = the build; `src/slums.rc.in`).

## Server address

`config.ini` `[config] host=` takes a host name (for example `slumsms.asslorde.com`) or an IPv4 address, and `port=` the login port. A name is resolved once, on the first connection.

## Movement during attacks

MapleRoot's single `[clienthax] moveattack` switch is split into three keys, one per client patch (`src/attackstop_patch.h`):

| Key | Patch | Default |
| --- | --- | --- |
| `attackwalk` | keep reading walk input during attacks, the "slide" (`0x009CBFB0`) | `false` (vanilla: attacks stop you) |
| `attackjump` | jump during attack animations (`0x0094C3BB`) | `true` |
| `chargemove` | move while charging keydown skills (`0x0095F97A`) | `true` |

Precedence: a key that is set wins for its own patch. A missing `attackjump`/`chargemove` follows the legacy `moveattack=` (default `true`; `moveattack=false` turns both off). `attackwalk` never follows `moveattack`; it is off unless `attackwalk=true`. A disabled site is restored to vanilla bytes if something else (such as a stray MapleRoot `ijl15.dll`) already patched it. `jumpshoot` and `jumpmagic` stay separate keys.

## Weather sway switches

`src/weathersway.cpp` bends plants, trees, hanging signs, ropes and ladders by building sheared frames of each map object when the map loads. Two `config.ini` keys turn it off for A/B testing; they are read once at startup:

| Key | What | Default |
| --- | --- | --- |
| `[weather] sway` | `false`: nothing bends; every object keeps the map's own layer | `true` |
| `[weather] ropesway` | `false`: ropes and ladders keep the map's own layer; plants still bend | `true` |

## Resizable item inventory (1.6.0)

Built with `-DMAPLEROOT_INV_RESIZE=ON`. Drag the inventory's right edge, bottom edge or bottom-right corner to change how many columns (4-16) and rows (3-14) are visible, at most 120 slots; the grid snaps to whole slots and the size is saved per character (`HKCU\Software\SlumsMS\Inventory`, value `w<world>_c<id>`; the client rewrites its own per-character key on login, so it cannot live there). The expand button becomes a 4 <-> 8 column switch: the 4-tab expanded view is gone while this is on. `src/invresize.cpp` replaces `CUIItem::GetSlotRect` (`0x0081E2C8`) and `GetSlotPositionFromPoint` (`0x0081DB7E`), detours `OnCreate` (`0x0081C6C9`), replaces `ToggleFull` (`0x0081E541`) and rewrites 14 layout operands; `src/invresize_patch.h` lists every site and its stock bytes. Slots the tab does not have yet (past the character's slot count) show the client's own locked-slot art (`Item/disabled`), and the new-item tab flash is centred on the stretched tab instead of the stock 34px position (`Draw` detour at `0x0081DC20`, call at `0x0081E658`). If any byte differs nothing is patched and the stock window stays.

| Key | What | Default |
| --- | --- | --- |
| `[clienthax] invresize` | `false`: patch nothing; the stock 4x6 window with its 4-tab expanded view | `true` |

## Mob touch damage and low defence (1.6.0)

The client computes the damage a mob's touch does to you (`CalcDamage` mob physical damage, `0x0079309F`) and the server applies it. Stock v83 compares your defence with `StandardPDD.img`'s value for your job class and level; below it, the defence term turns into extra damage scaled by STR/DEX/INT/LUK. A level 200 admin with 32767 stats took ~900 from a snail after `!job` (GM jobs have no table row, so the standard is 0 until the job changes). `src/mobdamage.cpp` reroutes the branch at `0x0079328D` so a defence below the standard counts as the standard: defence only ever reduces damage. Characters at or above their standard are unchanged. `src/mobdamage_patch.h` has the formula and the checked bytes.

| Key | What | Default |
| --- | --- | --- |
| `[clienthax] lowdefensepenalty` | `true`: keep the stock rule (low defence adds damage) | `false` |

## 4-byte HP and MP (1.6.0)

The 1.6.0 server sends HP, MaxHP, MP and MaxMP as 4 bytes, so they can go past 32,767 (cap 99,999). `src/hpmp.cpp` is always on and needs a 1.6.0 server; the build stamp (10600 and up) is what the server checks. The client reads the 9 widened packet fields with `Decode4`, stores the four stats raw (checksum 0) instead of as encrypted shorts, and detours `_ZtlSecureFuse<short>` to return them whole. It also widens the 57 places that cut a read-back value to 16 bits and raises the four 30,000 caps. `src/hpmp_patch.h` has the site table and how it was derived. Every site's bytes are checked first. If any differ, or another patch later overwrites one, the client shows an error and exits rather than misreading the server's stats.

## Direct3D 9 host and in-game settings (1.6.0)

The client draws through d3d8to9 (`d3d8.dll` imports `d3d9!Direct3DCreate9`). `src/d3d9host.cpp` detours `Direct3DCreate9` and, on the first `IDirect3D9`, logs every adapter (name, display, mode, primary, HAL caps) and every failing adapter query to `integration.log` (`D3D9:` lines), to pin down the start-up error `-2147467259` with Parsec or another virtual display as the main display. If `CreateDevice` fails it is retried on the other adapters, then with software vertex processing (`src/d3d9host_policy.h`). On the created device `EndScene` and `Reset` are detoured to host Dear ImGui (1.91.4, already built into the MapleRoot companion): **Ctrl+F10** opens the SlumsMS HUD window (`src/settings_overlay.cpp`); Esc or Ctrl+F10 closes it. Game options stay in the launcher, which owns those `config.ini` switches and changes them only while the game is closed. The same overlay draws the HUD (`src/overlay.cpp`): always-on widgets in the client's blue and cream (or plain ImGui), scaled to the resolution, positions saved as screen fractions in `HKCU\Software\SlumsMS\Overlay`. **Ctrl+F11** (or the settings window) freezes/unfreezes it: unfrozen widgets can be dragged and resized, frozen ones are fixed and click-through. The pilot widget is HP / MP / EXP (`src/hud_vitals.cpp`, ticket 91): the same values the stock bar shows, with MapleRoot's 64-bit EXP; the stock bar stays on. Widgets are switched on/off, resized and re-themed in the HUD window, live.

| Key | What | Default |
| --- | --- | --- |
| `[graphics] d3d9host` | `false`: no logging, no fallback, no overlay | `true` |
| `[graphics] adapterfallback` | `false`: log only, no CreateDevice retries | `true` |
| `[clienthax] settingswindow` | `false`: no HUD and Ctrl+F10/F11 do nothing | `true` |

## Movable status bar and quick slot (1.6.0)

**Edit UI** (Ctrl+F11, or the checkbox in the Ctrl+F10 HUD window) is one switch for the whole layout. While it is on, in the field, the HUD widgets can be dragged and resized, and the stock status bar and quick slot each get an outlined grip that drags them live. Turn it off and everything is fixed again, and every click goes to the game. `src/uilayout.cpp` moves an element through its origin vector: `CWndMan::ms_pOrgStatusBar` / `ms_pOrgQuickSlot`, placed by `ResetOrgWindow` in `src/wvs/wndman.h`. Quick slot clicks already work from both origins (`GetShortCutIndexByPos_hook`). Offsets are kept per character in `HKCU\Software\SlumsMS\Layout` (`w<world>_c<id>_bar` / `_qs`) as screen fractions, so they follow resolution changes, and are clamped so the element stays on screen (`src/uilayout_policy.h`). The status bar's buttons are modules too: **Buttons** (Shop, Trade, Menu, Shortcut) and **Icon buttons** (the small row above them) each drag as one group, with the stock art and the stock click handling. They are the bar window's own child controls (`CWnd::m_lpChildren`), moved through each control's position vector and kept inside the bar window. `CUIStatusBar::HitTest` (`0x008DE7DA`) normally drops clicks above the bar's bottom strip, so it is hooked to keep a hit on a moved control. For QA only, `config.ini [debug] uistate=1` makes the client write `slums-ui-state.json` twice a second (stage, chat focus, real-pixel rects of the bar, chat input, quick slot and button groups). The client-mcp driver reads it instead of guessing from screenshots.

HP, MP and EXP are separate HUD modules (the first of the modular HUD): each can be shown or hidden, moved, resized, and given its own text size and opacity in the HUD window. In Edit UI, modules and the stock elements snap to a 1.25% grid (faint lines every 5%; "Snap to grid while editing" turns it off). "Reset UI layout" in the HUD window puts the widgets, the bar, the quick slot and the inventory size back to stock. The grips need the D3D9 host (`[graphics] d3d9host`); saved positions apply without it.

## Bounces: double-tap jump and blocked maps

The server gives every character MapleRoot's Ninja Bounce (1050 / 10001050 / 20001050, "Jump Around!") and Vertical Bounce (1054 / 10001054 / 20001054, "Jump Upwards!"). MapleRoot makes them work by routing those ids through `CUserLocal::DoActiveSkill` (its detour) into the thieves' bound jump, `CUserLocal::DoActiveSkill_BoundJump` (`0x0096BE38`, cave at `0x0096BF0B`). `src/bounce.cpp` and `src/bounce_rules.h` add:

| Key | What | Default |
| --- | --- | --- |
| `[clienthax] doubletapjump` | `true` turns it on: a fresh jump press while airborne (no foothold, not on a ladder/rope, not swimming or in a fly map; keyboard auto-repeat ignored) also uses the job family's Ninja Bounce, the exact skill (and so the exact impulse) a Ninja Bounce hotkey uses; never real Flash Jump or Vertical Bounce; one per airtime as before | `false` |

Bounce strength: MapleRoot's `FlashJumpAll` cave (`SkillEdits/Activeskill.h`, at `0x0096BF0B`) already rewrote the bound jump's impulse immediates (`0x0096C00A`/`0x0096C021`/`0x0096C031`: left vx, right vx, vy) on every use; it now calls `SlumsBounce_ApplyImpulse`, which picks them from the skill and the character's current job. Thieves (4xx) and Night Walkers (14xx) keep MapleRoot's Ninja Bounce -683/606/-688 and Vertical Bounce -1/0/-1200 (real Flash Jump uses the same Ninja values); everyone else gets stock Flash Jump strength -350/350/-250 for Ninja Bounce and -1/0/-750 for Vertical Bounce. The server sends all four sets in a version 2 `0x3737` packet after the zones (`impulses:` in the server's `bounce-zones.yaml`, `!bounce reload` pushes changes); without it every job keeps MapleRoot's values. Ninja Bounce (1050 / 10001050 / 20001050) works only in the air, like stock Flash Jump: on the ground, a rope or ladder, or swimming, the DoActiveSkill detour refuses it before anything is sent (no request, MP, cooldown or animation). Vertical Bounce still works from the ground. Double-tap jump uses the Ninja Bounce, never Flash Jump or Vertical Bounce. Hermit Flash Jump (4111006) skips the cave in the stock code (`0x0096BF02`), so its impulse is written from the DoActiveSkill detour instead.

The patch is installed once at startup: the jump key's `call CUserLocal::Jump` in `CUserLocal::OnKeyDown` (`0x0094F5A7`) gets a new rel32, nothing else; `Jump` itself and MapleRoot's attackjump bytes (`0x0094C3BB`) are untouched. With the key off nothing is patched.

Blocked maps: on maps the server lists (its `bounce-zones.yaml`; jump quests by default) the six bounce ids do nothing, from a hotkey, a macro or a double-tap, and on jump quests real Flash Jump is refused too, as stock v83 fieldLimit already did. The server sends the list at login (LP `0x3737` `BOUNCE_ZONES_SYNC`); until then the built-in jump-quest list (`src/bounce_zones_builtin.inc`, generated from the server's yaml) applies. `tests/bounce_rules_test.cpp` checks both against the server's golden packet.

## Runtime inputs

The game Data tree is intentionally external. The accepted checkpoint separately retains the base game executable, supporting runtime DLLs, launcher and the exact manually accepted built DLLs. Install into a private copy of the owned v83 client, apply the matching core and addon assets, and use the server/client release manifest. `config.example.ini` uses login port 8485; the earlier disposable tests used 8488.

Do not overwrite files while the game is open or write assets through a junction into a different client. Launch with the matching host and port. A client-only source build is not a server deployment.

## Preserved and added

MapleRoot gameplay remains in the companion code (`mapleroot/ijl15`, linked into `slums.dll`; it no longer ships as a separate `ijl15.dll` proxy): custom skills, long EXP, expanded equipment/quickslots and existing packet contracts. Kaentake owns startup/resolution and the Custom namespace. Additions include field SHOP, visual weather/time/lamps, direction bindings and multipart character-info mount fitting/name clipping. Optional weather gameplay modifiers and the separate 64-bit damage guide remain excluded.

Both repos use tag `baseline-20261018-r1`. `release/baseline.json` records accepted artifact hashes/flags; the protected release record pins both commits. Seven native tests and full companion/client compilation passed from this repo layout. Archived binaries passed the manual checks documented under docs/evidence, including normal exit and resource release. New builds still require the paired runtime/assets and appropriate smoke testing.

Upstream Kaentake usage and notices are preserved in docs/KAENTAKE-UPSTREAM.md. Original file-level notices and third-party submodules remain intact. The inherited small `src/Custom.wz` UI extension is retained; the large game Data files are not included.


## Parsec startup workaround

On the deployment PC, starting the client while connected to a Parsec virtual display produced `-2147467259` during graphics initialization, before login. Disconnecting Parsec, launching the game, then reconnecting allowed entry with the original checkpoint files. This is an observed workaround for that machine/session; the underlying driver cause is not confirmed. Resolution changes, a GPU preference override and removing the DirectX wrapper did not resolve it, and those trials were reverted. No driver reset or client code change was required.
