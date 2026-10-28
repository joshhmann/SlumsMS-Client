# Monster Book addon (`MAPLEROOT_MONSTER_BOOK`)

Ticket 19. Ported from the "MonsterBook Publish" package (client-kentakae). The package's own design notes,
addresses and failure history stay in the module headers.

## What it does

| module | owns |
|---|---|
| `monsterbook.cpp` | Every card in colour with a counter from 0; mob art and real HP/MP at 0 cards; all four tabs; the card-level dual key fix; the book-data redirect. |
| `monsterbook_foundin.cpp` | A Found In row click opens the world map at that field; Found In and Episode text styling. |
| `monsterbook_drops.cpp` | The live drop % under each Dropping icon; 16 icons per page. |
| `monsterbook_search.cpp` | Mob-name search (local) and item-name search with the droppers of a hit, each with its %. |

Every byte patch is verified against the stock bytes first. A client with different bytes keeps the stock look.
All addresses were checked against the shipped `MapleStory.exe`
(sha256 `86e4e6be050caeda4a93443237256a84376ea74781fcba558d2418f0568aac90`).

## Wire

- CP 0x3734 `MONSTER_BOOK_QUERY`: `byte type`, then one of:
  - 0: `int mobId`;
  - 1: `str query`;
  - 2: `int itemId`.
- LP 0x3735 `MONSTER_BOOK_RESULT`: `byte type`, then one of:
  - 0: `int mobId`, `short n`, n x `{int itemId, int ppm}`;
  - 1: `str query`, `short n`, n x `int itemId`;
  - 2: `int itemId`, `short n`, n x `{int mobId, int ppm}`.

`ppm` is parts per million after the asking player's rates. The server half is
`MonsterBookQueryHandler` / `DropSearchService` in the SlumsMS repo.

## Data (Custom.wz)

`tools/wz/add_monster_book.py` bakes these into `src/Custom.wz` from `assets/monsterbook/monsterbook.json`.
That JSON is made by the server repo's `tools/monsterbook/build_book.py` from the server's own `drop_data` and WZ.

| node | why |
|---|---|
| `String/MonsterBookEx.img` | The whole book: per mob `map/<i>`, `reward/<i>` (the server's drops, best first) and `episode` (stats). It is a new image because Custom.wz cannot add or remove sub-folders of the stock `MonsterBook.img`. `monsterbook.cpp` points `CMonsterBookMan::LoadBook` at it through StringPool 0x8BF, and only when it resolves. |
| ~~`Mob/<id>.img/info/defaultHP`, `defaultMP`~~ | Removed in 1.6.1-rc7. In the player install the client took this one-value `info` instead of the stock one (maxHP, speed and the rest read empty for Lucid 8880150, Will 8880302 and Verus Hilla 8880415, measured 2026-10-28), so the tool no longer writes it. Those three book pages print HP the stock way. |
| `UI/UIWindowEx.img/MonsterBook/search/{item,mob}Box{0,1}` | The 122x17 search-field boxes, from `assets/monsterbook/*.png`. |

When the server's `drop_data` changes, re-run both tools and ship a new `Custom.wz`. This changes which icons are
listed; the percentages need nothing, because they are always live.
