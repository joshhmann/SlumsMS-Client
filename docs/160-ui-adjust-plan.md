# Adjustable UI: plan (1.6.0, else it leads 1.7.0)

Owner's direction, 2026-10-25: "I want widget and actual in game ui". Keep both:

- **(a) HUD widgets** (branch `client-160-d3d9`): the movable ImGui HP/MP/EXP widget, Ctrl+F11 freeze, Ctrl+F10 HUD window. This stays an option.
- **(b) The stock UI, adjustable in the real client** (ticket 91): the bottom status bar, the quick slot, and if feasible the HP/MP block, each moved on its own. The inventory resize (`client-160-invresize`) is already in. Positions are saved per character, with a reset.

Order: the hpmp TEST check, then buildshow, then this. Nothing here changes the wire.

## One "Edit UI" switch

Ctrl+F11 already freezes and unfreezes the HUD widgets. It becomes the single **Edit UI** switch:

- **Unfrozen (editing):**
  - the widgets can be dragged and resized, as today;
  - each movable stock element gets a grip, an outlined box with a label, drawn by the same ImGui overlay; dragging a grip moves that element live;
  - the HUD window (Ctrl+F10) gets "Reset UI layout", which resets the native positions, the widget positions and the inventory size.
- **Frozen (playing):** no grips, and every mouse click goes to the game, as today.

The grips are drawn by the ImGui overlay, so (b)'s editing needs the D3D9 host (`[graphics] d3d9host`, on by default). The saved positions apply with or without the host. If the host is off, a player keeps their last layout but cannot change it.

## How the stock elements move

`src/resolution.cpp` already places the stock HUD through its own origin vectors (`src/wvs/wndman.h` `ResetOrgWindow`):

- `CWndMan::ms_pOrgStatusBar` holds the bar and everything `CUIStatusBar::OnCreate` builds on it;
- `ms_pOrgQuickSlot` holds the quick slot. Its hit-testing is already offset through the `GetShortCutIndexByPos` hook;
- `ms_pOrgScreenMsg` holds the screen messages.

Moving an element is an extra `RelMove(dx, dy)` on its origin, applied right after `ResetOrgWindow` (start-up, resolution change) and when a character loads. The offset is clamped so the element stays fully on screen at every resolution. Offsets are stored as fractions of the screen, like the widgets (`overlay_policy.h`), so a layout survives a resolution change.

| Element | Origin | Feasible for 1.6.0? |
| --- | --- | --- |
| Bottom bar (HP/MP/EXP gauges, menu buttons, chat) | `ms_pOrgStatusBar` | Likely. It already moves as one piece; check button and chat hit-testing after a move. |
| Quick slot | `ms_pOrgQuickSlot` | Likely. Its hit-test is already offset; check the expand/collapse toggle (`m_bQuickSlotUp`) and the screen-message nudge that depends on it. |
| HP/MP block on its own | none: the gauges are drawn on the status bar's single canvas (`CUIStatusBar::Draw`) | Not without splitting that canvas into a second layer and hit area. **1.7.0**, unless the bar and quick slot are done early. |
| Inventory size | invresize (already per character) | Done. Only the shared reset is new. |

## Storage

`HKCU\Software\SlumsMS\Layout`, one value per character, `w<world>_c<id>`, matching invresize's `HKCU\Software\SlumsMS\Inventory`. It is loaded from the same `CConfig::LoadCharacter` hook, because the client rewrites its own `g0_` keys on field entry. Widget positions stay global in `HKCU\Software\SlumsMS\Overlay`.

## Steps

1. Merge `client-160-d3d9` into the 1.6.0 line, together with hpmp. `hud_vitals.cpp`'s Fuse<short> must return an int, so the widget shows HP over 32,767.
2. Add a layout store and policy (header-only, with a native test): the per-character key, fraction encoding, on-screen clamping, reset.
3. Apply the status bar offset after `ResetOrgWindow` and on character load. Check on TEST across resolutions: button clicks, chat input, gauge flash, level-up effect.
4. Apply the quick slot offset the same way. Check slot clicks, drag to and from slots, key-config display, the up/down toggle.
5. Grips in the overlay while unfrozen: rectangles from each element's origin and size, a drag writes the offset, and Reset clears it.
6. QA on TEST through client-mcp (mouse_drag already exists). Owner check, then the RC.

## Risks

- Stock code that hit-tests the status bar or quick slot in absolute coordinates, instead of through the origin, would mis-click after a move. Steps 3 and 4 exist to find these, one element at a time.
- An element moved to the top could overlap stock windows (minimap, chat log). The overlap is allowed; Reset is the escape hatch.
- Edit mode depends on the D3D9 host. If a player turns the host off, they keep their layout but cannot change it.

## Direction update: a modular HUD (owner, 2026-10-26)

The owner wants an FFXIV-style modular HUD: the HUD is a set of elements, each movable and adjustable (scale, text
size), edited in one layout mode. Decisions:

- **Two kinds of module.** Stock pieces the client already keeps on their own layer or window move with the stock art:
  quick slot, buff icons (`CTemporaryStatView`), minimap, quest helper, chat log. Pieces baked into the status bar's
  single canvas (HP/MP gauges, EXP, level/name) are rebuilt as our own modules from live client data, each with its
  own position, scale and text size.
- **Look: both, per module, player's choice.** Stock MapleStory art (the client's gauge sprites and fonts) or the
  themed ImGui style of the current HP/MP/EXP widget.
- **The stock bar stays and hides what is replaced.** Each part a module covers is hidden from the stock bar; what is
  not replaced yet (menu buttons, chat input) stays. The bar shrinks as modules land.
- **Module priority: all four groups.** The HP/MP/EXP split (EXP as its own long bar), quick slots / hotbars
  (movable, scalable, row layouts), buffs and cooldowns, then chat (log and input, text size).
- **Editor.** Ctrl+F11 Edit UI with a snap grid, a per-element panel (scale, text size, opacity, look, reset), and
  layout presets, kept per character.
- **Scope.** 1.6.0 (about Wed Oct 28) ships the bar and quick slot mover, the editor basics (grid, reset) and one
  module: HP/MP/EXP split into separate modules. The rest of the modular HUD (scaling of stock pieces, presets,
  more modules, hiding replaced stock parts) leads 1.7.0.


## State at the 1.6.0 merge (2026-10-26)

Shipped in 1.6.0 (checked on TEST, AqClient, 1600x900):
- **Edit UI:** Ctrl+F11, starts off every session.
- **Mover:** the status bar and quick slot drag live and are saved per character. Clicks follow the move: the game menu, and items dropped on quick slots.
- **Modules:** HP, MP and EXP are separate modules with show/hide, text size, opacity and reset.
- **Editing aids:** a 1.25% snap grid, and "Reset UI layout" (widgets, bar, quick slot, inventory size).

Known, not fixed in 1.6.0:
- The status bar moves vertically only: its art is 1920 px wide, wider than most screens.
- The ImGui modules draw over stock windows (inventory, stat window) when they overlap. The overlay draws after the game in EndScene, so drawing under the stock UI is not cheap. Hiding a module while a stock window covers it means walking `CWndMan::ms_lpWindow` for window rects. That is 1.7.0.
- The stock-art look for modules (client gauge sprites as textures) is 1.7.0.
- The QA driver (client-mcp) finds a moved status bar. Its chat_open check for a moved bar loads at the next Start-QA restart.

## Dissolving the bar (owner, 2026-10-26: in 1.6.0)

The stock bar is three kinds of parts:
- **Painted on its canvas:** HP/MP gauges and numbers, EXP, level/job/name. These are replaced by modules, and the stock painting is hidden.
- **Child controls:** 17, logged on TEST. Shop 1000, Trade 1001, Menu 1002, Shortcut 1007, the icon row 2000-2003/1005/1006/1009, the chat input 1011 and its type selector 1012.
- **Own layers:** the quick slot and the chat log.

Done:
- The two button groups move and click, through the HitTest hook above. Reset restores them.
- The QA state file replaces pixel guessing.

Next: the chat input and selector as a module, a level/job/name module, then hiding the painted parts the modules replace. Known: the game menu still pops up at its stock place above the bar, not next to a moved Menu button.

## The owner's split (2026-10-26, docs/160-ui-split-reference.png)

Separate pieces: chat log, chat input row, level/job/name, HP, MP, EXP, the main buttons, the icon row, and the quick
slots. What the stock bar is made of (status bar layers, logged on TEST):

| Layer (CUIStatusBar +) | Size | What |
| --- | --- | --- |
| 0x18 (window layer) | 1920x578 | level number, job, name, HP/MP/EXP numbers (drawn text) |
| 0xCD0 | 800x578 | bar art: the level plate, the gauge plate, the chat row |
| 0xCCC | 800x578 | the light background strip |
| 0xCD4 | 340x31 | the three gauge bars with their HP/MP/EXP labels |
| 0xBB8 / 0xBBC / 0xBC0 (+0xBC4) | 1x16 stretched | HP / MP / EXP gauge covers (width follows the value) |
| 0xCB8 | 434x14 | the chat input line |
| 0xCC4 (+0xCC8, 0xD80) | 1008x80 | the quick slot |

Moving layers works for whole pieces (the quick slot, the gauge bars as one strip), but HP, MP, EXP and level/name
each span several shared canvases. They become **native modules**: our own game layers, built from the stock
UI.wz art (gauge, plate and number sprites) and the game font, placed and sized freely. Then the stock pieces they
replace are hidden (their layers, and their areas of the shared canvases). Same look as stock, fully split, and the
base for an ElvUI-style skin later.

Done in the split: buttons, icon row, chat input (moves, types, survives chat open/close).


## Parked (owner, 2026-10-26)

Parsec and the exit-time crash (C0000005 in slums.dll's exit cleanup, seen once while the owner was on Parsec) are
parked: Parsec is "easily bypassable". No DXVK or dgVoodoo work.
