# Tickets 119 + 120: client research (read-only)

Date: 2026-10-28. Source: `/root/work/client/src-161-qa` (branch client-161-qa, HEAD ca7c572).
Exe: `scratchpad/client/MapleStory.exe` (sha256 prefix 86e4e6be050caeda, image base 0x400000; raw file offsets equal VA - 0x400000).
Every address marked "verified" was checked with `objdump -d -M intel --start-address=...` against that exe. StringPool ids were decoded
with `scratchpad/tools/stringpool.py`.

**The IDB is no help here.** `/root/Maplestory/v83.idb` has only 684 non-`sub_` function names (StringPool,
CInPacket, CField::OnPacket, CRT). It has nothing for CAnimationDisplayer, CMapLoadable, CPet or CSummoned. The full
dump is `scratchpad/research/v83_funcnames.txt`. The names below come from the code's behaviour, from StringPool
strings and from names already used in our source. Names I inferred are marked (inferred).

---

## 119: Short damage numbers (K/M/B) and a size slider

### Summary
The client draws a damage number in one function, `CAnimationDisplayer::Effect_HP` at 0x00437D0F. It formats the int
with `"%d"`, then turns each character into a digit sprite with `c - '0'`, using that number as a child name in one
of 8 digit sets from `Effect/BasicEff.img`. It packs the digits into ONE new canvas (height 57), puts that canvas in
ONE layer and animates it: 400 ms solid, 600 ms fade, 30 px rise.

That gives two small patches:
1. Format: replace the one `ZXString::Format` call. Big values become a short string whose `.`, `K`, `M` and `B`
   are written as `:`, `;`, `<` and `=`. Since `c - '0'` maps those to 10..13, the existing loop looks up child
   nodes "10".."13" with no change to the lookup code.
2. Scale: replace the two `InsertCanvas` calls. Before the canvas goes into the layer, swap it for a stretched copy
   (`IWzCanvas::CopyEx` already stretches; see weathercanvas.cpp), and shift its origin so it stays centred.

**Damage skins: none exist on the client today.** No branch of slums.dll has damage-skin code (I searched every
ref in the client repo). On the server, `client/DamageSkin.java`, `Character.setDamageSkin` and
`PacketCreator.Packets.updateDamageSkin` (CUSTOM_FIELD subop 0x01) are all commented out. So "damage skins keep
working" is a constraint on a future skin: it must ship glyphs 10..13 in each of its sets, or our format falls back
to plain digits for that set. The natural way to build a skin is to swap the 8 property pointers at
CAnimationDisplayer+0x170..+0x18C, so the format hook must test whichever set is active at that moment.

### Existing code to extend
- `src/overlay.cpp:307` `Overlay_DrawSettings()`. The slider goes next to "HUD size" (`overlay.cpp:347`). Prefs use
  `ReadPref`/`WritePref` (`overlay.cpp:32-39`) under `HKCU\Software\SlumsMS\Overlay` (`overlay.cpp:24`).
- `src/settings_overlay.cpp:14` is the window shell (Ctrl+F10). It has no changes of its own.
- `src/mobdamage.cpp` + `src/mobdamage_patch.h` are the pattern to copy: a `*_patch.h` with the stock bytes,
  `Classify()` and `PatchedBytes()`, a config.ini opt-out read with `GetPrivateProfileStringA`, and a ctest that
  checks the bytes against the exe (`tests/mobdamage_patch_test.cpp`, `tests/CMakeLists.txt:41-47`).
- `src/resman.cpp:46-77` (the `CWzProperty::raw_Serialize_hook` Custom.wz merge) and `src/movementkeys.cpp:164`
  (`InjectMovementKeyIcons`, called at `resman.cpp:53`) show how to add canvases into a stock .img property as it
  loads. Use the same pattern to inject the glyphs into `Effect/BasicEff.img/No*`.
- `src/weathercanvas.cpp:476` already calls `IWzCanvas::CopyEx(dx, dy, src, CA_OVERWRITE, dw, dh, sx, sy, sw, sh)`.
  Its comment at :447-459 records that an earlier release used this call to stretch one pixel over a whole canvas,
  so stretching is proven to work.
- `src/hook.h` `AttachClientHooks()` is where the new `AttachDamageFmtMod()` is called from.

