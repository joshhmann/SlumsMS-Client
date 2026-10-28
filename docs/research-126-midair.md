# Ticket 126: mid-air casting for mages and archers (research note)

Ticket: /root/work/SlumsMS-kuro-tickets/docs/tickets/126-midair-casting.md
Sources read: client /root/work/client/src-161-qa (HEAD ca7c572), server /root/work/SlumsMS-1602 (HEAD c8ce90cbe),
launcher /root/work/launcher (f32026a), exe .../scratchpad/client/MapleStory.exe (unpacked v83, image base 0x400000,
on-disk bytes are vanilla; all bytes below from `objdump -d -M intel`). The v83.idb has no names for these
functions (idb_func_of.py returns sub_XXXXXX), so function names below come from the code's behaviour and from
existing comments in the repo, not from the IDB. Marked [unverified] where they are a guess.

## Summary

**Most of this ticket has already shipped.** MapleRoot's `doini()` (mapleroot/ijl15/YourStuff.h:643-653) has two
config switches, `[clienthax] jumpshoot` and `jumpmagic`. Both default to **true** and have been in every
slums.dll since the 2026-10-18 checkpoint (6119130). The launcher already shows them as "Shoot in mid-air" and
"Cast magic in mid-air", both on (launcher/tauri/core/src/game_options.rs:86-89).
- `jumpmagic` lifts the foothold gate in TryDoingMagicAttack. Every normal magic attack can then be cast in
  the air. Ropes and ladders are still refused.
- `jumpshoot` lifts the gate for bows and crossbows in TryDoingShootAttack. Claws and guns never had an air
  gate in stock v83.
- The server has **no** air, foothold or stance check on attack packets, so it needs no change to accept these
  attacks.

What is still gated in the air (stock v83, untouched by MapleRoot):
1. **Keydown skills:** Hurricane, Wind Archer Hurricane, Piercing Arrow, Big Bang x3, plus Rapid Fire, Monster
   Magnet and Corkscrew for other jobs. DoActiveSkill_Prepare refuses to *start* them without a foothold.
2. **Teleport:** DoActiveSkill_Teleport refuses without a foothold. This is a movement skill, not an attack.
3. **Summons:** Puppet, Silver Hawk/Golden Eagle, Phoenix/Frostprey, Elquines/Ifrit, Bahamut, Cygnus summons.
   DoActiveSkill_Summon refuses without a foothold. Leaving this as stock is recommended.

So the real remaining work is:
- (a) confirm on TEST that air casts work per job, with no DISTANCE_HACK alert and no viewer-side drops;
- (b) optionally lift the keydown-start gate, and maybe Teleport, behind new switches with a tested per-skill
  policy;
- (c) fix the misleading comment and add a native test that pins the two existing MapleRoot sites. Neither has a
  test today.

## Existing code

Client (src-161-qa):
- mapleroot/ijl15/YourStuff.h:600 `doini()`, called from MapleHook.cpp:222 `HaxMaple()`. It runs only if
  config.ini parses (`reader.ParseError()==0`).
  - :643 `jumpshoot` (default "true"): `WriteByte(0x009539FA, 0xE9); WriteValue(0x009539FB, 0x12)`, i.e. jmp 0x00953A11.
  - :649 `jumpmagic` (default "true"): `WriteByte(0x009559E5, 0xE9); WriteValue(0x009559E6, 0x36)`, i.e. jmp 0x00955A20.
    Its comment wrongly says "TryDoingShootAttack"; the site is in TryDoingMagicAttack.
  - :630-641: the 1.5.0 attackwalk/attackjump/chargemove split, applied from src/attackstop_patch.h.
- src/attackstop_patch.h: the pattern to follow. It has:
  - a site table with address, length, original bytes and patched bytes;
  - `ClassifyAttackStopSite` (Original / MoveAttack / Unknown);
  - `ResolveMoveAttackSettings` (key precedence);
  - `DecideMoveAttackWrite`: patch only over vanilla bytes, restore only over our own bytes, leave unknown bytes
    alone;
  - `ApplyMoveAttackSettings(settings, reader, writer)`, a template so the logic runs natively.
  The test is tests/attackstop_patch_test.cpp (CTest `attackstop_moveattack_sites`, tests/CMakeLists.txt:133). It
  checks the encodings and, given an unpacked client, the bytes in the exe. The README section "Movement during
  attacks" (README.md:44-54) already says "`jumpshoot` and `jumpmagic` stay separate keys". There is no CMake
  option: the 1.5.0 proof's MAPLEROOT_VANILLA_ATTACK_STOP was removed in 53fd583, and the switches are
  config-only.
