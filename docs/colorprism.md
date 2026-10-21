# Coloring Prism addon (`MAPLEROOT_COLOR_PRISM`)

Ported from the "ColorPrism TrueColor Fix" package (`after/` files). Double-clicking the Coloring Prism
(item 5782000) opens a window with a live avatar preview and Tone / Chroma / Brightness sliders, with tabs
for an equip's body, its effect layer, hair and eyes. Confirm sends the tint; the server stores it, consumes
one prism and pushes it to everyone in the map. The design notes stay in the module headers.

| file | owns |
|---|---|
| `weapontint.cpp/.h` | The recolour (HSV clones swapped in around `CAvatar::PrepareActionLayer` 0x00453AD1 and the face builder 0x00453696), the tint tables, both opcodes. |
| `weapontint_proto.h` | The 8-byte tint entry decoder, shared with `tests/weapontint_proto_test.cpp`. |
| `coloringprism.cpp` | The window. |
| `coloringprism_hooks.cpp` | The host hooks the package expects: cash-use gate 0x004863D5, sends 0x00A0A63F / 0x00A1DC5B, `CDraggableItem::OnDropped` 0x004EF140. |

All addresses were checked against the shipped `MapleStory.exe`
(sha256 `86e4e6be050caeda4a93443237256a84376ea74781fcba558d2418f0568aac90`): function starts, and each hooked
signature against its `ret N` and call sites (see the header of `coloringprism_hooks.cpp`).

## Wire

CP `0x372E` WEAPON_TINT_ACTION: `0` snapshot request; `1` apply invType(1) invPos(2) itemId(4) hue(2) chroma(1)
bright(1) prismPos(2) layer(1); `2` restore invType(1) invPos(2) itemId(4) prismPos(2) layer(1); `3` apply look
kind(1) hue(2) chroma(1) bright(1) prismPos(2); `4` restore look kind(1) prismPos(2).

LP `0x372F` WEAPON_TINT_SYNC: `1` snapshot count(1) + entries (the local player's dyed items); `2` result code(1)
(1 ok, 2 not a cash equip, 3 no prism, 4 not tinted, 5 failed); `3` map table players(1), per player name(u16 +
bytes) items(1) + entries. Entry = key(4) hue(2) chroma(1) bright(1); key is the equip id, id + 10000000 for its
effect layer, 1 hair, 2 face. Hue is absolute, stored as -(degrees + 1) in -360..-1; 0/0/0 is no tint.

## Changes from the package

- The snapshot decoder zeroed every negative hue, so every tint the server sent rendered at hue 0. Entries
  outside -360..-1 are now dropped (`WeaponTint_DecodeEntry`).
- The window art moves to `UI/UIWindowEx.img/ColorPrism` (Custom.wz cannot add a node to the stock
  `UIWindow.img`); the close button uses `UI/Basic.img/BtClose` because this client's `UIWindow.img` has no `Bag`.
- Item 5782000 is a new image, `Item/Cash/0578.img`, in Custom.wz (icon origin rewritten to (0,33)).
  `tools/wz/add_color_prism.py` builds both from `assets/colorprism/`.
- The cash-use and drop hooks the package leaves to the host are in `coloringprism_hooks.cpp`.
- Item name and description: `String/Cash.img` is stock and Custom.wz cannot add `5782000` to it, so
  `coloringprism_hooks.cpp` hooks CItemInfo's string worker (0x005CF6FE, behind both GetItemName 0x005CF63E and
  GetItemDesc 0x005CF69E) and fills in 5782000's `name`/`desc` when the stock answer is empty. The text is in
  `coloringprism_text.h` and matches the server's `tools/colorprism/install_wz.py`; ctest `colorprism_item_text`.
- `AVATAR_EQUIP_SLOTS` (52, the client's real array size) is defined in `wvs/avatar.h`; the struct is unchanged.
- The opcode is routed by `packetdispatcher.cpp` and `WeaponTint_Tick` runs from `CallUpdate`.

## Known gaps

- Cape/aura layers from `itemeff.cpp` are not dyed: `WeaponTint_BeginItemEffSwap` is not wired, because that
  module caches layers on item/action/flip and a tint change would not invalidate them.
