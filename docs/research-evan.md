# Evan on SlumsMS: feasibility (read-only research, 2026-10-28)

## Summary

- **The stock v83 exe already has Evan code. It is switched off, not missing.** It names jobs 2001 and 2200-2218 "Evan0".."Evan10". It decodes per-tier extended SP for Evan jobs. It has a full Mir dragon object (spawn and move packets 0xB5/0xB6, the object, action loading from `Skill/Dragon/<job>.img`, following). It has the Evan skill window (`UI/UIWindow.img/SkillEx`, with the dragon's growth images and tabs), the dragon equip window, dragon equip items, and Evan skill actions. The one big thing it lacks is Evan **character creation**: the login screen only has Knight, Aran and Adventurer.
- **What is missing is data, plus the server remap.** Our client has no Evan skill images, no `Skill/Dragon`, no `SkillEx` UI node, and 17 Evan body actions are absent. The server moved Evan to ids 9900-9918. Those ids fall outside every Evan check in the exe, so the remap turns Evan **off** on the client. The server is also inconsistent with itself after the remap: `getSkillBook` still uses 2210-2218, while `hasSPTable` sends extended SP for 99xx and the client decodes that as a plain short.
- **Clean-ish data is already on the box.** `/root/maplestory_dump/Evan.zip` holds an XML export with Skill/2001 and 2200-2218, Skill/Dragon/2200-2218, Character/Dragon (5 items per slot), a body with the Evan actions, and a UIWindow.img that has SkillEx. Where it came from is unknown (its 2001.img has skills 20011037-39, so it is from a version after v83). The skill ids match the server's `constants/skills/Evan.java` exactly (pre-Big Bang layout). It is not from PlayKuro. PlayKuro has 2001.img (with custom 20011032-20011122) but **no 22xx and no Skill/Dragon**.
- **Recommendation:** use native Evan with the real ids 2200-2218 and real data. Do not build a fake dragon. Estimated size: **client DLL small (0.5-3 days, mostly verification; +1-2 days if we want an Evan button on the creation screen)**, **data medium (2-4 days: convert and merge Evan.zip into the v83 img files, about 40-60 MB added to the patch)**, **server medium-large (1-2 weeks: undo the remap, a creation path, Evan quests and NPCs, skill effects, SP books, bots, site and leaderboards, QA)**.

## 1. Client data Evan needs, and what we have

| Need | Ours (MapleRoot-based repack, our client's Data) | Source on box |
|---|---|---|
| Skill/2001.img (Evan beginner) | absent | Evan.zip `Evan/Skill/2001.img.xml` (8 MB) |
| Skill/2200, 2210-2218.img | absent | Evan.zip (2200: 22000000, 22001001 ... 2218: 22181000-22181003; same as server Evan.java) |
| Skill/Dragon/2200-2218.img (Mir's body and actions per growth stage) | absent | Evan.zip `Skill/Dragon/*.img.xml` (~41 MB XML). Actions: stand, move, dragonBreathe, magicmissile, fireCircle, blaze, ... Awakening |
| UI/UIWindow.img/SkillEx (Evan skill window), Equip/DragonEquip, Equip/BtDragonEquip | absent (checked MapleRoot Full Repack UIWindow.img: no SkillEx; Equip children have no Dragon*) | Evan.zip `UI/UIWindow.img.xml` has SkillEx (with Dragon, egg). Merge only those nodes; don't replace our UIWindow |
| Character/00002000.img Evan body actions | 158 actions; missing 17: Awakening, Earthquake, OnixBlessing, blaze, darkFog, dragonThrust, fireCircle, flameWheel, ghostLettering, killingWing, lightingBolt, magicRegistance, magicShield, mapleHero, recoveryAura, slow, soulStone | Evan.zip 00002000.img.xml (175 actions). Copy only the missing action nodes. The head (00012000) may need the same check (unverified) |
| Character/Dragon equips | 0194/0195/0196/0197 2000-2002 present | Evan.zip adds xxx2003, xxx2004 |
| String/Skill.img: 2001xxxx and 22xxxxxx names | absent (only 2001002-2001005, which are Magician skills) | not in Evan.zip. Needs a String source (see below) |
| Sound/Skill.img Evan entries, Effect/BasicEff.img/dragonEyes, String/Eqp dragon items | unverified | not in Evan.zip |
| Evan intro maps 100030000-100030400, NPCs 1013000-1013105 | **present** in the repack (v83 data already shipped them) | already have |
| Server-side XML: Skill.wz 2001 and 22xx | absent (no 2218.img.xml anywhere on the box) | Evan.zip XML can be dropped straight into the server's wz (it is already XML) |

Clean sources: GMS shipped Evan in a post-v83 patch. I believe this was around v84-v88 and the pre-Big Bang layout lasted to about v92 (**the exact version is unverified**). Take Skill, Skill/Dragon, the SkillEx UI and String/Skill from a pre-Big Bang GMS client (the v88-v92 era) or from an open repack of that era. Big Bang (v93+) changed Evan's skills. Evan.zip's ids match pre-Big Bang, so it is a usable base. Strings and sounds still have to come from the same era. We should look for property differences that the v83 client might reject (images in a newer format, new property types) when converting. Not checked.

## 2. What the v83 exe can and cannot do

All addresses are in `MapleStory.exe` (base 0x400000). Strings were decoded with `scratchpad/tools/stringpool.py`. The disassembly is at `scratchpad/evan/text.asm`.

| Area | Evidence | Status | DLL work |
|---|---|---|---|
| Job names | `get_job_name` 0x4A77EF builds a map: 0x4A8985 2001→"Evan0" (SP 0x152E), 0x4A89BF 2200→"Evan1" ... 0x4A8BC9 2218→"Evan10" | native | 0. MapleRoot's hook (`mapleroot/ijl15/YourStuff.h:2955`) falls through to the original for these ids |
| Is-Evan checks | inline `job/100==22 \|\| job==2001` at 0x4B234C (CUser vfunc, matches `CAvatar::IsEvanJob` in `src/wvs/avatar.h:67`), 0x4E2C18, 0x4E3179, 0x4E8F19, plus about 40 more `cmp ...,0x7D1` sites (0x4F1D2F, 0x7FE188, 0x8BA643 ...). 2001 is treated as a beginner job (0x52FD60-0x52FD6E: {0,1000,2000,2001}) | native | 0 |
| Evan job level | 0x4FEEC5: 2200→0, 2210..2218→job%10+1, otherwise **-1** | native | 0 |
| Extended SP | `GW_CharacterStat::Decode` 0x4E2C18: Evan → `ExtendSP::Decode` 0x4E2745 (count byte, then {jobLevel byte, sp byte} pairs), otherwise Decode2. The same applies to the stat-change path at 0x4E3179→0x4E318D. This matches Cosmic `addRemainingSkillInfo` (PacketCreator.java:118) | native, wire matches Cosmic | 0. Check that our 4-byte HP/MP patch (`src/hpmp_patch.h`, sites 0x4E2B9A..0x4E2BE0) does not shift anything. It sits before the SP field, so it should be fine |
| Dragon packets | CField::OnPacket 0x53151F → CUserPool::OnPacket 0x97208C (0xA0-0xEB) → 0x972401 (0xA2-0xB8) → 0xB5-0xB7 → `CUser::OnDragonPacket` 0x93908F. 0xB5: new CDragon (size 0x114, ctor 0x4FE2A0) stored at CUser+0x1F2C/0x1F30, Init 0x4FE502. 0xB6: move 0x4FEF90. 0xB7 is not handled in that function (unverified; removal may be implicit) | native, opcodes match Cosmic `SendOpcode` 0xB5/0xB6/0xB7 and `RecvOpcode MOVE_DRAGON 0xB5` | 0 |
| Dragon spawn layout | Init reads Decode4 x, Decode4 y, Decode1 moveAction, Decode2 (its value is not obviously used). Cosmic `spawnDragon` (PacketCreator.java:7381) writes int x, int y, byte stance, **byte 0**, short job, which is one byte more than the client reads. Trailing bytes are probably harmless (unverified) | minor server fix | 0 |
| Dragon rendering | CActionMan (0xBE78D4) method 0x41127F loads `Skill/Dragon/%d.img` (SP 0x131E) with %d = the **owner's job id** (0x4FEC6D-0x4FEC81). It indexes a per-level table using 0x4FEEC5. The default skill is 0x22000006 | native, **data-dependent** | 0. With a 99xx job the level index is -1, so it reads out of bounds. With missing Skill/Dragon it fails to load. Expect a crash or invisible dragon (not run) |
| Skill window | strings `UI/UIWindow.img/SkillEx/*` (SP 0x787-0x790, 0xA65, 0xA6C/0xA6D: backgrnd, BtSpUp, SpNum, Dragon/%d, egg/%d, Tab), used at 0x8B9D87 | native (assumed complete, not exercised) | 0-1 day if layout or tab bugs show up |
| Dragon equip | `Character/Dragon/%08d.img` for item types 194-197 (0x5C987A-0x5C9888), `Equip/DragonEquip` 0x7FD5D1, `BtDragonEquip` 0x7FE1A4, "This item can only be equipped on Evan." 0x4F1D41 | native | 0 |
| Evan character actions | names at SP 0x1573-0x1584 (dragonSpark, dragonBreathe, dragonIceBreathe, dragonFury, dragonAura, ...) | native, needs body data | 0 |
| Character creation | only NewChar, NewCharKnight, NewCharAran and RaceSelect/aran strings. **No Evan race** | **missing** | Either 0 (server converts at Lv1 via NPC/quest), or about 1-3 days to add a 4th race button and the CreateChar packet type plus Login.img art |
| MapleRoot hooks | `SkillEdits/Activeskill.h` uses per-skill-id lists (5th job, ltrb, crits, attack speed); `CharacterDataEx.h:144/147` patches EXP decode at 0x4E2C6E/0x4E31B6. Both are **after** the SP branch, so they look compatible | review | 1-2 days of QA for Evan magic attacks against those hooks |

So the 10 tiers and extended SP are not gaps in this exe. Nexon compiled the KMS-era Evan code into GMS v83 before releasing it.

## 3. Cheaper routes

**(a) The 9900 remap.** `SlumsMS-1602/src/main/java/client/Job.java:63-64` has EVAN1(9900) and EVAN2-10(9910-9918). Upstream Cosmic (`/root/maplestory_dump/Cosmic/.../Job.java:62`) has 2200-2218. The change arrived with the MapleRoot baseline import (commit 03d13cb21). The client has no reference to 9900 (no grep hits in src-161-qa or mapleroot/). **What it buys:** a 99xx character never enters any client Evan path (name, extended SP, dragon level table, SkillEx), which is presumably a guard against crashes, since MapleRoot ships none of the Evan data. **The reason is unverified;** MapleRoot left no comment. It buys nothing toward a playable Evan, and it leaves the server broken for Evan:
- `GameConstants.hasSPTable` (GameConstants.java:607) is true for 99xx. That makes the server send extended SP, while the client decodes a 99xx job as Decode2. The CharacterStat packet desyncs.
- `getSkillBook` (GameConstants.java:494) still checks 2210-2218. `AssignSPProcessor.java:69` uses skillid/10000 (22xx) while gains go to book 0.
- `JobCommand` rejects jobs >= 2200 (JobCommand.java:41).
- `Character.changeJob` creates a dragon for 99xx (Character.java:1444), which would crash per the dragon-rendering row above.

Conclusion: undo the remap (return to 2200-2218). Do not build on it.

**(b) Evan without the dragon.** The exe ties Evan's look and skill window to the dragon. If we just don't spawn one (server skips `createDragon`), skills still go through the normal MAGIC_ATTACK path from the character. This saves very little: the client still needs Skill/22xx, the SkillEx UI and body actions, and the dragon data is the cheapest part (copy Skill/Dragon). Dragon-animated skills (breaths, Dragon Thrust) would lose their visuals. Possible as a fallback if Skill/Dragon conversion breaks. Saves about 1 day.

**(c) The dragon as a pet, summon or familiar.** Not needed: the native CDragon exists and follows its owner. Faking it as a summon (SPAWN_SPECIAL_MAPOBJECT 0xAF) would need custom Summon data and server movement. That costs more and looks worse. Rejected.

**(d) v83-based servers that shipped Evan.** One web search turned up nothing citable. I name none. **Unverified** whether any public v83 server ships a playable Evan. HeavenMS and Cosmic carry Evan constants, scripts and dragon plumbing, but their data has no Evan skills, so it is not playable there.

## 4. Recommended path

Use native Evan on v83: real job ids, real data, and server-side conversion for creation.

1. **Data (2-4 days).** Convert `Evan.zip` XML to v83 img: Skill/2001 and 2200-2218, Skill/Dragon/*, the SkillEx, DragonEquip and BtDragonEquip nodes merged into our UIWindow, the 17 missing body actions, and dragon items 2003/2004. Find String/Skill (2001xxxx, 22xxxxxx), Sound/Skill and String/Eqp entries from a pre-Big Bang GMS (v88-v92) source. Ship the result as part of the full bundle. Also copy the XML into the server's wz/Skill.wz.
2. **Client (0.5-3 days).** Probably no code. QA the dragon, SkillEx and extended SP. Fix any MapleRoot hook interactions. Optional: an Evan creation button (+1-3 days).
3. **Server (1-2 weeks).**
   - Restore EVAN1-10 to 2200-2218, keeping maxId 22.
   - Fix the extra byte in `spawnDragon`.
   - Add an Evan path: create as a beginner, then an NPC or quest sets job 2001 and warps to 100030000. Cosmic has only 13 Evan quest scripts (22000-22008, 22500-22507) and 4 NPC scripts (1013xxx), so the intro and job-advance quests are partly missing.
   - Check skill effects (Magic Shield, Recovery Aura, Soul Stone, Phantom Imprint, Blaze, Dark Fog, Killer Wings, Flame Wheel, Illusion, Dragon Fury), the SP-per-level rules and the damage checks.
   - Update SoloMapling bots, job tables, the site and leaderboards.

The first hour of work should be a smoke test: on a QA build, give a test character job 2200 with Skill/Dragon data installed and confirm the dragon draws and follows. That proves or kills the plan cheaply. Nothing in this note was run.

## Evidence index

- Server: `/root/work/SlumsMS-1602/src/main/java/client/Job.java:60-67,120-122`; `server/maps/Dragon.java`; `tools/PacketCreator.java:118-133,166-170,7381-7412`; `net/opcodes/SendOpcode.java:221-223`; `RecvOpcode.java:181`; `constants/game/GameConstants.java:494,607-622`; `client/Character.java:1428-1449,12232`; `net/server/handlers/login/CreateCharHandler.java:105-117`; `constants/skills/Evan.java`.
- Upstream: `/root/maplestory_dump/Cosmic/src/main/java/client/Job.java:62-63`.
- Client DLL: `/root/work/client/src-161-qa/src/wvs/avatar.h:67`; `mapleroot/ijl15/YourStuff.h:2953-3011`; `API/MapleStory/Type/GW_CharacterStat.h:60` (extendSP commented out in MapleRoot's struct only).
- Exe addresses: as in section 2. `idb` has only 684 named functions, and none of them are Evan or dragon related.
- Data: `scratchpad/kuro` (Skill.wz images: 2001 present, no 22xx and no Dragon); `/root/maplestory_dump/Evan.zip` (52 files, 86 MB XML); MapleRoot Full Repack and Community Repack zips (no Evan skills, no SkillEx).
