# Research: widget mode and modern HUD modules (1.7.0)

Date: 2026-10-28. Read-only. Nothing was edited, built, run or committed.

Sources:
- Client: /root/work/client/src-161-rc2 (branch client-1.6.1-rc2).
- Docs: docs/160-ui-adjust-plan.md, docs/hud-skin-mockup-owner.png, docs/160-ui-split-reference.png.
- The existing 1.7.0 RmlUi plan: /root/work/client/src-170-rmlui-plan/docs/170-rmlui-hud-plan.md (commit 24aabba).
  It already covers the RmlUi backend, the order of work and the mockup's five panels. This note builds on it and
  does not repeat it.
- Exe: scratchpad/client/MapleStory.exe, sha256 86e4e6be...aac90, image base 0x400000.
- Names: /root/Maplestory/Angel.idb (python-idb).

Every address in the tables was disassembled from the exe with `objdump -d -M intel`. The "Entry bytes" are stock
bytes. MapleRoot or our own hooks may already detour some of these at runtime. Anything marked UNVERIFIED was
inferred and not checked in game.

---

## 0. Summary

- **Approach: put the skin over a live stock bar.** In widget mode the stock `CUIStatusBar` stays created, keeps its
  logic and keeps receiving input. Only its pixels are hidden.
  - Our modules draw on top. Where a module's shape differs from the stock piece, the two input hooks we already own
    map clicks to the stock control or slot: `CUIStatusBar::HitTest` (uilayout.cpp) and `GetShortCutIndexByPos`
    (resolution.cpp).
  - Hotkeys never go through the bar. They run CWndMan -> field -> `CUserLocal::UseFuncKeyMapped`, so hiding the
    bar cannot break them.
- **MVP (Phase 1).** Interactive:
  - the menu buttons, which run the stock button handler.

  Display only:
  - HP, MP and EXP (these modules already exist);
  - a level/job/name card;
  - quick slots with icons, key labels, counts and cooldowns;
  - buffs, optional, because the stock buff icons are not part of the bar.

  Chat stays stock and movable until ticket 123's ImGui chat module lands.
- **Renderer: ImGui first.** Build the mockup look in ImGui with a small skin kit: textured 9-slice frames, gradient
  bars and a bundled rounded font. Keep a data/action layer (`HudModel`) that knows nothing about the renderer, so
  RmlUi (the existing plan) can replace the views later without redoing the hooks. Whether to switch to RmlUi is the
  owner's call, after seeing one ImGui-skinned card.
- **Widget mode is what makes UI scale and magnets work.** Stock Gr2D art cannot be scaled cheaply. Once everything
  on screen is our own drawing, scale, snapping and linking apply to all of it.

---

## 1. Widget mode: hide the stock bar and keep it alive

### What the bar is (logged on TEST in 1.6.0, docs/160-ui-adjust-plan.md)

| Piece | Where | Visible part |
|---|---|---|
| Window layer `m_pLayer` (CWnd +0x18), 1920x578 | the bar's own canvas | level digits, job, name, HP/MP/EXP numbers, **chat log**, **chat input text**, and the child buttons (controls paint into this canvas) |
| +0xCD0 (800x578) | plate art | level plate, gauge plate, chat row |
| +0xCCC | light background strip | |
| +0xCD4 (340x31) | gauge strip | |
| +0xBB8 / +0xBBC / +0xBC0 / +0xBC4 | gauge covers | |
| +0xCB8 (434x14) | chat input line | |
| +0xCC4 (+0xCC8, +0xD80) (1008x80) | quick slot layers | |
| Child controls | Shop 1000, Trade 1001, Menu 1002, Shortcut 1007; icon row 2000-2003, 1005, 1006, 1009; chat edit 1011, type selector 1012, small buttons 1004 | |

hud_native.cpp already hides +0xCD4 and the covers every tick (`SetStockPiecesVisible`) and clears the plate and text
areas (`ClearStockArt`). Widget mode extends that approach.

### Recommended mechanism (option A: hide the layers, mask the hit test)

