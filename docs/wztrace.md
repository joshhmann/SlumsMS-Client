# wztrace: which WZ lookup ended the session (ticket b251, QA builds only)

211080100 (Rose Garden) ends the session during the field change through an uncaught C++ throw out of packet
processing, with no crash dialog (`/root/work/agent-runs/client/rosegarden/REPORT.md`). The map data itself is
complete, and `src/crash_diagnostics.cpp` stops logging C++ events after 16 per process -- the owner's run had already
spent all 16 on one startup file probe and 15 sound-module throws, so the throw that matters was never written.
What is missing is not a crash dump: it is **which WZ lookup the client made last**. A namespace path plus its
HRESULT names it.

`wztrace` is that trace. It is QA-only (built with `-DSLUMS_QA_MODULE=ON`), off by default, and writes one file,
`wztrace.log`, next to the exe.

## Switch

`config.ini`, section `[debug]`:

```ini
[debug]
wztrace=0
```

| value | what runs |
|---|---|
| `0` or the key missing | nothing. No hook is attached, no file is opened; the namespace hook costs one cached integer compare. |
| `1` | every **FAILED** WZ namespace lookup becomes one line in `wztrace.log`. |
| `2` | also keeps a ring of the last 128 lookups (misses **and** hits, with their HRESULT), detours `_CxxThrowException` and dumps that ring on each throw burst, and logs `MakeTile`'s arguments. |

The switch is read once per process (a QA client's `config.ini` does not change while it runs), so restart the client
after changing it. `integration.log` gets one line at startup saying which level is on.

## What the lines mean

```
2026-11-01T15:04:05.007Z wztrace level=2 ring=128 max=2097152
2026-11-01T15:04:05.120Z thread=4321 makeTile this=0A1B2C30 a0=00000003 a1=00000007 a2=00000005
2026-11-01T15:04:05.121Z thread=4321 lookup hr=80030002 Map/Tile/grassySoil/0
2026-11-01T15:04:05.122Z thread=4321 throw address=004287DD module=MapleStory.exe offset=000287DD hr=none
2026-11-01T15:04:05.122Z ring_begin count=128 (the last WZ lookups, oldest first)
ring[0] hr=00000000 Map/Back/grassySoil/back
...
ring[127] hr=80030002 Map/Tile/grassySoil/0
2026-11-01T15:04:05.122Z ring_end
```

- `wztrace level=...` -- the first line of every run: which switch produced this log, the ring size, the byte cap.
- `lookup hr=<HRESULT> <path>` -- one namespace lookup (`IWzNameSpaceImpl::raw__OnGetLocalObject_hook` in
  `src/resman.cpp`). At level 1 only failures are written, so every `lookup` line at level 1 is a miss.
  `80030002` is "the item was not found", `80030001` a failed namespace, `80070000`-range an OS path failure.
- `makeTile this=... a0=... a1=... a2=...` -- `CMapLoadable::MakeTile` (`0x0063A5C4`) was entered. The three
  arguments are printed **raw, never dereferenced**: their types are not provable from the stock code
  (`docs/research-119-120.md`), and a wrong dereference would crash the QA client. Small numbers are the layer and
  tile indices, so the line still says which tile was being built when a `Map/Tile/...` lookup missed.
  `MakeObj` and `MakeBack` are not hooked: `src/lamps.cpp` and `src/weather.cpp` own those two sites.
- `throw address=<addr> module=<file> offset=<offset> [hr=<HRESULT>|hr=none]` -- `_CxxThrowException`
  (`0x00A60BB7`) was entered, once per throw with **no 16-event cap**. The address is `_ReturnAddress()`, i.e. the
  code that called `_CxxThrowException`, printed in `crash_diagnostics.cpp`'s column shape. The HRESULT is the first
  dword of the thrown object, and only when it falls in the ranges `src/crash_diagnostics.cpp:37-38` accepts
  (`0x2xxxxxxx`, `0x8003xxxx`, `0x8004xxxx`, `0x8007xxxx`); a string, a pointer or packet data can never be printed
  as one. `hr=none` means the object did not start with something that looks like an HRESULT.
- `ring_begin` / `ring[n]` / `ring_end` -- the dump that follows a throw: the last lookups of this process, oldest
  first, each with its HRESULT. `ring[127]` (the last line before `ring_end`) is the newest lookup, i.e. the one
  the throw happened after. At most one dump per 2 s, so a throw burst cannot flood the log. There is no dump while
  the ring is still empty (no lookup seen yet).
- `wztrace.log reached its 2 MB cap: the trace stops here` -- the cap. The file never grows past 2 MB plus this one
  line; delete the log (or restart) to start a new trace.

Reading a failing run: take the first `throw` line of the burst and look at the entries just above it -- the newest
`ring[n]` lines name the lookups that preceded the throw, and the last one that failed is the most likely cause.
At level 1 there is no ring, so read the last `lookup` line before the throw.

## Cost

- Level `0`: no hook, no file. The only cost is the config read and the cached level compare in the namespace hook.
- Level `1`: one small append per **failed** lookup, opened/written/closed per line (a client that dies mid-load
  loses nothing, and the harness may delete the log between runs).
- Level `2`: plus one string per lookup for the ring and one detour each on `_CxxThrowException` and `MakeTile`. The
  originals are always called, so behaviour is unchanged. If the live bytes at either site are not the stock bytes
  `src/wztrace_patch.h` records for `MapleStory.exe` sha256 `86e4e6be...ac90`, that detour is skipped and
  `integration.log` says which site -- a different client build degrades quietly instead of hooking the wrong code.

## Tests

- `tests/wztrace_policy_test.cpp` (`ctest -R wztrace_rules`): the line formatters (one lookup is one line even with a
  newline in the path), the HRESULT filter, UTF-8-safe path cutting, the ring's order and wrap, the dump's buffer
  bound, the 2 s burst throttle and the 2 MB cap.
- `tests/wztrace_patch_test.cpp` (`ctest -R wztrace_sites`): the stock bytes at both sites, the guard's decision, and,
  when the cache variable `WZTRACE_TEST_EXE` points at a `MapleStory.exe`, the same bytes read out of the client
  including each function's `ret` (`ret 8` / `ret 0xC`, which is what proves the argument counts the hooks declare).