### Client hook points (all bytes verified)

| What | Address | Bytes | Notes |
|---|---|---|---|
| TSingleton<CAnimationDisplayer> instance | 0x00BEBF6C | `a1 6c bf be 00` at 0x9F9E07 | CreateInstance 0x009F9DFC (already in bypass.cpp:214), object size 0x1A8 |
| CAnimationDisplayer ctor (inferred) | 0x00435444 | `b8 49 94 a7 00 e8 4a b7 62 00` | Loads `Effect/BasicEff.img` (SP 0x3C8) into its digit sets, listed below |
| Digit-set members (IWzPropertyPtr) | this+0x170 NoRed0, +0x174 NoRed1, +0x178 NoBlue0, +0x17C NoBlue1, +0x180 NoViolet0, +0x184 NoViolet1, +0x188 NoCri0, +0x18C NoCri1 | ctor stores at 0x4356DA.. (SP 0x3C9..0x3D0) | "1" = large first digit, "0" = the rest |
| **CAnimationDisplayer::Effect_HP** (inferred) | 0x00437D0F | `b8 cf 9a a7 00 e8 7f 8e 62 00 81 ec f8 00 00 00` | __thiscall, `ret 0x14`: (int x, int y, int nDamage, int nType 0 red/1 blue/2 violet, int bCri). SEH prolog |
| Set selection | 0x437D35..0x437DB9 | | nType 0 + bCri: first=+0x18C, rest=+0x188. nType 0: +0x174/+0x170. 1: +0x17C/+0x178. 2: +0x184/+0x180. Stored in [ebp-0x18] (first) and [ebp-0x20] (rest) |
| **Format call (patch 1)** | 0x00437DFB | `e8 4b dd 00 00` (call 0x00445B4B) | `ZXString<char>::Format(ZXString*, const char* fmt, ...)`, __cdecl variadic. Args: &[ebp-0x1c], "%d" (SP 0x158B), nDamage; `add esp,0xc` follows |
| ZXString<char>::Format | 0x00445B4B | `8b 4c 24 04 8d 44 24 0c 50 ff 74 24 0c e8 f7 40 00 00` | forwards to FormatV 0x00449C54 |
| Digit lookup `c - '0'` | 0x437E52, 0x437ECA, 0x4380EE, 0x43827B | `0f be c0 83 e8 30` (each) | the result goes through `_itow` (0x00414ABC, `push 0xa`), so the child name is the decimal string: 10..13 become "10".."13". **No patch needed** |
| Missing child | 0x437F41 | `push ebx; call 0xA5FDE4` | `_com_issue_error` when the child is missing. **A missing glyph throws inside Effect_HP**, so the art must be present before the short format is turned on |
| Composite canvas | 0x437FD4..0x438069 | SP 0x3BC "Canvas", vtbl+0x2C Create(w=[ebp-0x24], h=0x39) | the digits are copied in with vtbl+0x80 at 0x43821E / 0x43839B; the crit "effect" overlay is SP 0x66C |
| Gr2D layer | 0x0043844B | `e8 2e e8 fe ff` (call 0x00426C7E IWzGr2D::CreateLayer, ecx = [0xBF14EC]) | the same function weather.cpp:1080 hooks. z is set via vtbl+0xB4 (0xC00614A4) at 0x4384A5 |
| **InsertCanvas calls (patch 2)** | 0x00438574 and 0x0043868C | `e8 32 e6 fe ff` / `e8 1a e5 fe ff` (call 0x00426BAB) | __thiscall on the layer (ecx = [ebp+0x14]), `ret 0x1c` (7 args); canvas = [ebp-0x14]. First call: 400 ms (0x190) alpha 255→255. Second: 600 ms (0x258) 255→0 |
| Centring | 0x004383DF..0x4383F4 | `8b 45 dc 99 2b c2 8b c8 8b 45 08 d1 f9 2b c1 ... 83 c0 d1` | layer x = x - width/2, y = y - 0x2F. Move via vtbl+0x90 at 0x438775 (start) and 0x438849 (rise of -30 over 1000 ms) |
| Callers of Effect_HP | 0x574FB3 (mob hit), 0x669263, 0x7A7125 (CSummoned area), 0x92EC8E (CUser area) | | MISS is a separate function, 0x00438A21; not touched |

