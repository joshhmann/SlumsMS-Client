# Tickets 123 (chat tabs, colours, blocklists) and 124 (mouse remap, controller): research note

Read-only research, 2026-10-28. Source tree: `/root/work/client/src-161-qa` (HEAD ca7c572). Exe checked:
`scratchpad/client/MapleStory.exe` (sha256 86e4e6be…, unpacked, image base 0x400000, file offset = VA - 0x400000;
.text 0x401000-0xAF0000). Names come from `/root/Maplestory/Angel.idb` (v83.idb has almost no names). Every address
below was disassembled from the exe bytes with `objdump -d -M intel`. Anything marked **UNVERIFIED** was inferred and
not checked in game.

Related plan: `/root/work/client/src-170-rmlui-plan/docs/170-rmlui-hud-plan.md` (branch with commit 24aabba) already
names `CUIStatusBar::ChatLogAdd 0x008DB070` as the chat hook and puts chat last in 1.7.0 ("skin and tabs on top of
the stock chat first"). Ticket 123 folds into that.

---

## Verified addresses (stock bytes)

| Function (Angel.idb name) | VA | First bytes | Conv. / args |
| --- | --- | --- | --- |
| `CHATLOG_ADD(ZXString<char>&, unsigned short type)` | 0x004906B5 | `b8 a8 0c a8 00 e8 d9 04 5d 00 51 33` | cdecl; 392 call sites; forwards to ChatLogAdd(text, type, -1, 0, ZRef{}) |
| `CUIStatusBar::ChatLogAdd(const char*, long type, long channel, int, ZRef<GW_ItemSlotBase>)` | 0x008DB070 | `b8 4a 2a ad 00 e8 1e 5b 18 00 83 ec` | thiscall, `ret 0x18` (4+4+4+4+8 bytes); 21 direct callers |
| `CUIStatusBar::CChatLog::CChatLog(ZXString<wchar_t>, type, colour, …, ZRef)` | 0x008DB485 | `b8 82 2a ad 00 e8 09 57 18 00 51 53` | thiscall, `ret 0x20`, object 0x34 bytes |
| `CUIStatusBar::ChatLogDraw()` | 0x008DBB93 | `b8 21 2f ad 00 e8 fb 4f 18 00 81 ec` | thiscall, no args |
| `CUIStatusBar::OnCreate(void*)` (creates the 23 chat fonts) | 0x008D01B2 | `b8 90 1d ad 00 e8 dc 09 19 00 81 ec` | thiscall |
| `CUIStatusBar::SetChatType(long)` (send target selector) | 0x008D4A6D | `b8 be 1f ad 00 e8 21 c1 18 00 8b 45` | thiscall |
| `CUIStatusBar::SetWhisperTarget(ZXString<char>, int)` | 0x008DB01C | (name only) | thiscall |
| `CUIStatusBar::SendGroupMessage(long, const ZXString<char>&)` | 0x008DF155 | `b8 1c 32 ad 00 e8 39 1a 18 00 83 ec` | thiscall |
| `CUser::OnChat(CInPacket&)` | 0x00937138 | `b8 fb c4 ad 00 e8 56 9a 12 00 83 ec` | thiscall |
| `CField::OnWhisper(CInPacket&)` | 0x0053228E | `b8 cc b8 a8 00 e8 00 e9 52 00 81 ec` | thiscall |
| `CField::OnGroupMessage(CInPacket&)` | 0x00531E00 | `b8 c4 b6 a8 00 e8 8e ed 52 00 83 ec` | thiscall |
| `CWvsContext::OnBroadcastMsg(CInPacket&)` | 0x00A22785 | `b8 cc c6 ae 00 e8 09 e4 03 00 81 ec` | thiscall |
| `CField::SendChatMsg(const ZXString<char>&, int)` | 0x0052C315 | `b8 f4 ac a8 00 e8 79 48 53 00 83 ec` | thiscall |
| `ZRef<GW_ItemSlotBase>::~ZRef()` | 0x00428A50 | — | thiscall (ChatLogAdd releases its ZRef arg with it at 0x8DB447) |
| `CInputSystem::Init(HWND, HANDLE*)` | 0x00599EBF | `b8 ac ff a8 00 e8 cf 6c 4c 00 83 ec` | thiscall |
| `CInputSystem::SetCursorVectorPos(x, y)` | 0x0059A0CB | `b8 c8 ff a8 00 e8 c3 6a 4c 00 83 ec` | thiscall (hooked in resolution.cpp:217) |
| `CInputSystem::IsKeyPressed(int vk)` | 0x0059A25A | `8b 44 24 04 33 d2 3b c2 7c 59 3d 00` | thiscall (hooked in movementkeys.cpp:30) |
| `CInputSystem::UpdateDevice(int idx)` | 0x0059A2E9 | `8b 44 24 04 83 e8 00 74 0a 48 75 0e` | thiscall; 0 → UpdateKeyboard(1), 1 → UpdateMouse, 2 → nothing |
| `CInputSystem::GetISMessage(ISMSG*)` | 0x0059A306 | `83 b9 c4 09 00 00 00 75 04 33 c0 eb` | thiscall |
| `CInputSystem::GetCursorPos(POINT*)` | 0x0059A388 | `56 8b f1 83 7e 0c 00 74 1a 8b 8e 38` | thiscall; DI mode returns +0x938/+0x93C |
| `CInputSystem::SetCursorPos(x, y)` | 0x0059A887 | `55 8b ec 51 51 56 8b f1 8b 4d 08 85` | thiscall (replaced by resolution.cpp:218) |
| `CInputSystem::TryAcquireDevice(int)` | 0x0059A911 | `55 8b ec 81 ec 14 01 00 00 8b 45 08` | thiscall |
| `CInputSystem::UpdateMouse()` | 0x0059AAFE | `55 8b ec 83 ec 34 53 56 8b f1 57 8b` | thiscall |
| `CInputSystem::UpdateKeyboard(int bGenMsg)` | 0x0059AF77 | `55 8b ec 81 ec 10 02 00 00 53 56 8b` | thiscall |
| UpdateKeyboard snapshot point (movement keys' jmp) | 0x0059B00A | `f6 85 2a ff ff ff 80` (`test byte [ebp-0xD6],0x80`) | patched by movementkeys.cpp:213 |
| `CInputSystem::GenerateAutoKeyDown(ISMSG*)` | 0x0059B2D2 | `53 56 57 8b f1 33 ff 39 7e 08 0f 84` | thiscall |
| `CInputSystem::AddISMessage(…)` (returns a 12-byte ISMSG slot) | 0x0059B382 | `56 8b f1 6a 00 ff 76 10 e8 d6 0c 00` | thiscall on `input+0x9BC` |
| `CWvsApp::ISMsgProc(msg, wParam, lParam)` | 0x009F97B7 | `55 8b ec 8b 55 08 b8 00 01 00 00 3b` | thiscall (called from our Run_hook, bypass.cpp:327/362) |
| `CFuncKeyMappedMan::FuncKeyMapped(long)` | 0x0058E12B | `8b 44 24 04 83 f8 07 77 47 ff 24 85` | thiscall |
| `CUserLocal::UseFuncKeyMapped(unsigned)` | 0x0094F2ED | `b8 e4 e4 ad 00 e8 a1 18 11 00 83 ec` | thiscall |
| `CWvsContext::UseFuncKeyMapped(unsigned)` | 0x00A0773D | `b8 37 9a ae 00 e8 51 94 05 00 81 ec` | thiscall |
| `CWndMan::ProcessKey(…)` | 0x009E40F1 | `b8 7c 6f ae 00 e8 9d ca 07 00 53 56` | thiscall |

Globals: `TSingleton<CInputSystem>` 0x00BEC33C (bypass.cpp:118, resolution.cpp:213), `CWndMan` 0x00BEC20C, key map
manager 0x00BED5A0 (movementkeys.cpp:15: FUNCKEY_MAPPED records `{u8 type; i32 id}` at `+4 + scan*5`, indexed by
DirectInput scan code), KeyConfig-open flag 0x00BED650, status bar singleton read by CHATLOG_ADD at 0x00BEC208,
global chat log list 0x00BF1100 (trimmed to 0x40 = 64 entries at 0x8DB387).

Hooks with `b8 xx xx xx xx e8 rel32` prologues (SEH frame setup) are already detoured elsewhere in this repo with
Detours/ATTACH_HOOK; Detours relocates the rel32 call.

---

## 123: Chat tabs, colours and blocklists

### Summary
Every chat-log line funnels through one function, `CUIStatusBar::ChatLogAdd` 0x008DB070, either directly (21 callers)
or via `CHATLOG_ADD` 0x004906B5 (392 callers). It gets the text (ANSI `const char*`) and an integer chat type
0..22. One detour there can drop lines (blocklists, the system/tip switch) and copy each line with its type into our
own buffer. Tabs and per-type colours are best done in an overlay chat module that reads that buffer. Adding tabs to
the stock window would mean rewriting ChatLogDraw's list walk and scroll bar, and the 1.7.0 RmlUi skin replaces that
window anyway. Recommendation: a **hybrid**. Filtering stays in the stock path, so it also works with the stock
window. Tabs and colours go in an ImGui module now, and the same buffer and policy later feed the RmlUi chat panel.

### Chat types and colours (from the exe)
`CUIStatusBar` keeps an array of 0x17 (23) `IWzFont` pointers at `+0xBE0` (ctor 0x8CFBFB). OnCreate (0x8D01B2)
creates each one as Arial (StringPool 0x1597), size 12, with the colour below. ChatLogAdd picks the font as
`[this + type*4 + 0xBE0]` (0x8DB19B) and a background colour by type (jump table at 0x8DB45D, types 11..20).
The meanings come from the callers (types pushed before the call), so they are **inferred**:

| Type | Text ARGB | Background | Who uses it (call sites found) |
| --- | --- | --- | --- |
| 0 | FFFFFFFF | – | normal map chat, `CUser::OnChat` 0x9373D9 (`"%s : %s"`, StringPool 0x704) |
| 1 | FF00FF00 | – | whisper in/out, `CField::OnWhisper` (`"%s>> %s"` 0x705, `"%s[%s]>> %s"` 0x706, `"%s<< %s"` 0x707) |
| 2 | FFFF99CC | – | party, `OnGroupMessage` group 1 |
| 3 | FFFF9900 | – | buddy, `OnGroupMessage` group 0 |
| 4 | FFE1ACFE | – | guild, `OnGroupMessage` group 2 |
| 5 | FFA6FF7F | – | alliance, `OnGroupMessage` group 3 |
| 6 | FFFF28A7 | – | spouse, `CField::OnCoupleMessage` |
| 7 | FFBBBBBB | – | grey system: EXP gain (`OnIncEXPMessage`), `CUser::OnEffect` |
| 8 | FFFFFF00 | – | yellow notices: drop pickup, curse filter (OnChat 0x9373EA), broadcast |
| 9 | FFFFF080 | – | (not traced) |
| 10 | FF60CEFF | – | `OnBroadcastMsg` notice (0xA22A63, 0xA22B47) |
| 11 | FF000000 | B0FFFFFF | GM/admin chat (`OnChat`: admin flag → 0xB) |
| 12 | FFFFAFAF | – | client errors/system results: party/guild results, skills, `SendGroupMessage` errors |
| 13 | FF003F7F | B0CAE7FF | `OnBroadcastMsg` (megaphone family, **inferred**) |
| 14 | FF770042 | CCFFBFDD | `OnBroadcastMsg` direct ChatLogAdd (super megaphone, **inferred**) |
| 15 | FF462706 | DDFFC600 | (not traced) |
| 16 | FF6C4CE3 | – | (not traced) |
| 17 | FFFC8BE5 | 2C4D1AAD | (not traced) |
| 18 | FF000000 | FF99CC33 or 80FF5C59 (by `channel != -1`) | `OnBroadcastMsg` item megaphone with ZRef item (**inferred**) |
| 19 | FFFFFFFF | 80FF5C59 | (not traced) |
| 20 | FF462705 | FF99CC33 | (not traced) |
| 21 | FFBBBBBB | – | (not traced) |
| 22 | FFFFFF00 (**UNVERIFIED** slot: last font call at 0x8D1245 writes through esi) | – | (not traced) |

Tab mapping: All = everything; Party = 2; Guild = 4 and 5; Whisper = 1; System = 7, 8, 9, 10, 12, 13-22 (and 6
optional). Tips/system switch = 7 and 12 (and 8 by option).

### Existing code
- No chat log code yet. Chat **input** row is a moved module: `src/hud_native.cpp:84-88, 348-373` (`UpdateChatRow`),
  `src/uilayout.cpp` (`UiLayout::kChatInput`, chat input control id 1011, selector 1012, per
  `docs/160-ui-adjust-plan.md:98`).
- ImGui modules and settings: `src/overlay.cpp` (module rows, line 309 onward; line 58 says chat follows in 1.7.0),
  `src/d3d9host.cpp:160-182` (`DrawOverlay`, EndScene hook 184), `src/settings_overlay.cpp`.
- Registry store pattern: `src/uilayout.cpp:43` (`Software\\SlumsMS\\Layout`), policy in `src/uilayout_policy.h`.
- Config pattern: `GetPrivateProfileStringA("clienthax", …)` (e.g. `src/invresize.cpp:550`).

### Hook points
1. **`CUIStatusBar::ChatLogAdd` 0x008DB070** (thiscall; `ret 0x18`). Detour signature:
   `void __fastcall ChatLogAdd_hook(void* self, void*, const char* text, long type, long channel, int flag, ZRefRaw item)`
   with `struct ZRefRaw { void* pad; void* p; }` passed by value (8 bytes on the stack, as `[ebp+0x18]/[ebp+0x1C]`).
   - **Drop a line:** call the original with `text = ""`. The split loop sees an empty string (0x8DB131/0x8DB139 →
     0x8DB37E), adds nothing and still runs the callee's `~ZRef` (0x8DB444). Never just return: the callee owns the
     ZRef and would leak it (or release it ourselves via 0x00428A50). **UNVERIFIED in game**: the empty path still
     calls `ChatLogDraw`, which should be harmless.
   - **Mirror:** before calling the original, push `{tick, type, channel, text→UTF-8}` into a ring buffer (ANSI →
     UTF-16 via `MultiByteToWideChar(CP_ACP)` → UTF-8 for ImGui).
   - Early exit inside the original (0x8DB0A1-0x8DB0D2: `[0xBEC1F4]+0xC` and `sub_631DE9`) is **not identified**. It
     may be a "chat disabled"/mute check. Mirror before calling, but mark mirrored lines only after the original
     runs, or ignore it.
2. **`CUIStatusBar::OnCreate` 0x008D01B2** (optional, stock window colours): after the original, replace fonts
   `[this+0xBE0+type*4]` with user colours (same face 0x1597, size 12). Low priority. The module draws its own colours.
3. **`CUIStatusBar::ChatLogDraw` 0x008DBB93** (optional): return early to hide the stock log when the module
   replaces it. **UNVERIFIED**: the chat log layer then keeps its last image. Clearing the layer's canvas may be
   needed.
4. Player blocklist on speech balloons (phase 2): `CUser::OnChat` 0x00937138 also makes the balloon over the
   speaker. Blocking only the log line leaves the balloon. Getting the speaker name needs the `CUser` name offset
   (**UNVERIFIED**, not looked up).
5. Click a whisper line to reply (nice-to-have): `CUIStatusBar::SetWhisperTarget` 0x008DB01C.

### Design
- `chatfilter_policy.h` (pure, unit-tested): `enum class ChatTab {All, Party, Guild, Whisper, System}`;
  `bool TabShows(ChatTab, int type)`; `bool IsSystemType(int)`; `ParseSpeaker(type, text)` → name for types 0/11
  (`"Name : msg"`), 1 (`"Name>> msg"`, `"Name[Ch]>> msg"`, outgoing `"Name<< msg"`), 2-6 (`"Name : msg"`,
  **UNVERIFIED** group format string not decoded); `bool KeywordBlocked(text, list)` (case-insensitive, ASCII fold);
  `Verdict Decide(type, text, rules)` → Keep/Drop. Never drop your own outgoing whisper echo or types 12 errors
  unless "hide tips" is on.
- `chatlog.cpp`: the hook, a ring buffer (500 lines, mutex-free, game thread only), the config load, and the
  registry store.
- ImGui module "Chat" in overlay.cpp's module list: tab bar, scroll region, per-type colours (defaults = table
  above), opacity and text size like the HP/MP modules, Edit UI drag/resize. Input stays the stock chat input.
- Persistence per PC: `HKCU\Software\SlumsMS\Chat`: `BlockedPlayers` (REG_MULTI_SZ), `BlockedWords` (REG_MULTI_SZ),
  `HideSystem` / `HideTips` (DWORD), `ColorN` (DWORD ARGB per type), `Tab` (DWORD). Config switch
  `config.ini [clienthax] chatmodule=true`, `chatfilter=true`.
- CMake: `option(MAPLEROOT_CHAT_MODULE "Chat tabs, colours and blocklists (overlay module + ChatLogAdd filter)" OFF)`,
  then `src/CMakeLists.txt`: `target_sources(injector PRIVATE chatlog.cpp)` + define. The module part needs
  `MAPLEROOT_INTEGRATION` (ImGui host). Add it to `scripts/build.ps1` once accepted.

### Risks
- ZRef ownership on the drop path (above). Get it wrong and every dropped item-megaphone line leaks or double-frees.
- Text encoding: lines are ANSI (CP_ACP on the player's PC). Korean/accented names need the conversion.
- Format parsing for player blocks depends on the format strings; GM-coloured or megaphone lines (`"Name : msg"` with
  channel/ear prefixes) need their own patterns. Unparsed lines are never dropped by the player rule.
- The early-exit gate inside ChatLogAdd is unidentified (see 1.).
- Hiding the stock log and keeping the stock input: the moved chat input (hud_native) must keep working.
- 64-line stock cap: our buffer is separate, so tabs keep more history than stock.

### Scaffold plan
- `src/chatfilter_policy.h`: header-only rules (above). `tests/chatfilter_policy_test.cpp`: tab mapping per type,
  speaker parsing for `"%s : %s"`, `"%s>> %s"`, `"%s[%s]>> %s"`, `"%s<< %s"`, keyword matching, the
  never-drop-own-whisper rule. Add it to `tests/CMakeLists.txt` like `uilayout_policy_test`.
- `tests/chatlog_patch_test.cpp` (optional, like `mobdamage_patch_test`): reads the exe copy and checks the
  prologue bytes of 0x8DB070, 0x4906B5, 0x8D01B2 and 0x8DBB93.
- `src/chatlog.cpp`: `AttachChatLogMod()` (byte anchors via memcmp as movementkeys.cpp:189-195 does, then
  `ATTACH_HOOK`), `ChatLog_Lines()` accessor, registry load/save.
- `src/overlay.cpp`: `DrawChatModule()` plus a settings section (tabs, colours, block lists, toggles).
- `src/hook.h`: declaration + call under `#ifdef MAPLEROOT_CHAT_MODULE`.
- README + `config.example.ini`: new keys.

### Test plan (TEST, copy of the RC client under SlumsMS-dev)
1. Two characters (or a QA char + SoloMapling bot) in one party and one guild. Normal chat, `/p`-style party chat,
   guild chat, whisper both ways, a buddy message, a megaphone if available.
2. Each tab shows only its types. All shows everything in order.
3. Block a player: their map chat and whispers disappear from the log and the module (balloon still shows in
   phase 1, documented). Unblock: new lines show again.
4. Keyword block: lines with the word drop, others stay. Case-insensitive.
5. Hide system/tips: EXP gain (type 7) and error lines (type 12) vanish. Toggling back shows new lines.
6. Restart the client: blocklists, colours and tab survive (registry).
7. Regression: the stock chat input still sends, the whisper target selection works, the moved chat input row
   (Edit UI) still works, the item megaphone with an item link shows and its tooltip works (ZRef path).
8. `uistate=1` JSON should gain the module rect so client-mcp can screenshot-check tabs.

### Effort
Filter hook + policy + tests: 1-1.5 days. ImGui module with tabs/colours/settings: 2-3 days. Stock-window recolour
and hide: 0.5 day. Balloon blocking: +0.5-1 day (needs the CUser name offset). Total M (about 4-6 days).

---

## 124: WASD, mouse buttons and controller

### Summary
The game reads the keyboard and mouse through **DirectInput 8**, not window messages. `CWvsApp::Run`, which this
repo re-implements in `bypass.cpp:300` (attached when `MAPLEROOT_INTEGRATION`, the shipping build), waits on three
DirectInput events. Index 0 calls `UpdateKeyboard`, index 1 calls `UpdateMouse`. Those functions diff device state
and queue ISMSGs (WM_KEYDOWN 0x100 with a VK, WM_MOUSEMOVE 0x200, buttons 0x201-0x206, wheel 0x20A), which
`ISMsgProc` 0x9F97B7 dispatches. Key slots are DirectInput **scan codes** in the key map at 0x00BED5A0
(`+4 + scan*5`). Movement keys already rewrite the raw 256-byte keyboard snapshot in `UpdateKeyboard` (jmp at
0x0059B00A). The same place is the cleanest injection point for "virtual keys" from mouse side buttons and a
controller: OR `0x80` into the scan code of the bound key. The game then makes WM_KEYDOWN/KEYUP, repeat, and
`IsKeyPressed` state by itself.

### Facts from the exe
- `UpdateKeyboard` 0x59AF77: copies the old VK state (`+0x824`, 256 bytes), `GetDeviceState(256, [ebp-0x110])`,
  maps scan to VK through `[this+0x428+scan*4]` into `+0x824`, then diffs and calls `AddISMessage`
  (`[slot]=0x100, [slot+4]=vk, [slot+8]=flags`). Only runs when the keyboard event fires (Run loop
  `MsgWaitForMultipleObjects` index 0) or from `TryAcquireDevice` (with bGenMsg=0).
- Keyboard event handle: `Init` stores the three `CreateEventA(0,0,0,0)` handles at `input+0x14/0x18/0x1C` and in
  `m_ahInput` (0x599EE6-0x599F30). Auto-reset events. So `SetEvent(*(HANDLE*)(input+0x14))` forces one
  `UpdateKeyboard` pass. **UNVERIFIED**: DirectInput may also reset or re-signal it, but a spurious pass is harmless.
- `UpdateMouse` 0x59AAFE: `GetDeviceState(0x14, [ebp-0x34])`. 0x14 = `sizeof(DIMOUSESTATE2)`, so the device has
  8 buttons. Buttons 0/1 (swapped by `GetSystemMetrics(SM_SWAPBUTTON)` stored at `+0x974`) and 2 (only as an MK flag)
  are used. **Buttons 3 and 4 (XBUTTON1/2, at `[ebp-0x25]`/`[ebp-0x24]`) are read and ignored.** The cursor is
  the engine's own: `+0x938/+0x93C` move by `lX/lY * (sens+5)/10` and are clamped (MapleRoot patches the clamps at
  0x59AC09/0x59AC22, `mapleroot/ijl15/YourStuff.h:57-58, 2084-2085`). Left and right button state is kept at
  `+0x940/+0x944`. `IsKeyPressed(VK_LBUTTON)` returns `+0x940` (0x59A282).
- `GetCursorPos` 0x59A388 returns `+0x938/+0x93C` when the DI mouse exists (`+0xC != 0`). The stock `SetCursorPos`
  0x59A887 writes `+0x938/+0x93C` in DI mode. **The repo's replacement (`resolution.cpp:229`) does not write
  `+0x938/+0x93C`**. It only moves the vector and the OS cursor. Keep that in mind for an in-process cursor
  driver.
- Window messages still arrive: `system.cpp:50` WndProc_hook already handles client WM_MOUSEMOVE/LBUTTONUP and passes
  mouse messages to ImGui (`d3d9host.cpp:289-292`), which works in game. So WM_XBUTTONDOWN/UP reach the WndProc
  (**UNVERIFIED in game**, but the same path).

### Existing code
- `src/movementkeys.cpp:15-20` constants. `:69` `IsKeyPressed_hook` (arrow fallback via GetAsyncKeyState). `:101`
  `RemapKeyboardSnapshot`. `:109` naked `KeyboardSnapshot_hook` (pushad, passes `ebp-0x110`, re-executes
  `test byte [ebp-0xD6],0x80`, jumps to 0x59B011). `:145` `ApplyMovementKeyBindings(keys, bindings)` turns
  bound movement scans into arrow scans (0xC8/0xD0/0xCB/0xCD). `:56` `TextEditFocused()` (chat/edit focus via
  CWndMan+0x88 vtable check). `:189-203` byte anchors. Built by `MAPLEROOT_MOVEMENT_KEYS` (`CMakeLists.txt:4`,
  `src/CMakeLists.txt:47-49`), on in `scripts/build.ps1`.
- `src/system.cpp:50-113` WndProc_hook (subclassed in `CreateWindowExA_hook:125`).
- `src/bypass.cpp:300-372` `CWvsApp::Run_hook`: the input/message loop. The idle branch (CallUpdate, render) is a
  per-frame game-thread spot.
- `src/resolution.cpp:213-235` CInputSystem SetCursorPos/SetCursorVectorPos hooks.
- Cursor readers: `src/invresize.cpp:109-118`, `src/coloringprism.cpp:75-150`, `src/cashshopwnd.cpp:132`.

### Hook points
1. **Snapshot injection (keys from any source)**: extend `RemapKeyboardSnapshot` (movementkeys.cpp:101), no new
   patch. Order: `VirtualKeys_Apply(keys)` (OR held virtual scans), **then** `ApplyMovementKeyBindings`, so a
   controller mapped to a WASD-bound key also moves. Keep the `kKeyConfigOpen` / `TextEditFocused()` guards, but
   let Enter/Esc through while a text box is focused. If movement keys are compiled out, the virtual-key module
   needs the same 0x59B00A jmp. Move the naked stub to a shared `inputinject.cpp` that both use.
2. **Mouse side buttons**: in `WndProc_hook` handle `WM_XBUTTONDOWN/UP` (`GET_XBUTTON_WPARAM`), set/clear the
   bound scan in the shared virtual-key set, `SetEvent(input+0x14)`, return TRUE. Alternative (no WndProc
   dependency): code cave after `GetDeviceState` in UpdateMouse at 0x59AB45 (`ff 15 0c 06 bf 00` = `call
   [timeGetTime]`, 6 bytes) reading `[ebp-0x25]/[ebp-0x24]`. The WndProc route is simpler. Middle button can be
   offered the same way (stock only uses it as MK_MBUTTON).
3. **Controller keys**: an XInput poll (thread at ~125 Hz, or the Run_hook idle branch / EndScene) maps buttons,
   D-pad and the left stick to scan codes in the same shared set. On any change it calls `SetEvent(input+0x14)`.
   Left stick → arrow scans 0xCB/0xCD/0xC8/0xD0 (up = rope/portal, down = crouch/ladder).
4. **Controller cursor**, two options:
   - (a) **SendInput** relative `MOUSEEVENTF_MOVE` plus LEFT/RIGHT DOWN/UP while the game window is foreground.
     DirectInput (non-exclusive) sees injected input as device data, so the engine cursor, clicks, drags and
     `IsKeyPressed(VK_LBUTTON)` all behave like a real mouse. No game addresses. **UNVERIFIED**: DI8 picking up
     SendInput deltas (normally true for non-exclusive mice); the cooperative-level flags were not found in the
     exe.
   - (b) In-process: write `+0x938/+0x93C`, call `SetCursorVectorPos` 0x59A0CB, and queue ISMSGs with
     `AddISMessage` 0x59B382 (`this = input+0x9BC`; fill `{msg, MK flags, MAKELPARAM(x,y)}` as UpdateMouse does),
     and set `+0x940` for drags. Game thread only (Run_hook). More exact but couples to more offsets. Fallback if
     (a) fails.
5. Direct action calls (`CUserLocal::UseFuncKeyMapped` 0x94F2ED, `CWvsContext::UseFuncKeyMapped` 0xA0773D) are
   **not** recommended: they skip hold/repeat and `IsKeyPressed` (charge skills, holding attack, rope climbing).

### Design
- `input_remap_policy.h` (pure, unit-tested):
  - `struct StickCfg { float deadzone=0.24f; float hysteresis=0.06f; }` and
    `Dir4 StickToDirections(int16 x, int16 y, const StickCfg&, Dir4 prev)`: radial dead zone, 8-way (diagonals
    give two arrows), hysteresis so a stick on the edge does not flicker.
  - `CursorStep RightStickToCursor(int16 x, int16 y, float dtMs, const CursorCfg&)`: radial dead zone, exponent
    curve (1.8), max speed px/s scaled by game height, sub-pixel accumulator.
  - Trigger threshold (XInput 30/255).
  - `ParseBinding("A=ctrl,B=alt,X=z,Y=shift,LB=pgup,RB=pgdn,BACK=esc,START=enter,RT=lclick,LT=rclick,X1=f1,X2=f2")`
    → table of `{source, scan | MouseLeft | MouseRight}` with a key-name → DIK scan table (`ctrl=0x1D, alt=0x38,
    shift=0x2A, z=0x2C, a..z, 0..9, f1..f12, ins/home/pgup/del/end/pgdn` with extended DIK codes `0xD2/0xC7/0xC9/
    0xD3/0xCF/0xD1`).
  - `void MergeVirtualKeys(uint8_t keys[256], const uint64_t held[4])`.
- `inputremap.cpp`: XInput loader (`LoadLibraryW(L"xinput1_4.dll")`, fallback `xinput9_1_0.dll`, no import-lib
  dependency), the poll thread (atomic held set, stops on DLL detach), foreground check
  (`GetForegroundWindow()` PID as movementkeys.cpp:75 does), SendInput cursor, WM_XBUTTON handler hook-in from
  system.cpp, `VirtualKeys_Apply` called from the snapshot hook.
- Config (`config.ini`, per PC): `[input] sidebuttons=true`, `x1=f1`, `x2=f2`, `[controller] enable=false`,
  `bindings=…`, `deadzone=24`, `cursorspeed=900`, `invertY=false`. Optional overlay settings later.
- CMake: `option(MAPLEROOT_INPUT_REMAP "Mouse side-button remap and XInput controller mode" OFF)`.
  `src/CMakeLists.txt`: `target_sources(injector PRIVATE inputremap.cpp)` + define. Needs
  `MAPLEROOT_INTEGRATION` for the Run_hook path, or works with the thread + SendInput alone.

### Risks
- Two patches at 0x59B00A: share one stub between movement keys and remap, or the second jmp overwrites the first.
- Held keys while the window loses focus: clear the virtual set on WM_ACTIVATE(inactive) / focus loss, or the
  game may keep "pressing" (UpdateKeyboard fails while unacquired, then TryAcquire resyncs).
- Chat focus: controller A=Ctrl would be harmless in a text box, but letter keys would type. Keep the
  `TextEditFocused()` guard.
- SendInput moves the real Windows cursor too. Only send while foreground and the cursor is inside the client
  rect, and stop at the window edge (MapleRoot's clamp).
- KeyConfig window open (`kKeyConfigOpen`): injection off, as movement keys do.
- Anti-cheat: none in this client (HackShield bypassed). Server-side, injected keys look like normal keys.
- XInput only covers Xbox-style pads. DualShock/Switch pads need Steam Input or DS4Windows (document it).

### Scaffold plan
- `src/input_remap_policy.h` and `tests/input_remap_policy_test.cpp`: dead zone/hysteresis edges, diagonals,
  cursor curve monotonic and zero inside the dead zone, binding parser (bad names rejected, extended scans),
  merge sets 0x80 without clearing real keys.
- `src/inputinject.cpp/.h`: shared snapshot stub at 0x59B00A (move the movementkeys naked stub here, keep its byte
  anchor check), `InputInject_SetHeld(scan, bool)`, `InputInject_Kick()` (`SetEvent(input+0x14)`).
- `src/inputremap.cpp`: XInput thread, side buttons, cursor.
- `src/system.cpp`: `case WM_XBUTTONDOWN/WM_XBUTTONUP` → `InputRemap_OnXButton(...)` under the define.
- `tests/inputremap_patch_test.cpp` (optional): exe bytes at 0x59B00A, 0x59AF77, 0x59AAFE, 0x59A887, 0x59B382.
- README + `config.example.ini`.

### Test plan (TEST)
1. Side buttons: bind X1 to a skill key (e.g. F1 with a buff) and X2 to a potion slot. Press: the skill casts once.
   Hold: repeats like a held key. No effect while typing in chat or with KeyConfig open.
2. Controller, with `[controller] enable=true` and a wired Xbox pad: left stick walks left/right, up climbs a rope
   and enters a portal, down goes down a ladder. A attacks (hold = continuous), B jumps, X loots (Z), right stick
   moves the cursor, RT clicks, so you can open the menu, drag an item, and double-click a potion in the inventory.
   Mapped keys open windows (I/E/K/S).
3. Movement-keys interplay: with WASD bound, a pad bound to "w" also moves (merge order).
4. Alt-tab with the stick held: the character stops; back in, no stuck key.
5. Unplug/replug the pad: no crash, it resumes (XInputGetState returns ERROR_DEVICE_NOT_CONNECTED).
6. Regression: keyboard and mouse unchanged with the feature off and on; ImGui overlay mouse still works.

### Effort
Side buttons (shared stub + WndProc + config + tests): 0.5-1 day. Controller keys + stick policy: 1-1.5 days.
Cursor via SendInput: 0.5 day (+1 day if in-process option (b) is needed). Total S-M (about 2.5-4 days).

---

## Open items (not verified)
- Exact meaning of chat types 9, 13-22 (only some callers traced) and the format string of group messages.
- ChatLogAdd's early-exit check (`[0xBEC1F4]+0xC`, `sub_631DE9`, `sub_8ADF76`).
- CChatLog field layout (`+0x18..+0x28` stores seen at 0x8DB4D8-0x8DB4F0; type/colour order not confirmed).
- DirectInput cooperative level (not located; window messages for the mouse arrive, so non-exclusive is likely).
- DirectInput seeing SendInput relative moves (normal for DI8 non-exclusive; test on TEST).
- `CUser` name offset for balloon blocking.