- History: 44ab18f (proof), 53fd583 (split into three keys), 0ab24b3 (merged into 1.5.0-rc1). Branch
  client-moveattack-split.
- src/bounce_rules.h:241-251 `AirState`/`IsAirborne`: an existing tested model of the same CVecCtrl fields:
  - foothold = CVecCtrl+0x110;
  - ladder = 0x00705309, rope = 0x00705343;
  - swim/fly = 0x007046AC.
  Reuse it.
- src/attackdiag.cpp / attackdiag_rules.h (MAPLEROOT_ATTACK_DIAG, `[debug] attackdiag=1|2`): logs remote attacks
  the viewer drops, via CUser::CheckAttackAction 0x0092EDB2 and OnAttack. Use it on TEST to prove viewers draw
  air casts.

Launcher: game_options.rs:86-89 (`jumpshoot`, `jumpmagic`, both true, "Movement" group). The client-default
table test is at :700-720. The README table is at :263-264.

Server (SlumsMS-1602, src/main/java):
- net/server/channel/handlers/MagicDamageHandler.java, RangedAttackHandler.java, CloseRangeDamageHandler.java
  → AbstractDealDamageHandler.parseDamage (:532). These read stance, display and direction. There is **no**
  foothold, air or stance-vs-position validation. The checks they do make:
  - morph-without-attack disconnect (Magic :52-58, Ranged :68, Close :60);
  - `display > 80` with a skill that has no action → FAST_ATTACK autoban (:80-84; the same action is used in
    the air, so it does not apply);
  - MPCON (:129), MOB_COUNT (:160);
  - **DISTANCE_HACK alert** (:210-243): sq-distance from `player.getPosition()` to the mob, limit
    200k + 400k ranged / +200k magic (+275k Genesis/Blizzard/Meteor). This is an alert only, and it calls
    monster.refreshMobPosition();
  - DAMAGE_HACK (:848-855).
- MovePlayerHandler.java / AbstractMovementPacketHandler.java: the server takes the client's positions and
  stances, with no physics validation. The only extra is NavObservedEdges teleport/flash-jump learning
  (:157-233).
- SkillEffectHandler.java:45-61: keydown "prepare" packets (Big Bang, Hurricane, Piercing Arrow, Rapid Fire,
  WA Hurricane...). No air check.
- AttackCapture.attack(...) is called in all three handlers (soloMapling/server/AttackCapture.java). It is
  diagnostic capture only.

Bots (soloMapling/ArtificialPlayer):
- BotGrindSystem/MovementStylePolicy.java:
  - mages with Teleport → TELEPORT (blink, then attack planted);
  - bowmen 310-322 → RANGED (planted kite);
  - haste thieves 410-422 / 1410-1412 → JUMP_ATTACK;
  - Hermit/NL/NW with Flash Jump → FLASH_JUMP.
  Cygnus archers (1300s) fall to PLANTED.
- BotGrindSystem/EngageBeat.java:241-262 `engageAndSwing`: only JUMP_ATTACK/FLASH_JUMP hop and swing mid-arc
  (JUMP_ATTACK_CHANCE 0.68, :22). Mages and archers always plant. Bots build attack packets server-side
  (soloMapling/server/BotAttackPackets.java, BotAttackSystem/BotClientPoses.java), so no client gate applies to
  them.

## Client gates (verified bytes, MapleStory.exe)

Notation: `vc` = the local CVecCtrl. It is obtained via `call 0x004AD42B` (get vec-ctrl ZRef), then the
`neg/sbb/and` -0xC adjust. Fields:
- `[vc+0x110]` = current foothold (null in the air and on ropes/ladders);
- `call 0x004B1FE1(&[vc+0x124],[vc+0x12C])` = decode of the ZtlSecure ladder/rope pointer (non-zero = on a
  ladder or rope);
- `call 0x007046AC` = swim map / fly map.