### Data / art needed
- 4 glyphs (`.`, `K`, `M`, `B`) × 8 sets = **32 canvases**, named "10" (.), "11" (K), "12" (M), "13" (B), under
  `Effect/BasicEff.img/{NoRed0,NoRed1,NoBlue0,NoBlue1,NoViolet0,NoViolet1,NoCri0,NoCri1}`. They must match the stock
  digits' height and origin (the "1" sets are the larger first-digit font). Draw them ourselves in the stock style;
  do not copy Kuro's art (ticket rule).
- Where they live: in our own Custom.wz, merged at load time (resman/movementkeys pattern). UNVERIFIED: whether
  `raw_Serialize_hook` sees the sub-property UOL `Effect/BasicEff.img/NoRed0` or only the .img UOL. If only the
  .img, have the inject function walk to the 8 children itself (the way InjectMovementKeyIcons walks UI.wz). A
  fallback that does not depend on serialize order: on the first Effect_HP call, `Add()` the canvases straight into
  the 8 live properties at CAnimationDisplayer+0x170..+0x18C.
- No server change. Damage is display-only.

### Design
- `damagefmt_policy.h` (pure, testable): `int ShortDamage(int v, char* out, size_t n)`.
  - v < 10,000 (or the feature is off): `"%d"`, unchanged.
  - Otherwise: 3 significant digits, **truncated, not rounded**, so 999,999 shows as 999K and never 1000K. Suffix K
    (1e3), M (1e6) or B (1e9); int max 2,147,483,647 becomes 2.14B. Trailing ".0" dropped ("12K", not "12.0K").
  - `EncodeGlyphs()`: `.`→`:`, `K`→`;`, `M`→`<`, `B`→`=`. Kept separate so tests can read the plain form.
  - Threshold and decimals are constants that tests pin.
- `damagefmt.cpp`:
  - Patch 1: rewrite the rel32 at 0x437DFB to `DamageFormat_cdecl(ZXString<char>* out, const char* fmt, int dmg)`. It
    calls the original 0x445B4B with `"%s"` and our buffer, or with fmt and dmg unchanged when off or when glyphs are
    missing.
  - To know which set is active, detour Effect_HP (0x437D0F) only to stash `this`, nType and bCri in statics (game
    thread only). The format hook then checks that the active "0" and "1" sets have children "10".."13", cached per
    IWzProperty pointer, so a future damage-skin set without glyphs falls back on its own.
  - Patch 2: rewrite the rel32 at 0x438574 and 0x43868C to a `__fastcall` wrapper with the 7-arg thiscall signature
    of 0x426BAB. When scale != 100, the first call builds the scaled canvas: Create(w*s, h*s), then one `CopyEx`
    stretch, then origin offset (w*(s-1)/2, h*(s-1)) so the rise and centring stay put. The second call reuses it.
    Then forward to 0x426BAB.
  - The scale is a global `int g_nDamagePct` (60..150). The slider writes it and it applies to the next number, so
    the change is live.
- Config: `config.ini [clienthax] shortdamage=true` (default true; false = stock `%d`, with no format patch
  installed). The size slider lives in the HUD window: registry `HKCU\Software\SlumsMS\Overlay\damage_pct` (DWORD,
  default 100) and `damage_short` (DWORD 0/1, a runtime switch under the ini master switch).
- CMake: `option(MAPLEROOT_DAMAGE_FORMAT "Short damage numbers (K/M/B) and a damage size slider" OFF)`, following the
  pattern in `CMakeLists.txt:3-14` and `src/CMakeLists.txt`.

### Risks
- **A missing glyph crashes the client** (the com error at 0x437F41). That is why the format path checks the glyphs
  per set and always falls back.
- UNVERIFIED: whether Gr2D honours a changed canvas origin for layer placement. If not, shift x/y instead: patch the
  `add eax,-0x2f` at 0x4383F4 and the halving at 0x4383EA, or adjust in the vtbl+0x90 call.
- UNVERIFIED: `CopyEx` filtering. Nearest-neighbour may look blocky at 150%. Fallback: lock pixels and do a bilinear
  resample with weathercanvas's A4R4G4B4/A8R8G8B8 path, but keep that off by default; the bWantPixels history in
  weathercanvas.h is a warning.
