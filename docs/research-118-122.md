# Research: tickets 118 (EXP tracker + DPS meter) and 122 (pet auto-potion window)

Date: 2026-10-28. Read-only research. Nothing was edited, built, run or committed.

Sources read:
- Client: /root/work/client/src-161-qa (branch client-161-qa, VERSION 1.6.0).
- Exe: scratchpad/client/MapleStory.exe, sha256 86e4e6be050caeda4a93443237256a84376ea74781fcba558d2418f0568aac90, image base 0x400000.
  Every address below marked "bytes" was checked with `objdump -d -M intel` on this file.
- Names: /root/Maplestory/v83.idb has almost no function names. **/root/Maplestory/Angel.idb is a named v83 database
  with the same addresses** (mobdamage_patch.h already cites it). Every function name below comes from it (python-idb,
  `idb_names.py /root/Maplestory/Angel.idb <regex>`). Its mangled signatures look borrowed from a later client (CMob::OnHit
  says 14 parameters, the code takes 13), so the parameter counts below come from the `ret` immediates, not the names.
- StringPool decoded with scratchpad/tools/stringpool.py.
- Server: /root/work/SlumsMS-1602 (branch release-1.6.0.2). Opcode registry: /root/work/addon-opcodes.md.

Shared HUD facts (both tickets):
- Modules are rows in `g_widgets` (src/overlay.cpp:59-63). Struct `Widget` is at src/overlay.cpp:41-53. Prefs are stored as
  `<id>_on/_x/_y/_w/_h/_text/_alpha` DWORDs under HKCU\Software\SlumsMS\Overlay (src/overlay.cpp:24, 81-115).
- A frozen HUD (the normal state) draws modules with `ImGuiWindowFlags_NoInputs` (src/overlay.cpp:259-262). **A module
  cannot have buttons or sliders while frozen.** Interactive UI belongs in the Ctrl+F10 window
  (src/settings_overlay.cpp:13-26 -> `Overlay_DrawSettings`, src/overlay.cpp:307), or in a separate window that
  d3d9host.cpp draws while it is open (the way `g_bVisible`/SettingsOverlay_Draw work, src/d3d9host.cpp:160-182, 269-294).
- `LoadPrefs` defaults every module to on (`ReadPref("<id>_on", 1)`, src/overlay.cpp:90). New modules that should start
  hidden need a `defOn` field in `Widget`. This is a small change to overlay.cpp.
- Module height is fixed by `rows` (src/overlay.cpp:45, 263-269). A multi-line DPS module needs `rows` = 4-6.
- Header for HUD entry points: src/d3d9host.h:23-26. Pure layout maths and tests: src/overlay_policy.h,
  tests/overlay_policy_test.cpp.
- The HUD and its D3D9 host are only built with MAPLEROOT_INTEGRATION (src/CMakeLists.txt:92-97). Hooks are installed
  in src/hook.h:225-272. With integration on, `VerifyKaentakeHooks` after each Attach*Mod is FATAL (src/mapleroot.cpp:30-35).
  So check the stock bytes before calling ATTACH_HOOK and skip the feature on a mismatch, as hud_vitals.cpp:42-60 does.
- The game draws in EndScene on its main thread (src/d3d9host.cpp:184-187). The client's packet and mob-update code also
  runs on that thread, so the hook and the draw do not race. UNVERIFIED: no thread-id check was done. A cheap lock or a
  one-time thread-id assert in the scaffold costs nothing.

---------------------------------------------------------------------------------------------------------------------

## 118: EXP tracker and DPS meter

### Summary
- **EXP tracker: client only, easy.** The 64-bit EXP and the level are already readable with code we have. Sample them
  once per frame in a pure "EXP rate" policy that handles level-ups, death loss and character changes.
- **DPS meter: one new detour.** Hook `CMob::OnHit` (0x00668B83). It is the single place where every damage number shown
  on a mob is applied, local and remote. Its first argument is the attacker's character id and its second the skill id,
  so filtering for the local character id gives own damage per skill.
- **Party ranks: a small server packet.** The server already sums damage per attacker on every mob
  (`Monster.takenDamage`), and `Monster.damageSnapshot()` already exists. Add a throttled LP 0x3739 (next free custom
  opcode) to party members on the boss's map.