1. **Every tick in `UiLayout_Tick`** (from CWvsApp::CallUpdate, on the game thread):
   - Set `visible = 0` on +0xCCC, +0xCD0, +0xCD4, the covers and the quick slot layers (+0xCC4, +0xCC8, +0xD80).
   - Re-assert it every frame. The client re-shows layers itself: `ToggleQuickSlot` 0x008DF4B1, `ReDrawQuickslot`
     0x008D82A7, and a new `CUIStatusBar` after a field change or a resolution change.
   - Keep the pattern hud_native uses: compare the bar pointer, and restore everything inside `catch`.
2. **Window layer +0x18.** It carries the chat log and the chat input. Two steps:
   - **W1 (MVP):** keep it visible. Clear its non-chat areas the way `ClearStockArt` already does. Also clear the
     button areas, because buttons repaint into this canvas: hook `CUIStatusBar::Draw` 0x008D80A4 and clear after the
     original, or clear in the tick as today. Chat log and chat input stay stock and keep their 1.6.0 mover.
   - **W2:** once ticket 123's ImGui chat module can show the log and an input line, also set the +0x18 layer
     `visible = 0`. The stock edit control 1011 can stay the real input, invisible, with our module mirroring its
     text. Typing then needs no new IME work. UNVERIFIED: how to read the edit control's text and caret; look at
     CCtrlEdit when 123 is done.
3. **Mask the hit test.** An invisible bar still owns its 1920x578 window rectangle. Without a mask it would swallow
   map clicks along the bottom of the screen.
   - Extend `BarHitTest_hook` (uilayout.cpp; `CUIStatusBar::HitTest` 0x008DE7DA, stock bytes
     `55 8b ec a1 0c c2 be 00`): in widget mode return 0 (the click goes to the map) unless the point is on a
     control or slot that a module maps.
   - A module "maps" a control when it draws that control's skin and wants its click. Return `(2, ctrl)` as the
     1.6.0 moved groups already do.
   - UNVERIFIED: whether CWndMan's window pick checks layer visibility. The hook makes this irrelevant.
4. **Never destroy or detach the bar or its children.** Much stock code calls the singleton 0x00BEC208:
   `CHATLOG_ADD` (392 callers), `SetStatusValue`, `SetButtonBlink` 0x008DFE21, `ReDrawQuickslot`, and quick slot drag
   and drop. `CWnd::RemoveChild` 0x009E06F6 exists. Do not use it.

**Option B (fallback / kill switch):** one extra `ms_pOrgStatusBar->RelMove(0, +screenH)` in `ResetOrgWindow`
(wvs/wndman.h). Everything on the bar goes off screen in one move, chat included, and nothing can be clicked. It is
cheap, but it takes chat with it and depends on nothing reading absolute positions. Keep it as the "if A misbehaves"
fallback.

### Input that must stay alive

| Need | How it works today / in widget mode |
|---|---|
| Hotkeys (skills, items, menus on keys) | `CUserLocal::UseFuncKeyMapped` 0x0094F2ED / `CWvsContext::UseFuncKeyMapped` 0x00A0773D take the WM_KEYDOWN lParam (scan code in bits 16-23, bit 31 = up) and read `CFuncKeyMappedMan`. The bar is not involved. Unaffected. |
| Enter to chat, chat keys | `CUIStatusBar::OnKey` 0x008D536C. The bar window still exists and keeps focus handling. Unaffected. |
| Menu buttons | Our module calls the stock handler (section 4). |
| Drag item/skill onto a quick slot, drag off to clear | Stock bar drag-drop + `GetShortCutIndexByPos` (hooked in resolution.cpp:494). In widget mode the HitTest mask claims points over our quick slot module, and the GetShortCutIndexByPos hook maps our module geometry to the slot index (section 2). |
| Stock tooltips on slots and buttons | Free while the stock slot or control sits under the cursor (`CUIStatusBar::ProcessToolTip` 0x008D76D7). If our geometry differs, map the cursor the same way. UNVERIFIED for tooltips. |
| Quest/notification blink | `CUIStatusBar::SetButtonBlink(id, on)` 0x008DFE21. Hook it, or read the control state, to show the mockup's red badge. |