### TryDoingMagicAttack: function 0x0095571F [name inferred from the Heal/Meteor ids in it]
```
9559b7  8b 87 70 05 00 00     mov eax,[edi+0x570]   ; move action
9559bd  83 e0 fe / 83 f8 12   and ~1; cmp 0x12      ; action 9 (dead?) [unverified meaning]
9559c3  0f 84 14 01 00 00     je refuse(0x955ADD)
9559cf  e8 96 c1 af ff        call 0x451B6A         ; one-time action
9559d4  83 f8 ff / 0f 8f ..   jg refuse              ; busy with another action
9559dd  8b 86 10 01 00 00     mov eax,[vc+0x110]
9559e3  85 c0
9559e5  75 2e 8b 45 ec        jne 0x955A15 ; mov eax,[ebp-0x14]   <-- jumpmagic writes E9 36 00 00 00 (jmp 0x955A20)
9559ea  3d 4a 1c 23 00 ...    skill == 2301002 Heal / 2111002 Explosion / 22151001 / 22121000 (Evan) -> allowed in air
955a06  e8 9f ec da ff        call 0x7046AC ; je refuse  -> otherwise air needs a swim/fly map
955a15  e8 90 ec da ff        call 0x7046AC ; je 0x955A3C
955a20  ff b6 2c 01 00 00 ... call 0x4B1FE1 ; jne refuse  -> ladder/rope refused
```
With jumpmagic on, the code skips straight to the ladder/rope test. That allows all magic attacks in the air
and still refuses them on ropes and ladders, in every map type. This covers 2xx and 12xx. Meteor (2121007),
Blizzard (2221007) and Genesis (2321008) are referenced inside this function (0x955D7C..) and dispatch through
DoActiveSkill → 0x00969B10 → here, so they are covered. 0x00969B10 refuses prone ((action&~1)==0xA).

### TryDoingShootAttack: function 0x009537D5 [name per MapleHook.cpp:355 comment]
```
953972  mov eax,[ebx+0x570]; and ~1; cmp 0x12; je refuse(0x953980)
9539a4  fieldLimit bit -> refuse 3121003 / 3221003 / 5201006 (the knock-back-self shots) [names unverified]
9539e8  83 7d dc 2d 74 06     weapon type == 45 bow
9539ee  83 7d dc 2e 75 17     or 46 crossbow, else jne 0x953A0B
9539f4  39 be 10 01 00 00     cmp [vc+0x110],edi(0)
9539fa  75 0f 8b ce e8        jne 0x953A0B ...     <-- jumpshoot writes E9 12 00 00 00 (jmp 0x953A11)
9539fe  call 0x7046AC ; je refuse                 -> stock: bows/xbows need a foothold unless swim/fly
953a0b  83 7d dc 2f 75 1c     claw (47): ladder/rope test only (0x953A11: call 0x4B1FE1; jne refuse)
953a2d  83 7d dc 31 75 1c     gun (49): ladder/rope test only
953a4f  call 0x7046AC ; je ; then ladder/rope test in swim/fly maps
```
jumpshoot sends bows and crossbows to the claw path's ladder/rope test, so they can shoot in the air but not
on a rope. This covers 3xx, 13xx, Arrow Rain 3111004, Arrow Eruption 3211004 and WA 13111000. Those dispatch
via DoActiveSkill (0x967F67/0x968012) → 0x009696A2 → here, with special cases only for range (0x9548B9).
Inferno/Blizzard arrows 3111003/3211003 also go through here. [unverified for each skill in game]

### DoActiveSkill_Prepare (keydown start): function 0x0096A86E [name inferred; hpmp_patch.h:136 calls it that]
```
96a92f  bb 6a 5c 4f 00        mov ebx,5201002 (Grenade)
96a934  skill == Grenade or 14111006 (Poison Bomb) -> skip foothold
96a942  83 bf 10 01 00 00 00  cmp [vc+0x110],0
96a949  75 0f                 jne 0x96A95A
96a94b  8b cf / e8 5a 9d d9 ff  call 0x7046AC
96a954  0f 84 54 05 00 00     je refuse(0x96AEAE)   <-- NOT patched today
96a968  push [vc+0x12C]; call 0x4B1FE1; jne refuse   (ladder/rope test, Grenade/Poison Bomb only)
96a984  call 0x7046AC; ladder test again in swim/fly maps
96aa49  per-skill branches: 3221001 Piercing Arrow, 3121004 Hurricane, 5221004 Rapid Fire, 13111002 WA Hurricane
```
Big Bang 2121001/2221001/2321001 is keydown (SkillEffectHandler; ids referenced at 0x95F993.., the chargemove
check). So Hurricane, Piercing Arrow, WA Hurricane and Big Bang **cannot be started in the air** even with
jumpshoot/jumpmagic on. Once started on the ground, `attackjump` and `chargemove` (1.5.0) may already let you
jump while still firing [unverified; test it].