### Existing code to extend
- src/overlay.cpp:59-63 `g_widgets`: add `{"exptrack", ...}` and `{"dps", ...}`. The id "exp" is already the EXP bar, so
  do not reuse it. src/overlay.cpp:41-53 `Widget`: add `defOn`. src/overlay.cpp:90: use it.
- src/d3d9host.h:23-26: declare `HudMeters_DrawExp()` and `HudMeters_DrawDps()`.
- src/hud_vitals.cpp:63-87 `SehReadVitals` shows the SEH-guarded read of CWvsContext/GW_CharacterStat.
  src/hud_vitals.cpp:128-133 is the EXP read through `MapleRoot_CurrentExp()` / `MapleRoot_NextLevelExp()` (src/mapleroot.h:9-10).
- mapleroot/ijl15/MapleHook.cpp:366-373 bridges EXP. mapleroot/ijl15/SkillEdits/CharacterDataEx.h:121-128 shows how the
  64-bit EXP is filled. `ExpSwap__Decode4To8` is patched into GW_CharacterStat::Decode (call at 0x004E2C6E) and into
  DecodeChangeStat (call at 0x004E31B6), so the EXP value changes only when a stat packet arrives. Polling it once per
  frame is enough.
- src/hud_native.cpp:285-311 `ReadWho`: reads the level as a secured byte, GW_CharacterStat +0x33 (key at +0x35) through
  `kAddr_FuseByte`.
- src/packetdispatcher.cpp:32-91: add a `#ifdef MAPLEROOT_HUD_METERS` branch for the new LP opcode, like bounce/weather.
- src/hook.h:225-272: add `AttachHudMetersMod()` under the new option.
- Server: server/life/Monster.java:93 (`takenDamage`), :437-460 (`applyDamage`; true damage per attacker, `fake` hits
  count as 1), :488-505 (`damageSnapshot()`), :368-385 (`broadcastMobHpBar`; the party fan-out pattern to copy).
  client/Client.java:1262 has `getClientBuild()` (CP 0x3738 build stamp). net/opcodes/SendOpcode.java:25-32 lists the
  custom LP opcodes.

### Client hook points (bytes verified on the exe)
| What | Address | Entry bytes | Convention / notes |
|---|---|---|---|
| CMob::OnHit | 0x00668B83 | `b8 94 17 aa 00` `e8 0b 80 3f 00` (mov eax, SEH; call __EH_prolog) | `__thiscall` with ecx = CMob*. 13 stack args (`ret 0x34` at 0x0066918C). a1 = attacker character id (`cmp [ebp+8],[CUserLocal+0x11A8]` at 0x00668BF5-0x00668BFB). a2 = skill id (compared with 1121001/1221001/1321001/5121004... at 0x00668DB4-0x00668DED). **a5 = damage** and a6 = critical flag: both are passed to ShowDamage at 0x00668E01-0x00668E0D, and damage < 0 skips the number at 0x00668DF4. The only caller is 0x006681CF in the mob's damage-info queue; it unpacks ZtlSecure fields from the queued record (a1 via 0x004E8152 from rec+0x0C, a2 from rec+0x18, a5 from rec+0x3C, a6 from rec+0x48). |
| CMob::ShowDamage | 0x006691D3 | `53 56 57 8b f1` | `__thiscall`, (damage, a7, crit, isSkill5121004). It has no attacker id, so **do not use it for the meter.** It is the natural hook for ticket 119 (short damage numbers). Keep 118 on OnHit so the two tickets never stack detours on the same function. |
| CUserLocal singleton | 0x00BEBF98 | data | Character id is at CUserLocal+0x11A8 (used by OnHit above); the name is at +0x11AC (src/attackdiag.cpp:33). |
| CUserLocal::GetCharacterLevel | 0x00949B15 | `55 8b ec 51 51` | `__thiscall`, returns BYTE. MapleRoot already calls it (CharacterDataEx.h:62-70). hud_native's +0x33 secured read is the alternative. |
| CWvsContext::GetCharacterData | 0x00425D0B | `55 8b ec 51 8b 81 b8 20` (checked in hud_vitals.cpp:53) | already used by the HUD |
| Alternative: CClientSocket::SendPacket | 0x0049637B | `b8 6c 12 a8 00` `e8 13 a8 5c 00` | `__thiscall(COutPacket&)`, `ret 4`. Not hooked anywhere today (it is only called, e.g. src/bypass.cpp:413). One could parse outgoing CP 0x2C/0x2D/0x2E/0x2F/0xB0 attack packets instead. Not recommended: the parser must follow MapleRoot's packet quirks, and summons/delayed hits would need separate handling. |