### Showing modules instead

The modules are rows of `g_widgets` (overlay.cpp:59-68). Needed changes:
- **A per-module `interactive` flag.** Today a frozen HUD is `NoInputs` (overlay.cpp ~264). Interactive modules
  (buttons, later quick slots) stay `NoMove|NoResize` when frozen but take input. `D3D9Host_HandleMessage` already
  eats mouse messages only when `WantCaptureMouse` is set, so clicks on the map still pass.
  - Turn `io.MouseDrawCursor` off for these modules, so the game cursor stays.
  - Hovering a module must not stop map clicks next to it. Only real items should capture.
- **A `defOn` field.** The 118 note already proposed it. New modules start off unless widget mode is on.
- **Widget mode switch.** The HUD window (Ctrl+F10) gets a checkbox, plus `[clienthax] widgetmode` (default off).
  Turning it on enables the replacement modules. Turning it off restores every stock layer.
- **Draw order.** ImGui draws in EndScene after the game, so modules cover stock windows (inventory, stats). Fix:
  each frame, walk `CWndMan::ms_lpWindow` (0x00BF1648, ZList<CWnd*>; resolution.cpp:359 already walks it). Take each
  window's rectangle from its layer position minus `m_pOrgWindow`, as `BarWindow()` does. Then fade or hide any
  module that overlaps an open window. **The behaviour is the owner's decision** (section 7).

---

## 2. Quick slots module

### Where the data is

| What | Address / layout | Notes |
|---|---|---|
| `CQuickslotKeyMappedMan` singleton pointer | 0x00BED658 (the ctor at 0x0072B7BC writes `this` there) | Stock: 8 scan codes (int) at +4, a saved copy at +0x24. **MapleRoot expands this to 26 slots** (mapleroot/ijl15/YourStuff.h:2682-2750, moreshit.h): 26 ints at +4 and the copy at +0x6C; defaults are `Array_aDefaultQKM_0`. OnInit 0x0072B82A, Save 0x0072B87E, Default 0x0072B8E6. Slot i -> scan code = `*(int*)(qkm + 4 + 4*i)`, i < 26. UNVERIFIED in game for i >= 8 (but MapleRoot itself reads it this way). |
| `CFuncKeyMappedMan` singleton pointer | 0x00BED5A0 | `FUNCKEY_MAPPED { u8 nType; i32 nID; }` at `+4 + scan*5` (unaligned, so use memcpy), 89 entries. Right Shift 0x36 is aliased to 0x2A. Already read by cashshopwnd.cpp:1425-1460 and coloringprism.cpp:323. |
| Per-slot cache the stock quick slot draws from | `Array_Expanded[312]` in MapleRoot (26 x 12 bytes, `{u8 type; i32 id; pad}`). It replaces the bar's own +0xD20 array. Filled by `CompareValidateFuncKeyMappedInfo` (cave at 0x008DD8B8). | It is compiled into slums.dll, so we can reference the symbol directly. Use it as a cross-check, not as the source of truth. |
| Slot positions (stock art) | `Array_ptShortKeyPos` (MapleRoot; stock table 0x00BE2DB0): 2 rows x 13, pitch 35 px, rows at y 8 and 41. `GetPosByIndex` 0x008DE93D. | Our module can use its own layout and map it in `GetShortCutIndexByPos_hook`. |
| Types | 1 skill, 2 item, 4 menu (UI toggle id), 5 basic action (52 attack, 53 jump), 6 face, 7 pet potion, 8 macro (type 8 needs no id; see 0x0094F38C) | 1/2/4/5/8 are seen in the code above. The rest follow the server's (Cosmic) enum. UNVERIFIED in the client. |
| Item count | `CWvsContext::GetItemCount(long itemId)` 0x00A2858C, `__thiscall`, ecx = *(0x00BE7918) | Entry `55 8b ec 83 ec 18 8b 0d 18 79`. |
| Skill learned / level | `CSkillInfo::GetSkillLevel(CharacterData&, skillId, SKILLENTRY**)` 0x007616F6; singleton 0x00BE78DC | The stock cooldown draw calls it (0x008E079F). |
| Cooldown end time | `CWvsContext +0x3714` = `ZMap<long,long,long>` (skillId -> tEnd). `ZMap::GetAt(const long& key, long& out)` 0x004E86A0 (`56 8b f1 83 7e 04 00`), returns nonzero if found. | tEnd = `timeGetTime() + seconds*1000` (0x0095BEAB-0x0095BEB9). Remaining = tEnd - timeGetTime(), exactly as the stock `DrawSkillCooltime` 0x008E0634 does (0x008E07E9-0x008E081E). |
| Cooldown start (for a sweep fraction) | Hook `CWvsContext::SetSkillCooltimeOver(skillId, tEnd)` 0x00A123A4 (`b8 98 b0 ae 00 e8 ...`, `ret 8`) and `RemoveSkillCooltimeOver(skillId)` 0x00A1246C (`81 7c 24 04 6f ae 4f 00`, `ret 4`). Record `{skillId, start=now, end}`. | Called from `CUserLocal::OnSkillCooltimeSet` 0x0095BE66 (packet). Skill 5221006 (Battleship) is special: its value is durability, not a time. tempstat.cpp already special-cases it. |