- Each damage number creates one extra canvas when scaled. Cost is negligible; free it right after the second insert.
- Other patches must not touch 0x437DFB, 0x438574 or 0x43868C. HpMp_Verify-style self-check: compare the bytes at
  attach time and refuse on a mismatch (Classify → Unknown → stock).
- Effect_HP uses an int; there is no 64-bit damage path, so B is enough.

### Scaffold plan
- `src/damagefmt_policy.h`: `ShortDamage`, `EncodeGlyphs`, `ClampPct` (no Windows).
- `src/damagefmt_patch.h`: addresses and stock bytes (0x437D0F prolog, 0x437DFB, 0x438574, 0x43868C,
  0x437E52/0x437ECA/0x4380EE/0x43827B as read-only anchors), plus `Classify()` and `PatchedCall(site, target)`.
- `src/damagefmt.cpp`: `AttachDamageFormatMod()`, `DamageFmt_SetScale(int)`, `DamageFmt_SetShort(bool)`,
  `DamageFmt_InjectGlyphs(IWzProperty*, const wchar_t* uol)` (called from resman.cpp:53 next to the movement keys
  call).
- `src/overlay.cpp`: in `Overlay_DrawSettings()`, a "Damage numbers" row with a "Short (K/M/B)" checkbox and a "Size"
  `SliderInt` (60..150 %), using WritePref("damage_short"/"damage_pct").
- `src/hook.h`: declaration plus the call under `#ifdef MAPLEROOT_DAMAGE_FORMAT`.
- `Custom.wz`: 32 glyph canvases.
- `tests/damagefmt_policy_test.cpp`. Cases: 9,999 → "9999"; 10,000 → "10K"; 12,345 → "12.3K"; 999,999 → "999K";
  1,234,567 → "1.23M"; 2,147,483,647 → "2.14B"; 0 and negative unchanged; glyph encoding.
- `tests/damagefmt_patch_test.cpp`: checks the stock bytes at the 4 sites against the exe (mobdamage_patch_test
  pattern).
- config.example.ini and README: `[clienthax] shortdamage`.

### Test plan (TEST)
1. Default: a hit under 10k looks exactly as before. Then 12,345 / 1,234,567 against a high-HP mob (GM `!damage`
   style). Check red, crit (NoCri) and heal (blue) numbers, plus a summon's and a party member's numbers.
2. Slider at 60 / 100 / 150 % while hitting: the next number is resized and stays centred over the mob; no restart.
3. Remove one glyph from Custom.wz on a TEST copy: plain digits, no crash, log line `DAMAGEFMT_GLYPHS_MISSING set=...`.
4. `shortdamage=false`: stock behaviour, with no patch bytes written (check with the debug log).
5. Normal exit with the HUD window open: native cleanup completes (the existing shutdown markers).

### Effort
S–M: about 1 day of code and tests, plus art for 32 small glyphs and one TEST session.

---

## 120: Display and performance toggles

### Summary
Every map layer is an `IWzGr2DLayer`, and each has `visible` and `color` (ARGB; alpha in the top byte, multiplying
with the canvas). Both are already written live by our code: weather.cpp, lamps.cpp, itemeff.cpp and wnd.h. So
every toggle works the same way: **capture the layers at creation, tag them with a category and an owner (own or
other), then write `visible` or `color` whenever a toggle changes and on each new capture.** Nothing waits for a
map change, as long as the captured lists stay alive until the field is torn down. This is the lifetime rule
weather already follows: weather.cpp:282-294 and the RestoreObj re-arm at :1194.

The difficulty is the hooks, not the rendering. Weather already detours IWzGr2D::CreateLayer, LoadMap, Update,
RestoreTile, RestoreObj, RestoreBack and MakeBack. Lamps detours MakeObj. Weather only captures **in sky fields**
(`g_bSceneryScope` / `g_bInSkyField`, weather.cpp:238/251) and only when MAPLEROOT_WEATHER is built. So 120 needs its
own capture that does not depend on weather.