Angel.idb also names these related functions: CMob::OnDamaged 0x0066C6C2 (calls ShowDamage at 0x0066C6E4),
CUserLocal::TryDoingMeleeAttack 0x00950921, TryDoingShootAttack 0x009537D5, TryDoingMagicAttack 0x0095571F,
CalcDamage::PDamage 0x0078DF87 (MapleRoot already detours it), MDamage 0x00791617, CSummoned::TryDoingAttack 0x007A5C99,
and CAnimationDisplayer::Effect_HP 0x00437D0F.

UNVERIFIED:
- Whether summon, puppet and DoT/poison ticks reach OnHit with a1 = owner id. Summon attacks probably queue through the
  same damage-info path, and poison is likely CMob::OnDamaged/other. Check on TEST with a summon class and Poison Mist.
- The exact meaning of a3, a4 and a7-a13. Not needed.
- Skill names. `CSkillInfo::GetSkill` is at 0x0075C755 with the singleton at 0x00BE78DC (src/attackdiag.cpp:24,28), but
  no SKILLENTRY name offset was verified. Use String.wz Skill.img/<id>/name through resman, or show "Skill 1121008" first.

### Server work (party ranks)
- New `server/life/BossDamageRanks.java` (or similar):
  - `onHit(Monster m, Character from)` is called from `Monster.applyDamage` when `!fake && m.isBoss()`. It sends at most
    one update per mob per second (store the last-sent timestamp per oid) and a final update from the mob's death path
    (killBy/`dispatchMonsterKilled`).
  - Recipients: the attacker's party members on the same map whose `getClient().getClientBuild() >=` the shipping build.
    Later, expedition members if `getExpedition()`/EventInstanceManager applies. UNVERIFIED how expeditions are exposed
    in this fork.
  - Data: `m.damageSnapshot()`, filtered to the recipients' party member ids, plus names from the map.
- New LP **0x3739 BOSS_DAMAGE_RANK** (server->client). /root/work/addon-opcodes.md lists 0x3739..0x373B as free. A grep
  of SendOpcode/RecvOpcode, the 170-scaffolds and kuro-tickets repos and the client found no user. Add the line to
  addon-opcodes.md.
  - Wire format v1: `short format=1, int mobOid, int mobId, long mobMaxHp, byte flags (bit0 final), byte n,
    n x {int charId, string name, long damage}`, with n capped at 30.
  - Golden tests on both sides: a Java packet test and tests/bossrank_proto_test.cpp on the same bytes, as was done for
    CP 0x3738.
- Gate on the build stamp. Old clients never get the packet. UNVERIFIED whether the stock client tolerates an unknown
  opcode: packetdispatcher.cpp swallows ours before stock dispatch, but an old slums.dll does not.
- Mismatch to document: the server counts true applied damage (`clientDamage(...)` caps, AbstractDealDamageHandler.java
  :581/:827, and HP clamping), while the client meter counts the numbers shown. Rank = server numbers; own DPS = client
  numbers.

### Data needed
- Local: EXP (int64, `MapleRoot_CurrentExp`), next-level EXP (`MapleRoot_NextLevelExp`, 0 outside the field), level
  (byte), character id (CUserLocal+0x11A8, to reset the session on a character change), and `GetTickCount64` time.
- DPS: per OnHit with a1 == local id: skill id (a2; 0 = basic attack), damage (a5, skip if < 0), critical flag (a6).
- Ranks: the LP 0x3739 payload above.

### Design
- **EXP tracker module** ("exptrack", 3 rows):
  1. EXP/h over a selectable window of 5, 15 or 60 minutes. The window is chosen in the Ctrl+F10 window, because the
     module itself takes no input.
  2. Time to level: (next - exp) / rate, shown as "--" when the rate is 0.
  3. EXP gained this session.

  Policy `ExpTracker::Sample(nowMs, charId, level, exp, nextExp)`:
  - Same level: gained += max(0, exp - lastExp).
  - Level went up by one: gained += (lastNext - lastExp) + exp. Several levels at once, or MapleRoot's custom table:
    add (lastNext - lastExp) + exp and accept being off by the skipped levels' tables. The policy can be given an
    EXP-table lookup later.
  - EXP went down (death penalty or quest): record a loss, not a gain.
  - Character id changed or nextExp == 0: pause, and reset on a new character.

  Keep a ring of (t, cumulative) samples. Take one sample when EXP changes, plus one per minute, so the 60-minute window
  needs at most ~200 entries.