### Icons in ImGui

- **Skill icon:** `Skill/<job>.img/skill/<id>/icon`, through `get_rm()->GetObjectA` the way hud_native.cpp's
  `Sprite()` does. `iconDisabled` shows when the skill cannot be used. The job is `id / 10000`, printed as `%03d`
  (e.g. 411.img), or as is for 4-digit jobs (1311.img). MapleRoot custom skills use the same path.
- **Item icon:** `CItemInfo::GetItemIcon(long id, int, int)` 0x005D3BD8 (singleton 0x00BE78D8). UNVERIFIED: the
  meaning of the two flags. Or walk the item property (`CItemInfo` 0x005DA83C wrapper in wvs/iteminfo.h) to
  `info/icon`.
- **Buff icon:** the `TEMPORARY_STAT::pLayer` canvas (section 3).
- **Canvas to texture:**
  - `IWzCanvas::Getpixel(x, y)` returns ARGB (uilayout.cpp `DrawnWidth` already uses it). A 32x32 icon is 1024
    calls, done once per icon.
  - Fill an `IDirect3DDevice9::CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED)`. Managed textures
    survive `Reset`, so the existing Reset hook needs nothing new. Pass it as `ImTextureID`.
  - Cache by (kind, id) and evict by LRU (about 256 entries).
  - **Do not use `_LockAddress`.** weathercanvas.cpp records that the raw-pixel path broke the client twice and is
    kill-switched off.
- **Overlays, in ImGui on top of the icon:**
  - cooldown: `AddConvexPolyFilled` sweep plus seconds text (the stock look uses `UI/UIWindowEx.img/SkillCooldownNumber`,
    tempstat.cpp);
  - item count, bottom-right (as in the mockup);
  - key label, top-left (scan code -> name table; the 26 MapleRoot defaults are in `Array_aDefaultQKM_0`);
  - greyed when MP is short or the item count is 0.

### Clicks and drag-and-drop

- **Display-only first (Phase 1).** The module is click-through. The invisible stock quick slot stays under it at
  stock geometry, so drag-and-drop and tooltips keep working with no new code. This works only while our module
  uses the stock 2x13 layout at 100% scale.
- **Phase 2, own layout or scale:** map in `GetShortCutIndexByPos_hook` (point -> our slot rect -> index), and claim
  those points in `BarHitTest_hook`. Drops then reach the stock `CDraggableItem::MapFuncKey` 0x004F38F4 /
  `CDraggableMenu::MapFuncKey` 0x004F9386, which send CHANGE_KEYMAP. No new packet.