### Existing code to extend
- `src/weather.cpp:1080-1176`: the `IWzGr2D__CreateLayer_hook` capture router; one flag per category routes the
  layer. `:1179` RestoreTile_hook and `:1194` RestoreObj_hook (re-arm when graphics detail changes); `:2279-2287` the
  attach list; `:1070` `ApplyList` (writes `color` with try/catch).
- `src/lamps.cpp:1491` `CMapLoadable::MakeObj_hook`, attached at `:2575`.
- `src/wvs/field.h:8-21`: CMapLoadable MEMBER_HOOKs (LoadMap 0x639B3D, Update 0x6399EF, RestoreTile 0x63A100,
  RestoreObj 0x63AA7E, RestoreBack 0x63CBBA, MakeBack 0x63CD4E, MakeObj 0x63AD16). **MEMBER_HOOK makes one static
  pointer per class**, so a second module cannot define `CMapLoadable::RestoreTile_hook` again. It needs its own
  pointer variables (Detours chains a second detour on the same target).
- `src/itemeff.cpp:56` `UpdateItemEff(CUser*)` builds the extra Effect/ItemEff layers for **every** CUser, local and
  remote (`aItemEffectLayer[i].l.pLayer`, wvs/avatar.h:42). It is hooked from CUser::UpdateAdditionalLayer 0x940EB7,
  CUser::OnAvatarModified 0x92E916 and CUser::SetMoveAction 0x92ECD1 (itemeff.cpp:93-112). The cap and the "hide
  others' cash effects" toggle can go straight in here.
- The local user is `*(CUserLocal**)0x00BEBF98` (overlay.cpp:30, bounce.cpp:36, monsterbook_drops.cpp:286). Owner
  test: `pUser == localUser`.
- HUD settings: `src/overlay.cpp:307` (see 119).

### Client hook points

| What | Address | Bytes (verified) | Conv. / notes |
|---|---|---|---|
| IWzGr2D::CreateLayer (global Gr2D at [0x00BF14EC]) | 0x00426C7E | `55 8b ec 51 53 56 8b 75 24 57 8b d9` | __thiscall, 8 args after this (weather.cpp:1080 signature). Already detoured by weather |
| CMapLoadable::LoadMap | 0x00639B3D | `b8 c2 d2 a9 00 e8 51 70 42 00` | thiscall, no args; teardown and rebuild point |
| CMapLoadable::Update | 0x006399EF | `b8 b8 d1 a9 00 e8 9f 71 42 00` | per frame; a good place to apply after a toggle change |
| CMapLoadable::RestoreTile | 0x0063A100 | `b8 83 d3 a9 00 e8 8e 6a 42 00` | rebuilt only by LoadMap |
| **MakeTile** (inferred; not hooked by anyone yet) | 0x0063A5C4 | `b8 48 d4 a9 00 e8 ca 65 42 00` | thiscall, `ret 0xc`. Creates one tile layer (CreateLayer at 0x63A88C) and AddTails it into **ZList<IWzGr2DLayerPtr> at CMapLoadable+0x3C** (0x63A9A4 `lea ecx,[eax+0x3c]`, 0x63A9A7 `call 0x644E01`) |
| CMapLoadable::RestoreObj | 0x0063AA7E | `b8 ac d4 a9 00 e8 10 61 42 00` | also run by the graphics-detail apply 0x00642890 (`b8 c8 e4 a9 00 ...`) |
| CMapLoadable::MakeObj | 0x0063AD16 | `b8 bf d7 a9 00 e8 78 5e 42 00` | lamps.cpp owns the MEMBER_HOOK. Tagged objects go to a map at this+0xB8 (0x63B781); the general object store is touched at this+0x50 (0x63B823): **layout UNVERIFIED** |
| CMapLoadable::RestoreBack / MakeBack | 0x0063CBBA / 0x0063CD4E | `b8 5c d9 a9 00 ...` / `b8 f5 db a9 00 ...` | backs rebuilt by LoadMap and by RestoreBack (resolution, zoom, detail) |
| Generic displayer layer loader (inferred) | 0x0043EA3E | `b8 77 a9 a7 00 e8 50 21 62 00` | __cdecl, about 10 args, **56 callers**, one of them CUser::LoadLayer (0x941728). Most skill and one-time effects come through here |
| CUser::LoadLayer | 0x00941417 | `b8 73 d7 ad 00 e8 77 f7 11 00` | thiscall (bsUOL, bLeft, USERLAYER&, int*), signature in itemeff.cpp:17. `this` gives the owner of every user-attached layer (stock ItemEff/SetEff/chair effects) |
| CUser::UpdateAdditionalLayer | 0x00940EB7 | `b8 dc d6 ad 00 e8 d7 fc 11 00` | already hooked (itemeff.cpp) |
| **CUserPool::OnUserRemotePacket** (inferred) | 0x00972401 | `55 8b ec 53 56 57 8b f9 8b 4d 0c` | __thiscall (int nType, CInPacket*), `ret 0x8`. Finds the CUserRemote (0x9716ED) and dispatches; calls CUserRemote::OnAttack 0x9803AB at 0x972550. **A thread-local "creating for another player" flag around this call tags every layer created by others' attacks, skill effects and emotes** |
| Pet init (inferred CPet::Init) | 0x00703314 | `b8 80 d9 aa 00 e8 7a d8 35 00 83 ec 2c 8b 45 08` | thiscall, `ret 0x8`. CreateLayer at 0x70358F; the layer is stored at **this+0x124** (0x703596). Callers: 0x93919E (CUser), 0x95AB78 (CUserLocal range), 0x97F835 / 0x983B93 (CUserRemote range). A second builder, 0x007036BC (CreateLayer 0x70385D), also writes this+0x124. The name "CPet" is UNVERIFIED (inferred from the call sites and from "Item/Pet/" SP 0x69D at 0x70AC6C) |
| Summon init (inferred CSummoned::Init) | 0x007A3366 | `b8 ce 73 ab 00 e8 28 d8 2b 00 83 ec 34 8b 45 0c` | thiscall, `ret 0x20`. CreateLayer at 0x7A35E0, layer at **this+0xD8** (0x7A35E7). Second builder 0x7A7B46 (CreateLayer 0x7A7CE1). Callers include 0x949ACD (CUserLocal range) and 0x9601F9. Owner field offset UNVERIFIED |