- **DPS meter module** ("dps", ~5 rows):
  - Current DPS, using an "encounter" model: it starts on the first hit and ends after 8 s with no hits; DPS = encounter
    damage / encounter seconds.
  - Session total, then the top 3 skills (name or id, share in %, hits).
  - If an LP 0x3739 update arrived in the last 10 s, extra rows "Party: 1. Name 45% ...". Or put ranks in a third module
    "dpsparty" so solo players keep the meter small. Recommended: the third module, defaulting off.
  - The OnHit detour does no allocation: it pushes {tick, skill, dmg} into a fixed ring (e.g. 4096 entries) under the
    lock. The draw call folds it into per-skill totals (`std::unordered_map<int, Agg>` with a cap of 64 skills).
- Both modules default off (`defOn=false`) and are switched on in Ctrl+F10. Edit UI move/resize works as is because
  layout is generic in overlay.cpp. Resolution changes are already handled (src/overlay.cpp:236-241).

### Risks
- A fatal hook failure under integration (see Shared). Byte-check 0x00668B83 first and skip the meter on a mismatch.
- OnHit fires when the number is shown. That is after the attack delay (`CMob::GetRemainDamageInfoDelay` 0x00663937), so
  DPS trails by up to ~1 s. Fine for a meter.
- A dead or removed mob: queued hits for a mob that leaves the field may never reach OnHit, which undercounts slightly.
- The client numbers are not the server's (caps, MapleRoot damage hooks). Label the meter "your hits".
- MapleRoot's critical-display caves (dCrits 0x007650AF, MapleHook.cpp:341-345) change how criticals look, not the OnHit
  arguments. UNVERIFIED. Damage over 2^31 is not possible here: the server reads int damage
  (AbstractDealDamageHandler.java:581).
- Ticket 119 will likely detour ShowDamage. Keep 118 on OnHit (see the hook table).
- Per-frame cost: one SEH read (already done for vitals; reuse `FrameVitals`-style caching), a few `snprintf` calls and
  a 4096-entry fold. Well under 0.2 ms. Measure with QueryPerformanceCounter around both draw functions in a debug build.
- Server: the throttle state per mob must be cleared on mob removal to avoid a leak. Use a WeakHashMap or remove it on
  death.

### Scaffold plan
- CMake: `option(MAPLEROOT_HUD_METERS "EXP tracker and DPS meter HUD modules (party ranks need matching server support)" OFF)`
  in CMakeLists.txt next to line 14. In src/CMakeLists.txt, inside `if(MAPLEROOT_INTEGRATION)`, add `if(MAPLEROOT_HUD_METERS)
  target_sources(injector PRIVATE hud_meters.cpp) target_compile_definitions(injector PRIVATE MAPLEROOT_HUD_METERS)`.
- config.ini: `[clienthax] hudmeters=true` (default true when built; false turns off the hook and the modules). Document it
  in README and config.example.ini, using the ReadIniBool pattern from src/d3d9host.cpp:41-45.
- New files:
  - `src/hud_meters_policy.h`: pure, no Windows/ImGui. `ExpTracker` (Sample, RatePerHour(windowMs), TimeToLevelMs,
    SessionGained), `DpsMeter` (AddHit, Fold, EncounterDps, SessionTotal, TopSkills(n)), `FormatShort(int64)` (K/M/B, which
    ticket 119 can share), `BossRank::Parse(const uint8_t*, size_t, Rank&)` with format checks.
  - `src/hud_meters.cpp`:
    - `AttachHudMetersMod()`: checks the bytes at 0x00668B83 and ATTACH_HOOKs `CMob__OnHit_hook`. Signature:
      `void __fastcall(void* mob, void*, unsigned long charId, int skillId, int a3, int a4, int damage, int crit,
      int a7..a13)`, which forwards all 13 arguments.
    - `HudMeters_DrawExp()`, `HudMeters_DrawDps()`, `HudMeters_DrawParty()`.
    - `HudMeters_HandleRank(CInPacket*)`.
    - `HudMeters_DrawSettings()`, called from `Overlay_DrawSettings`: window length, reset session.
  - `tests/hud_meters_policy_test.cpp`: level-up carry, death loss, character switch, window maths, encounter idle cut,
    top-N ordering, FormatShort.
  - `tests/bossrank_proto_test.cpp`: golden bytes shared with the server test. Register both in tests/CMakeLists.txt.