- **Phase 2, click to use (new behaviour; stock v83 does not use a slot on click, UNVERIFIED):**
  - Queue `{scan}` from ImGui.
  - In `UiLayout_Tick` (the game thread), call `CUserLocal::UseFuncKeyMapped(this=*(0x00BEBF98), lParam=scan<<16)`
    and then the key-up `(scan<<16)|0x80000000`.
  - This is the exact code path a key press takes, so every check (MP, cooldown, stun, map rules) stays stock.
  - Keydown/charge skills (Hurricane, Big Bang...) need a held press. Leave those key-only.
- **Mockup paging (the "2" with up/down arrows):** read it as display pages over MapleRoot's 26 slots. Real extra
  binding sets would need server keymap work. **Owner decision.**

---

## 3. Buffs and cooldowns module

- **Active buffs:** `CTemporaryStatView` embedded at `CWvsContext +0x2EA8` (`lea ecx,[edi+0x2ea8]` at 0x00A12425).
  - Its list is `m_lTemporaryStat` at +4 (`ZList<ZRef<TEMPORARY_STAT>>`, wvs/tempstat.h).
  - Per entry: `nType` +0x1C, `nID` +0x20, `pLayer` +0x28 (the icon), `pLayerShadow` +0x2C, `tLeft` +0x38 (ms),
    `tLeftUnit` +0x3C (1/16 of the duration, used for the shadow index).
  - Total duration ~ `tLeftUnit*16`. UNVERIFIED: check against a known buff.
  - Functions: `SetTemporary` 0x007B24D5, `ResetTemporary` 0x007B2679/0x007B2717, `Update` 0x007B2829 (hooked by
    tempstat.cpp), `UpdatePassively` 0x007B30DB, `AdjustPosition` 0x007B2BB0 / `FindIcon` 0x007B3055 /
    `ShowToolTip` 0x007B2E58 (hooked in resolution.cpp).
  - Packets: `CWvsContext::OnTemporaryStatSet` 0x00A202BE / `OnTemporaryStatReset` 0x00A2071F.
  - Read the list once per tick on the game thread into the snapshot (section 6). Read pixels from
    `pLayer->Getcanvas(0)` once per (nType, nID).
- **The stock buff icons are not on the status bar.** Widget mode does not have to hide them. A buff module is a
  restyle (bigger icons, timers as text, a bar layout) and can come after the MVP. Options:
  - keep the stock icons and move them through the existing `AdjustPosition` hook; or
  - hide their layers (`pLayer`/`pLayerShadow` `visible = 0` per tick) and draw ours.
  - Right-click-to-cancel: the stock handler is reached through FindIcon. Keep the stock layers under the module, or
    find the cancel request (UNVERIFIED). Display-only first.
- **Cooldowns:** the hook pair in section 2 (`SetSkillCooltimeOver` / `RemoveSkillCooltimeOver`) gives a complete
  list of skills on cooldown, with start and end times. A "Cooldowns" module lists them with icons, which also covers
  skills that are not on a quick slot. Display-only.

---

## 4. Menu buttons module

`CUIStatusBar::OnButtonClicked(unsigned id)` 0x008D423D (`56 57 8b f1 8b 0d 98 bf be 00`, `__thiscall`, `ret 4`).
It first calls `CUserLocal::OnKeyDownSkillEnd` 0x0095BEDF if a keydown skill is running, then switches on the id.
The jump table is at 0x008D4320:

| Id | Stock button | Calls (ecx = CWvsContext 0x00BE7918) |
|---|---|---|
| 1000 | Shop (Cash Shop) | `SendMigrateToShopRequest(0)` 0x00A04DCA |
| 1001 | Trade (MTS) | sub_00A12522 (unnamed; reads CWvsContext +0x2090 and shows a notice; MTS migrate, UNVERIFIED) |
| 1002 | Menu | `UI_Menu()` 0x00A06735 |
| 1007 | Shortcut | sub_00A06CBC (unnamed, UNVERIFIED as UI_ShortCut; opens a popup) |
| 1003 / 1004 | chat type buttons | `SetChatType(1)` / `SetChatType(3)` 0x008D4A6D |
| 1005 | icon row | `UI_Toggle(5, -1)` 0x00A04F7A (key config, inferred) |
| 1006 | icon row | `ToggleQuickSlot()` 0x008DF4B1 |
| 1009 | icon row | `SendClaim()` 0x008D4A22 |
| 2000-2003 | icon row | `UI_Toggle(1/0/2/3, -1)` plus `SetButtonBlink(n, 0)` (equip/item/stat/skill, inferred from the func-key menu table at 0x00A07998) |