Patch options:
- **(A) byte patch.** At 0x0096A942 write `EB 24` (jmp 0x96A968) and NOP the following 7 bytes. The foothold
  demand is gone, but the ladder/rope test is kept. Same shape as jumpshoot.
- **(B) per-skill policy (recommended).** A 9-byte codecave at 0x0096A942 that calls
  `SlumsMidAir_AllowPrepare(skillId=[ebp-0x10], vc=edi)` and jumps to 0x96A968 (allowed: rope test still
  runs) or keeps the stock path.

### DoActiveSkill_Teleport: function 0x00957B74 [inferred: DoActiveSkill sends 2101002/2201002/2301001 to 0x969146 → call 0x957B74]
```
957c1f  39 7d 18              cmp [ebp+0x18],edi   ; 5th arg, DoActiveSkill passes 0
957c25  75 0c                 jne 0x957C33
957c27  39 b8 10 01 00 00     cmp [vc+0x110],edi
957c2d  0f 84 fd 04 00 00     je refuse(0x958130)  <-- stock: no Teleport in the air
```
The Blaze Wizard Teleport id 12101003 was not found as an immediate (so it goes through some other compare)
[unverified]. Teleport is a movement skill, so treat it as optional for this ticket and ask the owner. Mid-air
Teleport also changes bot NavObservedEdges learning (AbstractMovementPacketHandler:157-233).

### DoActiveSkill_Summon: function 0x0096B140 (called from 0x9689DF for 3211005)
`96b168 83 ba 10 01 00 00 00 / 0f 84 73 02 00 00`: no summon in the air. Recommendation: keep it stock, since
a summon would spawn in mid-air or on a drop.

## Server changes

There is nothing required for acceptance: no handler rejects or flags an air attack because of being in the air.
To consider:
1. **DISTANCE_HACK.** It uses the last movement-packet position. The jump apex is about 80-100 px above the
   last position, which is well under the sqrt(600k)≈775 px ranged / sqrt(400k)≈632 px magic limits. A
   Teleport-then-cast in the air could produce a stale position [unverified]. Watch the `DISTANCE_HACK` alerts
   on TEST; do not change the limits unless one fires.
2. **Optional:** an `air` tag in AttackCapture, set when the last movement stance is a jump/fall stance, so TEST
   runs can count air casts. This is diagnostic only.
3. **Bots, if the owner wants consistency (behind a flag).** Today mages and archers always plant.
   - Add a `MovementStylePolicy` or `EngageBeat` option `slums.botAirCast` (default false), letting
     RANGED/TELEPORT bots sometimes `GCMovement.jumpToward/jumpInPlace` before `BotAttackDriver.botAttack`,
     as JUMP_ATTACK does.
   - Never start keydown skills in the air unless the client keydown gate is also lifted. Viewers' clients
     drop a Hurricane/Rapid Fire shot that has no keydown packet (attackdiag reason), and
     BotClientPoses.KEEPS_KEYDOWN_POSE covers those skills.
   - Put the skill list in one Java class (e.g. soloMapling/ArtificialPlayer/BotAttackSystem/MidAirPolicy.java)
     that mirrors the client header. Include a golden test asserting both lists are equal, as bounce_rules
     does with the server yaml.

## Risks

- **Animation and stance in the air.** The local client plays the normal attack action over the jump
  stance; this has been stock since 1.0 for claws and guns, and since the checkpoint for bows and magic.
  Remote viewers validate the pose in CUser::CheckAttackAction (0x0092EDB2), which does not depend on the
  foothold. Commit 20f29c1 already catches the remote melee no-range ZException, but it covers melee only.
  Run TEST with `[debug] attackdiag=1` and an observer client.