- Edits: src/overlay.cpp (Widget.defOn, three rows), src/d3d9host.h (declarations), src/packetdispatcher.cpp (0x3739
  branch), src/hook.h (install), README, config.example.ini.
- Server: SendOpcode `BOSS_DAMAGE_RANK(0x3739)`, `BossDamageRanks.java`, `BossDamageRankPackets.java` plus a golden test,
  a hook in `Monster.applyDamage` and the death path, and a config flag `BOSS_DAMAGE_RANKS_ENABLED` (default false) in
  ServerConfig/config.yaml.

### Test plan on TEST
1. Native tests: `ctest` on Silvia (policy and golden). Server: `mvn test` for the packet golden and a throttle unit test.
2. Copy the RC client under Z:\MSv83\SlumsMS-dev\ and enable the modules in Ctrl+F10.
   - EXP: kill mobs and check EXP/h rises and the session total matches the EXP bar delta. Level up once (e.g. with a GM
     EXP command) and check there is no negative spike. Die, and check the loss is not counted as gain. Change channel
     and relog, and check the session resets on a character change only.
   - DPS: hit a training dummy or mob with two skills and check the top-skills split. Stand next to another player or bot
     attacking, and check their damage is NOT counted. Check summons (UNVERIFIED path).
   - Layout: Edit UI move/resize, then 800x600 -> 1920x1080. The modules keep their place.
   - Perf: debug-log the average draw time over 600 frames.
3. Party ranks: two TEST characters in a party on a TEST boss map. Both see ranks within ~1 s and a final update on the
   kill. A third non-party player sees nothing. An old client (build < gate) gets no packet (server log).

### Effort
EXP tracker ~0.5 day. DPS meter plus hook ~1 day. Server ranks plus goldens ~1 day. TEST pass ~0.5 day. Total **M (~3 days)**.
The ticket says S-M; the party-rank part is what makes it M.

---------------------------------------------------------------------------------------------------------------------

## 122: Pet auto-potion settings window

### Summary
In v83 the pet auto-potion is split into two settings, both reachable through stock functions:
- **Which potion:** a per-character item id pair in `CFuncKeyMappedMan` (+0x3C0 HP, +0x3C4 MP). It is set by dragging a
  potion onto the pet HP/MP slot in Key Config. That calls `CFuncKeyMappedMan` setters, which send **CP 0x87
  CHANGE_KEYMAP {int mode=1|2, int itemId}**. The server stores the id in the keymap as key 91/92 (type 7), saves it in
  the DB, and sends it back at login as **LP 0x150/0x151 {int itemId}**. This survives relog.
- **When:** the threshold is the **HP/MP alert ("flash") setting of the System Options window**: CConfig +0x80
  (`soHPFlash`) and +0x84 (`soMPFlash`), in 5% steps (value 0..19, default 10 = 50%, 0 = off). It is a GLOBAL client
  option saved in the registry by `CConfig::SaveGlobal`, shared by every character on that PC, and it also drives the
  status bar's low-HP flash. **The server never receives the threshold.** The client decides when to send CP 0xAB
  PET_AUTO_POT. The server only infers an alert from when the request arrives.

So the window needs **no server work**. It reads and writes these two stock settings through the stock functions.

### Existing code to extend
- src/wvs/config.h:6-28: `CConfig` singleton at 0x00BEBF9C, with `GetOpt_Int`/`SetOpt_Int`, MEMBER_HOOKs for
  `SaveGlobal` 0x0049C8E7 and `ApplySysOpt` 0x0049EA33. Add `MEMBER_AT(int, 0x80, m_nHPAlert)`,
  `MEMBER_AT(int, 0x84, m_nMPAlert)`, `MEMBER_AT(int, 0x18, m_nPetConsumeHPItemID)`, `MEMBER_AT(int, 0x1C, ...MP)`.