Recommendation:
- Our buttons queue the id and call `OnButtonClicked(bar, id)` from `UiLayout_Tick`. That gives the identical stock
  behaviour, including blink reset and keydown-skill end.
- For windows the bar has no button for (quest, party, buddy, world map...), call
  `CWvsContext::UI_Toggle(type, -1)` 0x00A04F7A (`b8 0a 96 ae 00 ...`), or `UI_Open` 0x00A05AB8 / `UI_Close`
  0x00A0631B. Get the type numbers from the func-key menu jump table at 0x00A07998: log each entry once on TEST
  before relying on it.
- **Known:** the game menu and the shortcut popup open at their stock place (1.6.0 note). Opening them next to our
  button needs a position hook in their OnCreate (address not found yet).

---

## 5. Look and skin: ImGui vs RmlUi

What the mockup needs, from left to right:
- **Chat panel:** pill tabs, close button, coloured lines, scroll bar, input with chat-type icons and an emoji
  button.
- **Character card:** portrait, gold "LV. 200" badge, job and name.
- **Vitals card:** rounded gradient bars with glossy fill and large numbers.
- **Quick slot card:** 2 rows, key labels, counts, empty "+" slots, page arrows.
- **Menu card:** 4 large blue gradient tiles with icons and labels, and 5 small icon tiles, one with a red badge.
- **Everywhere:** cream panels with soft borders and shadow.

Assets:
- **Font:** a rounded sans with bold for numbers and labels, e.g. Nunito or M PLUS Rounded (OFL). About 0.3-0.6 MB.
  FreeType is already linked (imgui_freetype).
- **Textures:**
  - a 9-slice panel frame;
  - a slot frame;
  - large and small button tiles (normal, hover, pressed);
  - about 20 icons at 2x (64 px): coin, person, book, gear, bag, people, clipboard, star, menu, chat bubbles, emoji,
    plus, arrows, close.

  A 512x512 atlas covers all of it. Source: CC0 (Kenney UI packs) or commissioned art. The mockup itself is a
  generated image, not usable art.
- **Procedural:** gauge gradients and gloss, the level badge, tabs, and the count/notification badges.
- **Portrait:** an avatar render into a canvas (coloringprism.cpp already drives a live avatar; the character-info
  window has a picture path). Medium effort. Ship the card without it first.

| | ImGui skin kit | RmlUi (existing plan) |
|---|---|---|
| Hosting | Already on the device (EndScene, Reset, input, font rebake) | A new D3D9 renderer, 400-600 lines (no upstream DX9), new state save/restore on the game's device |
| Look | Rounded rects, `AddRectFilledMultiColor` gradients, `AddImage` 9-slice and icons, FreeType text. Matches the mockup with an art atlas. No CSS, so tuning is code. | CSS-like decorators, flex layout, animations; a skin change is a data change |
| Effort to the first skinned module | ~1-2 days | ~3-4 days (backend + input + one document) |
| Size | +atlas +font (~1 MB) | +1.5-2 MB library + art |
| Z-order over stock windows | Same problem as RmlUi (EndScene) | Same |
| Risk | Low | Device-state leaks, a new input layer, text input (IME) for chat |

**Recommendation.**
1. Build the vitals card and the menu card in ImGui with the skin kit, and show the owner.
2. Keep all game data and actions in `HudModel` (section 6), so the views are thin.
3. If the owner wants data-driven, CSS-editable skins or player themes, do the RmlUi backend as written in the 1.7.0
   plan; the modules port by rewriting only the views.