- **Falling while casting.** Long casts (Meteor, Blizzard, Genesis, Arrow Rain, Big Bang release) keep falling
  under gravity, so the effect is anchored where the cast started. If attackwalk is off, the air arc keeps its
  momentum anyway. Falling off a ledge mid-cast is cosmetic, because the server takes the client's mob list.
- **Knockback.** A hit during an air cast still knocks back. With attackjump the one-time action no longer
  blocks Jump, so you can be knocked back while the attack plays. Same behaviour as 1.5.0.
- **Ropes and ladders.** Both existing patches keep the ladder/rope refusal. Option A/B for keydown must keep
  it too: jump to 0x96A968, never past it. A plain "skip the foothold test" (`75 0f`→`EB 0F`) would allow
  casting on ropes, so do not use it.
- **Swim and fly maps (Aqua Road, Orbis tower fly).** jumpmagic and jumpshoot remove the stock "air allowed
  only in swim/fly maps" rule, which is a superset. The ladder test still runs.
- **Exceptions dropped by jumpmagic.** Heal, Explosion and the Evan breaths were already allowed in the air
  in stock, so nothing is lost.
- **Keydown in the air with chargemove on.** Charging while falling: the release packet carries the charge
  time, and the server reads it for Big Bang (parseDamage :550). There is no position check, so it is low
  risk, but the remote OnPrepare draw happens in the air [unverified].
- **config.ini parse failure** disables every doini patch, jumpshoot and jumpmagic included. The launcher
  already reports unparseable lines.

## Scaffold plan

Client (new branch off the 1.6.x QA line, e.g. client-midair-cast):
1. `src/midair_patch.h`, header-only and natively testable, modelled on attackstop_patch.h:
   - Site table:
     - `{0x009539FA, 5, 75 0F 8B CE E8, E9 12 00 00 00, "TryDoingShootAttack bow/xbow foothold gate"}`
     - `{0x009559E5, 5, 75 2E 8B 45 EC, E9 36 00 00 00, "TryDoingMagicAttack foothold gate"}`
     - new `{0x0096A942, 9, 83 BF 10 01 00 00 00 75 0F, <cave jmp + nops>, "DoActiveSkill_Prepare foothold gate"}`
     - optional `{0x00957C27, 12, ..., "DoActiveSkill_Teleport foothold gate"}`
   - Classify/decide/apply exactly as `DecideMoveAttackWrite`.
   - Policy:
     ```cpp
     enum class AirCast { Allow, Stock };
     AirCast MidAirPolicy(int skillId, int jobId);
     inline constexpr std::array kAirKeydown = {3121004, 3221001, 13111002, 2121001, 2221001, 2321001};
     // optional: 5221004 Rapid Fire, 1121001/1221001/1321001 Monster Magnet = Stock
     inline constexpr std::array kNeverInAir = {/* summons */ 3111002, 3211002, 3111005, 3211005,
         2121005, 2221005, 2321003, 12111004, 13111004 /* verify ids */};
     bool IsExplorerOrCygnusMageArcher(int job);  // 200-232, 300-322, 1200-1212, 1300-1312
     ```
     The test lists every Explorer and Cygnus mage/archer attack skill id with its expected route
     (magic / shoot / keydown / summon) and the expected decision.
2. Move the two MapleRoot writes from YourStuff.h:643-653 to `integration::ApplyMidAirSettings(...)`. Keep the
   key names `jumpshoot`/`jumpmagic` and their default true so the launcher stays correct. Add
   `[clienthax] jumpkeydown` (default to be decided by the owner; suggest true on TEST only) and, if wanted,
   `jumpteleport` (default false). Fix the jumpmagic comment.
3. The keydown cave lives in a .cpp under src/ (e.g. src/midair.cpp). It is a naked thunk that pushes
   [ebp-0x10] and edi and calls `SlumsMidAir_AllowPrepare`:
   - Allow → jmp 0x0096A968 (the ladder/rope test, then the stock flow);
   - Stock → re-run the original `cmp [edi+0x110],0` / call 0x7046AC path, i.e. return to 0x0096A949 with
     flags set.
   Check the entry bytes before patching, as 20f29c1 does.
4. CMake: optional `option(MAPLEROOT_MIDAIR_KEYDOWN "Lift the in-air start gate for keydown attacks ([clienthax] jumpkeydown)" OFF)`.
   Turn it on in scripts/build.ps1 only after TEST. The existing jumpshoot/jumpmagic need no option.