- src/resolution.cpp:164-171, 699-700 already detours `SaveGlobal` and `ApplySysOpt` (pass-through plus resolution). Calling
  the originals through those detours is fine.
- src/movementkeys.cpp:15, src/coloringprism.cpp:323-330 and src/cashshopwnd.cpp:1425-1439 already use
  `CFuncKeyMappedMan` at 0x00BED5A0.
- Inventory reading for the potion list: reuse the USE-inventory walker from an existing addon (cashshopwnd.cpp,
  coloringprism.cpp or monsterbook*.cpp all read inventories; pick whichever already exposes item id + count).
- src/settings_overlay.cpp:13-26 and src/overlay.cpp:307-372 are where the editable controls go. Optionally add a
  read-only status module in `g_widgets`.
- Server, read-only, nothing to change:
  - net/server/channel/handlers/KeymapChangeHandler.java:66-80: mode 1/2. It **disconnects** if itemId != 0 is not in
    USE inventory.
  - net/server/channel/handlers/PetAutoPotHandler.java:35-63: CP 0xAB; reads byte, long, int, short slot, int itemId.
  - client/processor/action/PetAutopotProcessor.java: consumes one potion (USE_COMPULSORY_AUTOPOT is not set in
    config.yaml, so it defaults to false).
  - client/Character.java:10527-10557: server-side autopot on HP loss, using keymap 91/92 and its inferred alert.
  - net/server/channel/handlers/PlayerLoggedinHandler.java:250-255: login sends 0x150/0x151.
  - client/Character.java:1482-1487 `changeKeybinding`, :9637 keymap saved to the DB.