A third option, for the "stock art" look:
- Native Gr2D layers (hud_native.cpp's method) with skin art in Custom.wz. It is correctly z-ordered under stock
  windows.
- But it does not scale smoothly and text is limited to the game font. Keep it only for the stock-look variant
  already shipped.

---

## 6. Shared infrastructure, magnet linking and zoom

### HudModel (do this first)

- One snapshot struct filled in `UiLayout_Tick`, i.e. CWvsApp::CallUpdate on the game thread. It holds:
  - vitals and the level/job/name read;
  - 26 slots `{scan, type, id, count, skillLevel, cdStart, cdEnd}`;
  - buffs `{type, id, tLeft, total}`;
  - cooldowns;
  - button blink states;
  - the open-window rectangles for occlusion.
- An action queue the other way (button id, use slot, toggle UI) that the tick drains.
- This settles the 118 note's open question ("is EndScene on the game thread?"): we never call client code from
  EndScene. Guard the swap with a cheap lock, and keep SEH around every read, as hud_vitals.cpp does.
- Icon texture cache (section 2) on the render side. It reads canvases on the game thread and uploads during the
  frame, or does the 1024 Getpixel calls in the tick and hands over a pixel buffer.

### Magnet linking (pure policy header + native test, like overlay_policy.h)

- **Snapping.** While dragging, test the moving rectangle's 4 edges and 2 centre lines against every other visible
  module and the screen edges. Snap if within 8 px × UI scale. Draw a guide line. This works together with the
  existing 1.25% grid; Alt disables both.
- **Linking.** Dropping with an edge snapped creates a link stored on the child: `{parentId, parentAnchor (9-point),
  childAnchor, gapPx/scale}`. Dragging a parent moves its linked children (a tree; refuse cycles). Ctrl-drag detaches
  a child. Edit UI shows links as small chain marks.
- **Layout on resolution change.** Lay out the roots from their screen fractions (today's storage), then the children
  in tree order from their anchors. Clamp at the end. A chain then keeps its shape on every resolution.
- **Storage.** Add `<id>_link` values under HKCU\Software\SlumsMS\Overlay. They are global, like widget positions
  today. Per-character storage is a decision (section 7).
- **Presets.** A preset is a named copy of all module values. "Mockup bottom row" ships as the default for widget
  mode.

### Zoom / UI scale

- **Global HUD scale.** It exists (`g_nScale`, font rebake in `Overlay_BeforeFrame`). Add a per-module scale
  (75-200%, steps of 5) next to text size and opacity. Icons and skin art at 2x with linear filtering stay sharp down
  to 50%.
- **Stock Gr2D pieces cannot be scaled cheaply.** That would need a stretched canvas copy per repaint (`CopyEx`, as
  in the 119 note). So scaling only fully works in widget mode. Say so in the HUD window.
- **"Game zoom"** (windowed, sharp scaling, mouse mapping, from the 1.7.0 list) is a separate feature on the
  present/back-buffer path and the input mapping, not part of this module work. Plan it on its own.

---

## 7. Phased plan

| Phase | Content | Interactive? | Effort |
|---|---|---|---|
| **0. Infrastructure** | HudModel snapshot + action queue in `UiLayout_Tick`. Icon texture cache (Getpixel -> managed texture). `interactive` and `defOn` module flags. Occlusion rectangles from `ms_lpWindow`. Policy tests. | none | 1-1.5 d |
| **1. MVP widget mode** | `[clienthax] widgetmode` + HUD-window checkbox. Hide the stock bar per section 1, W1 (chat stays stock and movable). HitTest mask. Modules: HP/MP/EXP (existing), **level/job/name card**, **quick slots display-only** (icons, keys, counts, cooldown sweep; invisible stock slot kept under it at stock geometry for drag-and-drop and tooltips), **menu buttons** (Shop/Trade/Menu/Shortcut + icon row via `OnButtonClicked`). Optional **cooldowns** list (hook pair). Default theme = the existing cream/blue. | menu buttons only | 4-6 d + TEST 1 d |
| **2. Interaction** | Quick slots with their own layout/scale (GetShortCutIndexByPos + HitTest mapping for drops), click to use (`UseFuncKeyMapped`, non-keydown only), buff module (display, optional right-click cancel), notification badges (SetButtonBlink), popups opened next to our buttons. | yes | 3-4 d |
| **3. Skin (mockup)** | ImGui skin kit: atlas, 9-slice, font, cards in the mockup's order. Portrait later. Slot in ticket 123's chat module, then widget mode W2 (hide the window layer; the stock edit stays the hidden input). RmlUi instead, if the owner chooses it: +4-5 d for the backend. | as Phase 2 | 4-6 d (+ art) |
| **4. Layout polish** | Magnet snapping and links, per-module UI scale, presets incl. "mockup row", occlusion behaviour, Reset covering everything. | edit mode | 3-4 d |

Total about 16-22 working days for ImGui; add ~5 days for the RmlUi route. Each phase ships behind a switch and
falls back to the stock bar on any exception (the hud_native pattern).

## 8. Risks

- **Stock layers come back:** the client re-shows them (quick slot toggle, `ReDrawQuickslot`, a new bar after a
  field change or resolution change). Re-assert every tick. Turning widget mode off must restore every layer and the
  hit test.
- **Click swallowing:** an invisible bar window takes map clicks at the bottom of the screen unless the HitTest hook
  masks it. Test with attack-by-click, NPC clicks and drops near the bottom edge.
- **Thread safety:** never call client functions from EndScene. Everything goes through the tick queue.
- **MapleRoot coupling:** the 26-slot layout lives in MapleRoot patches (`Array_Expanded`, the quick slot table, the
  CQuickslotKeyMappedMan size). A MapleRoot change breaks our reader. Byte-check 0x008DE926 (MapleRoot already does)
  and fall back to 8 slots.
- **Integration hooks are fatal on mismatch** (`VerifyKaentakeHooks`). Byte-check before every new `ATTACH_HOOK`
  (0x00A123A4, 0x00A1246C, 0x008D80A4) and skip the feature on a mismatch.
- **Overlay over stock windows:** without the occlusion step, modules sit on top of the inventory and stat windows.
  The fade/hide behaviour needs the owner's choice.
- **Texture cost:** keep icon uploads to a few per frame (budget), because Getpixel is slow. Managed pool, so device
  Reset is safe.
- **Click-to-use perception:** it is the same code as a key press, but it is new to v83 players. Mention it in the
  release notes. Keydown skills stay key-only.
- **Chat in widget mode** depends on ticket 123. Until then W1 keeps the stock chat, which looks mixed next to a
  skinned row.
- **UNVERIFIED items to check on TEST first:**
  - the UI_Toggle type numbers;
  - the 1001 and 1007 handlers;
  - the FUNCKEY type numbers 3/6/7;
  - the `GetItemIcon` flags;
  - `tLeftUnit*16` = buff total;
  - slot reads for i >= 8;
  - whether stock slots react to clicks.

## 9. Owner decisions

1. **Renderer for the skin:** ImGui skin kit first (recommended) or straight to RmlUi (the 1.7.0 plan).
2. **Widget mode:** opt-in (recommended) or default for new players. Keep stock chat in widget mode until 123's
   module has input (recommended), or wait to ship widget mode until then.
3. **Overlap with stock windows:** fade, hide or stay on top.
4. **Quick slot paging in the mockup:** display pages over the 26 slots (client only), or real extra binding sets
   (server work).
5. **Click-to-use on quick slots:** yes or no. Which keydown skills stay excluded.
6. **Art and font source:** CC0 pack, commissioned art, or stock UI.wz icons. Font choice and licence. Download size.
7. **Portrait on the character card:** yes (medium effort) or level badge only.
8. **The second icon row:** what bag / people / clipboard / star / menu open (map them to UI_Toggle types).
9. **Layout storage:** global (today's widgets) or per character (today's stock offsets). Presets.
10. **Magnet behaviour:** links on by default? The Alt/Ctrl modifiers.