### Per toggle

**Hide map back / tiles / objects** (live, no map change):
- Tiles: hook MakeTile 0x63A5C4 (unclaimed) with a scoped "category = tile" flag, and capture in our CreateLayer
  post-hook. Alternatively walk the ZList at CField+0x3C directly (`src/ztl/zcoll.h:126`: count/head/tail after
  the vtable). Walking needs no hook at all; it is UNVERIFIED that this list holds every tile.
- Backs: a chained detour on MakeBack 0x63CD4E plus RestoreBack, same capture.
- Objects: a chained detour on MakeObj 0x63AD16 plus RestoreObj. Re-arm on the graphics-detail path the way
  weather.cpp:1194 does.
- Hiding = `layer->visible = 0`. Not color alpha 0: invisible layers likely skip the draw, which is the FPS gain
  (UNVERIFIED; measure it). Leave foreground (`front`) backs alone, or give them their own checkbox: hiding them may
  look broken on some maps.
- Clear the lists in a chained LoadMap pre-hook (the same teardown edge as weather.cpp:1993-2046).

**Opacity for skill effects, pets and summons, own vs others**:
- Skill effects: in the CreateLayer post-hook, tag a layer "other" when the thread-local flag from
  CUserPool::OnUserRemotePacket (0x972401) is set; otherwise "own" when created under the local user's skill path.
  Write `color = (alpha<<24) | 0xFFFFFF`. UNVERIFIED: whether the stock one-time animations rewrite `color` each
  frame. The damage-number code uses per-canvas alpha (InsertCanvas a0→a1), not `color`, which suggests `color`
  multiplies and stays. Check on TEST.
- Pets: post-hook 0x703314 / 0x7036BC, read the layer at this+0x124 and pick the owner from the call site
  (CUserLocal vs CUserRemote callers above), or compare the pet's owner field to the local user (offset UNVERIFIED).
