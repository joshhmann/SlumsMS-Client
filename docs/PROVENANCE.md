# Source provenance

The starting installation is MapleRoot Full Repack. The client history begins with the existing joshhmann/kaentake checkout at 2a43a01e7f4cdd75cda08ab533049230760d7cfa; pinned upstream submodules are preserved. The companion source is the MapleRoot ijl15 source port, with only relative project paths adjusted for this repository layout.

Server integration was developed against the captured running MapleRoot source and SoloMapling commit 47b61b5381118ebdc1064ddbd0b1c32f75778871. The separate joshhmann/SoloMapling headless-drop branch was deliberately excluded. Original file-level author/licence notices remain in place.

The manually accepted gameplay checkpoint is Accepted-Addons-20261018-r1. The public source baseline additionally replaces the inherited hardcoded registration webhook with an optional private environment setting. Two new tests cover disabled/configured construction without sending messages, and isolated full startup passed. Exactly two server class entries differ from the earlier accepted JAR for that change; all other 22,709 entries are byte-identical.

Full game Data, server WZ definitions, real configuration and database dumps remain external deployment inputs. Their protected checkpoint has complete asset fingerprints and restore notes. No live rollout occurred during repository preparation.

The Monster Book addon (`MAPLEROOT_MONSTER_BOOK`: `src/monsterbook*.cpp`, `assets/monsterbook/*.png`) is ported from the "MonsterBook Publish" package. Its client half was written for the Kentakae client mod and its search-field art comes from the package's `reference/searchfields`. The package's design notes and history stay in the module headers. `docs/monsterbook.md` lists what changed for this repo.

The Coloring Prism addon (`MAPLEROOT_COLOR_PRISM`: `src/weapontint.*`, `src/coloringprism.cpp`) is the `after/` client half of the "ColorPrism TrueColor Fix" package; `src/coloringprism_hooks.cpp` and `src/weapontint_proto.h` are new for this repo. The window art and the 5782000 item image in `assets/colorprism/` are BunsMS-authored procedural art (`authored-provenance.json`; icon traced from the GMS 5782000 icon), not recovered package art. `docs/colorprism.md` lists what changed.

The resizable inventory (`MAPLEROOT_INV_RESIZE`: `src/invresize.cpp`, `src/invresize_patch.h`) is ported from the community "invresize" mod (`invresize.zip`, collected 2026-10-25, no licence file; the owner confirmed on 2026-10-25 that the author shared it publicly on Discord for use, which they accept as permission to ship it). Its analysis and logic are kept; this repo moved the addresses and stock bytes into a testable header, logs instead of a message box on a byte mismatch, and adds the `[clienthax] invresize` switch. Every address and guard was re-checked against MapleStory.exe sha256 86e4e6be...ac90.