### Client hook points / calls (bytes verified on the exe)
| What | Address | Entry bytes | Convention / notes |
|---|---|---|---|
| CFuncKeyMappedMan singleton | 0x00BED5A0 | data | +0x3C0 pet HP item id, +0x3C4 pet MP item id (zeroed in the ctor at 0x0058DD6F/0x0058DD76) |
| set pet HP potion (name UNVERIFIED; in Angel.idb's CFuncKeyMappedMan block) | 0x0058E03C | `b8 50 f4 a8 00` `e8 52 2b 4d 00` | `__thiscall(int itemId)`, `ret 4`. Writes +0x3C0, then sends COutPacket(0x87){Encode4(1), Encode4(itemId)} through SendPacket 0x0049637B (0x0058E04F-0x0058E088). Stock callers are in CUIKeyConfig (0x007FF0F9, 0x007FF2E5, 0x0080163C). |
| set pet MP potion | 0x0058E0A8 | `b8 64 f4 a8 00` `e8 e6 2a 4d 00` | the same, writing +0x3C4 with mode 2 (0x0058E0BB-0x0058E0F4) |
| CFuncKeyMappedMan OnPacket switch | 0x0058DE79 | `8b 44 24 04 2d 4f 01 00 00` | `(nType, CInPacket*)`, `ret 8`: 0x14F -> OnInit 0x0058DDB4, 0x150 -> 0x0058DE2D (Decode4 into +0x3C0; 0 falls back to CConfig+0x18), 0x151 -> 0x0058DE53 (+0x3C4; fallback CConfig+0x1C) |
| CUserLocal::TryConsumePetHP | 0x0095B9A4 | `b8 4c ed ad 00` `e8 ea 51 10 00` | `__thiscall(int nDelta, int bForce, uchar)`, `ret 0xC`. Fires when (hp - nDelta)*100/maxHp < CConfig[+0x80]*5 (0x0095BA30-0x0095BA57), or when bForce is set. Item = CFuncKeyMappedMan+0x3C0 (0x0095BA62-0x0095BA67). Needs pet[i] (CUserLocal+0x1EDC, 3 slots, stride 8) with a secured flag at pet+0x164/+0x16C (0x0095BA86-0x0095BA92). Sends through 0x00A0955C. |
| CUserLocal::TryConsumePetMP | 0x0095BBE0 | `b8 60 ed ad 00` `e8 ae 4f 10 00` | the same, with CConfig+0x84, item +0x3C4 and pet flag +0x170/+0x178 (0x0095BC59-0x0095BCE3) |
| send PET_AUTO_POT (CWvsContext method) | 0x00A0955C | `b8 f4 9b ae 00` `e8 32 76 05 00` | `__thiscall`, 7 args (`ret 0x1C`). Builds COutPacket(0xAB): pet SN (8 bytes), Encode1, Encode4(tick), Encode2(slot), Encode4(itemId) (0x00A095B5-0x00A0960D). Cosmic reads byte, long, int, short, int, a different order for the first two fields, but it ignores them. |
| CConfig::LoadGlobal | 0x0049C441 | `b8 a4 16 a8 00` `e8 4d 47 5c 00` | Loads `soHPFlash` -> +0x80 and `soMPFlash` -> +0x84 with GetOpt_Int(GLOBAL_OPT, key, default 10, 0, 19) (0x0049C7B1-0x0049C81A). Keys are StringPool 0xB62/0xB63. |
| CConfig::SaveGlobal | 0x0049C8E7 | `b8 30 17 a8 00` `e8 a7 42 5c 00` (stock; detoured by resolution.cpp at runtime) | `__thiscall()`. Writes +0x80/+0x84 back (0x0049CBCF, 0x0049CC10). |
| CConfig::ApplySysOpt | 0x0049EA33 | `55 8b ec 53 56` (stock; detoured by resolution.cpp) | `__thiscall(CONFIG_SYSOPT*, int bApplyVideo)`, `ret 8`. Copies 12 dwords into CConfig+0x64..+0x93, which includes +0x80/+0x84 (0x0049EA42-0x0049EA48). CUISysOpt::SetRet (0x009949E1) calls ApplySysOpt(..., 1) and then SaveGlobal (0x00994A1A, 0x00994A21). |
| CConfig::LoadCharacter | 0x0049D0B6 | (already a MEMBER_HOOK in config.h) | Loads `PetConsumeItemID` (StringPool 0xD51) -> +0x18 and `PetConsumeMPItemID` (0xD53) -> +0x1C from the per-character option store (0x0049D6CE-0x0049D73A). Local fallback only. |

StringPool texts the window can reuse:
- 0xD6B "Your pet will consume the Potion once when your HP goes below HP alert setting."
- 0xD6C (the MP version)
- 0xD69/0xD6A "You are lacking the HP/MP Potion that your pet is supposed to use."
- 0xB1F/0xB20 "Only usable on Auto HP/MP Potion Pouch."

UNVERIFIED:
- What the pet flag at pet+0x164 (HP) and +0x170 (MP) is. It is read through 0x00508A95, a ZtlSecure fuse, and is most
  likely "this pet has the Auto HP/MP Potion Pouch ability". The string 0xB1F suggests pouch items gate it.
- The CONFIG_SYSOPT field order beyond +0x80/+0x84. Writing +0x80/+0x84 directly and calling SaveGlobal avoids needing it.
- The setter names (Angel.idb does not name 0x0058E03C/0x0058E0A8; the behaviour above is from disassembly).
- How the stock potion filter (which items Key Config accepts) decides. The candidate is CWvsContext::IsAbleToConsume
  0x00A10579, called from the send path. To list potions, use Item.wz spec hp/mp/hpR/mpR through the existing item-info
  helpers. The exact helper address was not checked.

### Server work
None required. Optional, separate hardening tickets:
- PetAutoPotHandler does not check that a pet with a pouch is out. It trusts the slot and itemId (it does check the item
  id matches the slot).
- KeymapChangeHandler disconnects on a missing item. Our UI must only offer items the character holds.

### Data needed
- Read: CConfig+0x80/+0x84 (alert steps), CFuncKeyMappedMan+0x3C0/+0x3C4 (item ids), USE-inventory item ids, counts and
  names, item spec hp/mp, and whether an active pet has the HP/MP pouch flag (for a hint line).
- Write: the alert step through CConfig fields plus `SaveGlobal()`. The item through 0x0058E03C/0x0058E0A8, which also
  updates the server.

### Design
- **"Pet potions" section in the Ctrl+F10 window**, placed above or below the HUD module table; or a separate small
  window opened from a button there, which is closer to the ticket's "small HUD window". It needs input, so it cannot be
  a frozen module.
  - Per HP/MP: a combo of USE potions with HP (or MP) effects, showing name and count, plus "None".
  - A slider "Use below 0-95%" in steps of 5; 0 = off. Under it: "Shared with the low-HP flash in System Options; applies
    to every character on this PC."
  - A status line: the pet/pouch state, and "Out of <potion>" when the count is 0.
  - Apply on change:
    - Item: `((void(__thiscall*)(void*, int))0x0058E03C)(*(void**)0x00BED5A0, itemId)`. The server stores it, so it survives
      relog.
    - Threshold: write CConfig+0x80 = pct/5, then call `CConfig::SaveGlobal(this)`. Only when the value changed, not every
      frame, because it writes the registry.
- Optional read-only module "petpot" (2 rows): "HP: Elixir x123 < 50%" and "MP: ...". Default off.
- Out of scope unless the owner asks: a per-character or 1%-step threshold. That would need hooking the compare inside
  TryConsumePetHP/MP (CConfig reads at 0x0095BA3F/0x0095BC89) and the server's inferred alert would then drift.

### Risks
- **Disconnect risk:** sending CP 0x87 mode 1/2 with an item not in USE inventory gets the player disconnected by the
  server. Re-check the inventory right before calling the setter; 0 is always safe.
- The threshold is global and coupled to the low-HP flash. Players may be surprised. Say so in the UI.
- 5% granularity (stock).
- Calling SaveGlobal runs resolution.cpp's SaveGlobal detour, which also saves the resolution. Harmless, but test it.
- ImGui text input while chatting: the window takes keyboard input only when focused (src/d3d9host.cpp:287-293).
- Calling client functions from EndScene: the same thread as the game (UNVERIFIED, see Shared), and the same practice as
  hud_vitals. Wrap in SEH.

### Scaffold plan
- CMake: `option(MAPLEROOT_PET_POTIONS "Pet auto-potion settings window in the HUD (stock settings, no server change)" OFF)`.
  Inside the integration block add `pet_potions.cpp` and the define `MAPLEROOT_PET_POTIONS`.
- config.ini: `[clienthax] petpotions=true` (document it in README and config.example.ini).
- New files:
  - `src/pet_potions_policy.h`: pure.
    - `AlertStepFromPercent(int pct)` (clamp 0..95, round to 5) and `PercentFromStep`.
    - `ClassifyPotion(hp, hpR, mp, mpR) -> {Hp, Mp, Both, None}`.
    - `CanOffer(itemId, count)` (count > 0, item id in the USE range 2000000-2099999).
    - `EncodeKeymapPotion(mode, itemId) -> bytes` (0x87, int, int) for a golden test that matches
      KeymapChangeHandler's parse.
    - `ShouldConsume(hp, maxHp, delta, step)`, which mirrors the stock compare, so the UI hint matches when the pet
      actually drinks.
  - `src/pet_potions.cpp`:
    - `PetPotions_Setup()` checks the bytes at 0x0058E03C, 0x0058E0A8, 0x0049C8E7 (stock or E9) and 0x0095B9A4.
    - `PetPotions_DrawSettings()`, called from Overlay_DrawSettings, plus an optional `PetPotions_DrawModule()`.
    - SEH-wrapped readers for the CConfig and CFuncKeyMappedMan fields, and the USE-inventory list (reused helper).
  - `tests/pet_potions_policy_test.cpp`, registered in tests/CMakeLists.txt.
- Edits: src/wvs/config.h (MEMBER_ATs), src/overlay.cpp (call the settings section and optional module row),
  src/d3d9host.h (declarations), README, config.example.ini.

### Test plan on TEST
1. `ctest` (policy test) on Silvia.
2. On a TEST character with a pet and an Auto HP/MP Potion Pouch:
   - Pick Red Potion for HP and set 60%. Take mob damage below 60% and check the pet drinks. Check the server log or the
     item count drop.
   - Set 0% and check the pet never drinks.
   - Open stock System Options and check its HP alert slider shows the same value. Check Key Config's pet HP slot shows
     the chosen potion.
   - Relog and change channel: the potion comes back from server LP 0x150 and the threshold from the registry.
   - Pick an item, then drop or sell the whole stack. The combo must no longer offer it. Confirm there is no disconnect
     path: the item is re-checked before the send.
   - Without a pouch, the hint shows and nothing is consumed.
3. A second character on the same PC sees the same threshold (expected: global) and its own potion.

### Effort
**S (~1.5-2 days)**: UI ~0.5 day, inventory list and potion classification ~0.5 day, policy and tests ~0.25 day, TEST
~0.5 day. No server change.