- Summons: post-hook 0x7A3366 / 0x7A7B46, layer at this+0xD8. Owner: compare the summon's owner character id with
  the local character id (offsets UNVERIFIED; find them in CSummonedPool::OnCreated's decode). Summoned layers may
  include several sub-layers (attack and die animations through 0x43EA3E); the flag approach covers those as long
  as they are created inside CSummonedPool packet handling (UNVERIFIED; needs its address).

**Hide other players' cash effects**:
- slums-owned ItemEff layers: in `UpdateItemEff` (itemeff.cpp:56), when `pUser != local`, set `visible` from the
  toggle each time it runs, and walk all known users when the toggle changes.
- Stock ItemEff/SetEff (SP 0x980-0x983 "Effect/SetEff.img/%d/effect", "Effect/ItemEff.img/%d/0|1"; many references
  in 0x93A0C0..0x940A10): detour CUser::LoadLayer 0x941417. After the original, if `this != local` and the UOL starts
  with `Effect/ItemEff.img` or `Effect/SetEff.img`, set `l.pLayer->visible`. Keep a per-map registry
  (IWzGr2DLayerPtr + owner) so a toggle can re-apply live.

**Cap animated other-player items**:
- Same registry, filtered to remote, animating layers. Each N frames (Update hook), sort by distance to the local
  user. The nearest K get `Animate(GA_REPEAT)` and the rest `Animate(GA_STOP)` (both already used: weather.cpp,
  lamps.cpp, itemeff.cpp:85). K = slider 0..20; "off" = unlimited (stock).

### Data / art needed
None. No server change.

### Design
- New module `displayfilter.cpp`, independent of MAPLEROOT_WEATHER:
  - Its own Detours pointer variables (not the `CMapLoadable::X` MEMBER_HOOK statics) on: CreateLayer (post),
    MakeTile 0x63A5C4, MakeObj, MakeBack, RestoreBack, RestoreObj, LoadMap, Update,
    CUserPool::OnUserRemotePacket 0x972401, CUser::LoadLayer 0x941417, pet init 0x703314/0x7036BC and summon init
    0x7A3366/0x7A7B46.
  - Attach AFTER weather and lamps, so their detours stay first and ours chain behind them.
  - Category lists: Back, Tile, Obj, EffOwn, EffOther, PetOwn, PetOther, SumOwn, SumOther, CashOther.
  - When nothing is enabled (the default), the hooks only push to lists, or skip capture entirely while every
    filter is stock. Stock defaults change nothing.
  - `DisplayFilter_Apply()` runs on a toggle change from the HUD window. The ImGui window is drawn from the D3D9
    Present path on the game thread (UNVERIFIED that it is the same thread as Gr2D; if not, set a dirty flag and
    apply it in the Update hook).
- `displayfilter_policy.h` (pure, testable): `struct Prefs { bool hideBack, hideTile, hideObj; int effOwnPct,
  effOtherPct, petOwnPct, petOtherPct, sumOwnPct, sumOtherPct; bool hideOtherCash; int animCap; }`. Functions:
  `AlphaArgb(pct)`, `Visible(category, prefs)`, `ChooseAnimated(distances[], cap) -> mask`, and registry key names.
  Defaults equal stock.
- Registry under `HKCU\Software\SlumsMS\Overlay`: `df_hide_back`, `df_hide_tile`, `df_hide_obj`, `df_eff_own`,
  `df_eff_other`, `df_pet_own`, `df_pet_other`, `df_sum_own`, `df_sum_other` (percent, default 100),
  `df_hide_other_cash` (0), `df_anim_cap` (0 = off).
- HUD window: a "Display" collapsing header in `Overlay_DrawSettings()` with 3 checkboxes, 6 sliders, 1 checkbox and
  1 slider. Each applies on change.
- config.ini `[clienthax] displayfilter=true` as the master kill switch (false = no hooks attached).
- CMake: `option(MAPLEROOT_DISPLAY_FILTER "HUD display/performance toggles: hide map layers, effect/pet/summon opacity, other-player cash effects" OFF)`.

### Risks
- **Double detours** on CreateLayer, MakeObj, MakeBack, RestoreObj and LoadMap that weather and lamps already own.
  Detours supports chaining, but CheckedHook / VerifyKaentakeHooks (checked_hook.h) may flag the changed prologue.
  Check how `VerifyKaentakeHooks` treats an already-patched target before shipping. Alternative: one shared
  `maplayers.cpp` capture that both weather and displayfilter subscribe to. That is cleaner, but it refactors
  weather, which the owner has accepted.
- **COM ref lifetime**: holding IWzGr2DLayerPtr keeps layers alive past the game's own release. Weather does this per
  map and clears in LoadMap, so map-scoped lists are proven safe. Per-user layers (cash effects, pets) churn
  within a map. Prune entries in Update whose `AddRef()`/`Release()` count shows only our reference (UNVERIFIED
  whether Gr2D's render list holds its own ref; if it does, the count never drops to 1 and the layer stays drawn
  after the game "removes" it). The safer choice: store the owner pointer plus a USERLAYER pointer and re-read it,
  instead of holding the layer.
- Writing `color` on layers that weather also tints (backs, tiles, objects): the two modules fight. Hide with
  `visible`, never alpha, and let weather own `color` there.
- Effects created outside packet handling (local skill casts from key input, buffs refreshed in Update) are
  "own" by default. Effects created for others outside 0x972401 (party buffs via CUserPool::OnUserCommonPacket,
  summon pool, CMob-hit effects) are UNVERIFIED and need more context hooks.
- Hiding objects also hides portals, ropes and ladders drawn as objects, and it would affect lamps. Exclude
  lamp-captured layers (Lamps_CaptureLayer consumes first) and portal layers (weather already skips with
  g_bObjKeepNativeColor).

### Scaffold plan
- `src/displayfilter_policy.h` and `tests/displayfilter_policy_test.cpp`: defaults are stock; AlphaArgb(100) =
  0xFFFFFFFF; the cap picks the nearest K; key names are stable.
- `src/displayfilter_patch.h`: addresses and stock prologue bytes for all hooks above, with a `Classify()` byte check
  and a ctest against the exe.
- `src/displayfilter.cpp`: `AttachDisplayFilterMod()`, `DisplayFilter_LoadPrefs()`, `DisplayFilter_Apply()`,
  `DisplayFilter_DrawSettings()` (called from overlay.cpp).
- `src/itemeff.cpp`: call `DisplayFilter_OnItemEffLayer(pUser, layer)` at the end of UpdateItemEff.
- `src/hook.h`: declaration plus the call after weather and lamps.
- `src/CMakeLists.txt` and `CMakeLists.txt`: the option and its define; config.example.ini and README keys.
- Suggested order, smallest first: (1) hide tiles/backs/objects; (2) others' cash effects and the cap (itemeff plus
  CUser::LoadLayer); (3) pets and summons; (4) skill-effect opacity, which has the most UNVERIFIED context work.