5. tests/midair_patch_test.cpp plus `add_test(NAME midair_sites COMMAND midair_patch_test)`, checking:
   - the encodings: E9 targets 0x953A11 and 0x955A20, and the cave jump lands on 0x96A968;
   - with an unpacked exe: the original bytes;
   - the policy table for each skill id.
6. Documentation: a README "Movement during attacks" row for jumpkeydown/jumpteleport; config.example.ini keys
   with comments.
7. Launcher (a separate repo; the owner's call): add the jumpkeydown option and its client-default row
   (game_options.rs:700-720). This is required whenever a client key is added, because the test pins the
   table length.

Server (SlumsMS-1602 line):
- No handler change is required.
- Optional: an AttackCapture `air` tag; `MidAirPolicy.java` plus a golden test against the client header; bot
  flag `slums.botAirCast` (default false) in EngageBeat/MovementStylePolicy, letting RANGED and TELEPORT
  bots hop-cast only with skills the policy allows (no keydown or summon skills in the air).

## Test plan (TEST, client with attackdiag=1, a second observer client on the same map)

For each job below:
- jump from flat ground and cast each attack skill at the apex and while falling;
- try it on a rope and on a ladder (must be refused, nothing sent);
- try it in a swim map (Aqua Road) and in Orbis/El Nath air if relevant.

Watch:
- the server log for DISTANCE_HACK, DAMAGE_HACK, MOB_COUNT and FAST_ATTACK, and for disconnects;
- the observer's attackdiag.log for dropped attacks;
- whether MP is spent and the mobs are damaged.

Jobs and skills:
- F/P 210-212: Fire Arrow, Poison Breath, Explosion, Element Composition, Fire Demon, Meteor; Big Bang
  (keydown; expect refusal unless jumpkeydown).
- I/L 220-222: Cold Beam, Thunder Bolt, Ice Strike, Thunder Spear, Chain Lightning, Blizzard; Big Bang.
- Cleric/Priest/Bishop 230-232: Holy Arrow, Heal (already allowed in the air in stock), Shining Ray, Angel
  Ray, Genesis; Big Bang.
- Magician 200: Energy Bolt, Magic Claw.
- Blaze Wizard 1200-1211: Magic Claw, Fire Pillar, Flame Gear [verify ids].
- Hunter/Ranger/Bowmaster 300-312: Double Shot, Arrow Bomb, Power Knock-Back (fieldLimit case), Strafe,
  Inferno, Arrow Rain, Hurricane (keydown).
- Crossbowman/Sniper/Marksman 320-322: Iron Arrow, Blizzard, Arrow Eruption, Strafe, Snipe, Piercing Arrow
  (keydown).
- Wind Archer 1300-1311: Double Shot, Strafe, Arrow Rain 13111000, Hurricane 13111002 (keydown).
- Also: Teleport 2101002/2201002/2301001/12101003 in the air (stock refusal; only if jumpteleport is built);
  summons in the air (must stay refused).

Regression:
- attackjump, chargemove and doubletapjump still behave as in 1.5.0;
- jumpshoot=false and jumpmagic=false restore the stock refusal (byte restore path);
- `ctest` passes.

## Effort

- Verification only (the existing switches are already shipped): **S**, about half a day on TEST across
  8 job lines.
- Plus the keydown gate, policy header and tests: **S-M**, 1-2 days.
- Plus optional Teleport and bot hop-casting with a Java mirror and golden test: **M**.
The ticket's "M" overestimates, given that the client and server halves for normal attacks already exist.

## Unverified items

- Function names (TryDoingMagicAttack 0x95571F, TryDoingShootAttack 0x9537D5, DoActiveSkill_Prepare 0x96A86E,
  DoActiveSkill_Teleport 0x957B74, DoActiveSkill_Summon 0x96B140) are inferred. The IDB has no names for them.
- The meaning of move action 0x12 (probably dead).
- In-game confirmation that jumpshoot/jumpmagic are actually active on players' installs. They are on unless
  config.ini sets false or fails to parse.
- Whether attackjump/chargemove already allow jumping while holding Hurricane/Big Bang.
- The Blaze Wizard Teleport and the Cygnus summon/attack id list.
- The remote-viewer behaviour for air keydown starts.
