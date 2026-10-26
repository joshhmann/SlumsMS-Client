# RmlUi HUD skin: plan (1.7.0)

Plan only, no code yet. The target is the owner's mockup (`docs/hud-skin-mockup-owner.png`). Only the HUD counts;
the HD world art in it is not achievable. Only the main HUD pieces get the new skin; every other window (inventory,
stats, shops, quests) stays stock (owner, 2026-10-26).

## Target, left to right along the bottom

1. **Chat panel.** Tabs (All / Party / Guild / Whisper / +), the log, an input with an emote button.
2. **Character card.** Portrait, a "LV. 200" badge, job, name.
3. **HP / MP / EXP.** Rounded bars with large numbers ("14758 / 32630", "0.00%").
4. **Quick slots.** 1-6 and Q/W/E/R rows, item icons with counts, page up/down.
5. **Button grid.** Shop / Trade / Menu / Shortcut with icons, and a second icon row.

The look: rounded cream panels (#EEEEDD family, as the 1.6.0 ImGui theme), soft gradients, blue accents (#99BBCC /
#557799), and crisp text at any size.

## Why RmlUi

HTML/CSS-style documents (RML + RCSS) with FreeType text. That gives:
- **Styling:** rounded panels, gradients, decorators and animations without hand-drawn geometry.
- **Layout:** flex layout that reflows when a module is resized.
- **Readable text:** scales cleanly at any size.

MIT licence. It sits next to ImGui on the existing D3D9 host. ImGui stays for tools (the HUD window, Edit UI grips);
RmlUi draws the skinned modules.

## Backend

- **No DX9 renderer upstream.** RmlUi 6.3 (latest tag) ships DX11, DX12, GL2, GL3, Vulkan and SDL_GPU renderers, so
  we write `RmlUi_Renderer_DX9`, modelled on the DX11 one and on ImGui's `imgui_impl_dx9.cpp`, which already runs on
  this device. Roughly 400-600 lines:
  - compiled geometry (vertex and index buffers per CompileGeometry);
  - textures (LoadTexture from PNG data, GenerateTexture from FreeType glyph atlases);
  - scissor regions and transforms;
  - D3D9 state save and restore around the draw, as ImGui's backend does.
  - Shaders and filters (RmlUi 6 "render interface 2" layers, filters, masks) are optional: skip them at first and
    use plain textured and coloured geometry.
- **Where it draws:** d3d9host.cpp's EndScene hook, after the game and before ImGui, so ImGui's tools stay on top.
  Reset handling as for ImGui (release and recreate device objects).
- **System interface:** time from GetTickCount64, logging to integration.log, clipboard via Win32.
- **File interface:** documents and art come from the client's Data folder (`Data/UI/SlumsHUD/*.rml|rcss|png`), so a
  skin change is a data change.
- **Art:** stock WZ sprites (item icons, skill icons) reach RmlUi as textures through a custom `wz:` source, e.g.
  `<img src="wz:Item/Consume/0200.img/02000000/info/icon"/>`, decoded with the client's own resource manager.

## Dependencies and size

- **FreeType:** already in 1.6.0 (`external/freetype`, 2.13.3).
- **RmlUi core:** a submodule at tag 6.3, built static for Win32 MSVC with `RMLUI_FONT_ENGINE=freetype`. No Lua,
  no SVG, no Lottie. Expect roughly +1.5-2 MB on slums.dll.
- **No other libraries:** the renderer and interfaces are ours.

## Input

- **Order:** RmlUi first, then ImGui, then the game, in the same WndProc path.
- **Mouse:** RmlUi takes a mouse event only when the cursor is over one of its elements or one has focus (the chat
  input). Otherwise it passes the event on, so clicks on the map still reach the game.
- **Keyboard:** goes to RmlUi only while its chat input has focus; game hotkeys keep working otherwise.
- **Edit UI (Ctrl+F11):** keeps working. RmlUi modules become elements of the same layout store
  (`HKCU\Software\SlumsMS\Layout`) with grips, snapping and reset.

## Data the modules need (all client-side)

- **HP / MP / max / EXP:** as the 1.6.0 modules read them (Fuse with the 4-byte HP/MP detour; MapleRoot's 64-bit EXP).
- **Level, job, name:** as hud_native.cpp reads them (`get_job_name`).
- **Portrait:** the client renders the avatar into a canvas. Find the call the character-info window uses for its
  avatar picture, copy that canvas into an RmlUi texture, and refresh it on equip changes. Medium effort.
- **Quick slots:** the key map (which skill or item sits on which key), icons from WZ, and live item counts from the
  inventory. Clicks route to the existing quick-slot handling: keep the stock control alive under the skin and pass
  the click through, as the 1.6.0 button groups do.
- **Chat:** the hardest. Mirror incoming lines (hook the chat-log add, `CUIStatusBar::ChatLogAdd` 0x008DB070) and
  route typing back through the stock chat input. Restyling and moving the stock chat comes first.

## Order of work (1.7.0)

1. **Backend and hosting:**
   - `RmlUi_Renderer_DX9` and the system and file interfaces;
   - one test document drawn over the field;
   - input routing.
   - About 2 days.
2. **HP / MP / EXP panel and the character card** (text and level badge first, portrait after):
   - replaces the 1.6.0 native modules when the skin is on.
   - About 1-2 days.
3. **Button grid:**
   - icons and labels;
   - clicks call the stock button handlers (`CUIStatusBar::OnButtonClicked` 0x008D423D).
   - About 1 day.
4. **Quick slots:**
   - icons, counts, key labels, page up/down;
   - clicks pass through to the stock quick slot.
   - About 2 days.
5. **Chat:**
   - skin and tabs on top of the stock chat first;
   - a full replacement only if that holds up.
   - 3 days or more.

Each step ships behind a switch (`[clienthax] hudskin`): stock (1.6.0 modules) or skin. Edit UI, layout storage and
Reset carry over.

## Also 1.7.0 (decided earlier)

- Game zoom (windowed, sharp scaling, mouse mapping).
- Widget mode that hides the stock bar.
- Magnet linking between modules.
- Moving the chat log (or the chat rebuild above).

## Risks

- **The DX9 renderer is new code on the game's device.** State leaks would show as broken game rendering. Save and
  restore all state, as ImGui's backend does, and test resolution changes and device Reset.
- **The portrait** depends on finding a clean avatar-to-canvas path. If it is messy, ship the card without one first.
- **Text input inside RmlUi** (chat) needs IME and caret work. That is why chat comes last.
- **Size:** RmlUi and the skin art add a few MB to the client download.