### Test plan (TEST)
1. Henesys and a crowded FM room: each map toggle on and off with no map change; the next map load keeps the
   setting; changing graphics detail and resolution keeps it (the RestoreObj/RestoreBack rebuild paths).
2. Two characters (owner's test alt plus a bot) in one map: own vs other opacity for pets, summons and skills;
   others' cash weapon effects hidden and shown live.
3. Cap: 10+ players with animated cash items (bots in FM); cap 3 animates the nearest 3 only and updates as you move.
4. FPS: the d3d9host frame time (or the built-in FPS counter if present) on a crowded FM channel, all off vs. back +
   tiles hidden + cap 3. Record the numbers in the ticket.
5. Weather build at night: the tint is still right on visible layers; no crash on channel change, cash-shop
   round-trip or exit (native cleanup markers).

### Effort
Toggles 1–2: S each (about 1 day together). Pets and summons: S–M (owner offsets need RE). Skill-effect own vs
other opacity: M (context hooks beyond 0x972401). The hook-coexistence check with weather and lamps: S.

---

## Not verified / open items
- Function names for 0x435444, 0x437D0F, 0x43EA3E, 0x63A5C4, 0x703314, 0x7036BC, 0x7A3366, 0x7A7B46 and 0x972401
  are inferred from behaviour; the addresses and bytes are verified.
- CMapLoadable members other than the tile ZList at +0x3C (objects +0x50, tagged objects +0xB8, backs) are not
  mapped.
- Whether Custom.wz merge reaches `Effect/BasicEff.img/No*` sub-properties.
- Whether IWzGr2DLayer::color alpha survives stock animation, whether canvas origin is honoured for a scaled damage
  canvas, and CopyEx filter quality.
- The owner field offsets in pet and summon objects.
