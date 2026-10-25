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
