#!/usr/bin/env python3
"""Bake the Monster Book addon's data into src/Custom.wz (ticket 19; runs on Linux, no WZ editor needed).

Writes three things, all through the Custom.wz rules in resman.cpp (a new top-level image is served whole; an
existing image only gets leaf values added or replaced):

  String/MonsterBookEx.img   the whole book: per mob map/<i>, reward/<i>, episode. It is a NEW image because a
                             stock image cannot gain or lose sub-folders through Custom.wz; monsterbook.cpp points
                             the book loader at it (StringPool 0x8BF) only when it exists, so a Custom.wz without
                             it leaves the stock book in charge.
  (no Mob/<id>.img)          1.6.0-1.6.1-rc6 also wrote Mob/<id>.img/info/defaultHP for mobs whose maxHP the book
                             cannot print as an int (Lucid 8880150, Will 8880302, Verus Hilla 8880415). In the
                             player install the client then took that one-value info INSTEAD of the stock info
                             (maxHP, speed, ... empty; measured 2026-10-28), so these are no longer written and
                             any left in Custom.wz are removed. Their book page may show HP as stock prints it.
  UI/UIWindowEx.img/MonsterBook/search/{itemBox0,itemBox1,mobBox0,mobBox1}
                             the two search-field boxes (122x17, BGRA8888), from assets/monsterbook/*.png.

Inputs:
  assets/monsterbook/monsterbook.json   the SERVER's view of the book: drops, spawn maps, stats, HP strings.
                                        Made by the server repo's tools/monsterbook/build_book.py from its own
                                        drop_data and WZ. Re-run that and this whenever drop_data changes.
  --stock-book Data/String/MonsterBook.img, --stock-cards Data/Item/Consume/0238.img (optional, from the
                                        client): keep each mob's stock "Form" line, keep stock maps for mobs the
                                        server has no spawn for, cover every client card, and report the diff.

The rewards are the server's drop_data, best chance first. Mobs the server does not know keep their stock entry.

    python3 tools/wz/add_monster_book.py [--stock-book ...] [--stock-cards ...] [--check]
"""
import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, Node, canvas, parse_img, write_img, semantic_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
ASSETS = ROOT / 'assets' / 'monsterbook'
BOOK_IMG = 'String/MonsterBookEx.img'
UI_IMG = 'UI/UIWindowEx.img'
SEARCH_ART = ('itemBox0', 'itemBox1', 'mobBox0', 'mobBox1')
FORM_RE = re.compile(r'^\s*Form\s*:\s*(.*?)\s*$', re.I)
LEVEL_RE = re.compile(r'^\s*Lv\.?\s*:\s*(\d+)\s*$', re.I)


def stock_entries(path):
    """{mob: {'map': [...], 'reward': [...], 'episode': str}} from a client String/MonsterBook.img."""
    if not path:
        return {}
    root, _ = parse_img(Path(path).read_bytes(), 'MonsterBook.img')
    out = {}
    for e in root.children:
        if not e.name.isdigit():
            continue
        def ints(name):
            n = e.child(name)
            return [c.value for c in sorted(n.children, key=lambda c: int(c.name)) if c.kind in ('int', 'short')] \
                if n is not None else []
        ep = e.child('episode')
        out[int(e.name)] = {'map': ints('map'), 'reward': ints('reward'),
                            'episode': ep.value if ep is not None and ep.kind == 'string' else ''}
    return out


def stock_cards(path):
    if not path:
        return {}
    root, _ = parse_img(Path(path).read_bytes(), '0238.img')
    cards = {}
    for c in root.children:
        info = c.child('info')
        mob = info.child('mob') if info is not None else None
        if mob is not None and c.name.isdigit():
            cards[int(c.name)] = mob.value
    return cards


def stock_header(episode):
    """(level, form) from a stock episode's first lines ("Lv. : 12\\nForm : Mammal\\n\\n...")."""
    level, form = None, None
    for line in episode.replace('\\n', '\n').split('\n')[:3]:
        m = LEVEL_RE.match(line)
        if m:
            level = int(m.group(1))
            continue
        m = FORM_RE.match(line)
        if m:
            form = m.group(1)
    return level, form


def episode_text(level, form, stats):
    text = 'Lv. : %d\nForm : %s' % (level, form or 'Monster')
    if stats:
        text += '\n\n' + '\n'.join(stats)
    return text.replace('\n', '\\n')   # the client stores line breaks as a literal backslash-n


def build_book(book, stock, cards):
    server = book['mobs']
    ids = sorted({int(m) for m in cards.values()} | set(stock) | {int(m) for m in book['cards'].values()})
    report = {'entries': 0, 'from_server': 0, 'stock_only': 0, 'reward_changed': 0, 'maps_from_stock': 0}
    root = Node(BOOK_IMG, 'property')
    for mob in ids:
        s, old = server.get(str(mob)), stock.get(mob)
        if s is None and old is None:
            continue
        if s is None:
            # the server has no Mob.wz entry for it, so it can never spawn: keep the stock page
            maps, reward, episode = old['map'], old['reward'], old['episode']
            report['stock_only'] += 1
        else:
            level, form = stock_header(old['episode']) if old else (None, None)
            maps = s['map'] or (old['map'] if old else [])
            if not s['map'] and old and old['map']:
                report['maps_from_stock'] += 1
            reward = s['reward']
            episode = episode_text(s['level'] or level or 0, form, s['stats'])
            report['from_server'] += 1
            if old is not None and old['reward'] != reward:
                report['reward_changed'] += 1
        entry = Node(str(mob), 'property', children=[
            Node('map', 'property', children=[Node(str(i), 'int', v) for i, v in enumerate(maps)]),
            Node('reward', 'property', children=[Node(str(i), 'int', v) for i, v in enumerate(reward)]),
            Node('episode', 'string', episode)])
        root.children.append(entry)
        report['entries'] += 1
    missing = sorted({int(m) for m in cards.values()} - {int(e.name) for e in root.children})
    return root, report, missing


def hp_overrides(book, wanted):
    out = {}
    for mob, e in book['mobs'].items():
        if int(mob) in wanted and e.get('defaultHp'):
            out[int(mob)] = e['defaultHp']
    return out


def search_art():
    nodes = []
    for name in SEARCH_ART:
        im = Image.open(ASSETS / f'{name}.png').convert('RGBA')
        if im.size != (122, 17):
            sys.exit(f'{name}.png is {im.size}, expected 122x17')
        nodes.append(canvas(name, im))
    return Node('search', 'property', children=nodes)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--book', default=str(ASSETS / 'monsterbook.json'))
    ap.add_argument('--stock-book', help="the client's Data/String/MonsterBook.img")
    ap.add_argument('--stock-cards', help="the client's Data/Item/Consume/0238.img")
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz is up to date')
    args = ap.parse_args()

    book = json.loads(Path(args.book).read_text(encoding='utf-8'))
    if book.get('format') != 1:
        sys.exit('unexpected monsterbook.json format')
    stock = stock_entries(args.stock_book)
    cards = stock_cards(args.stock_cards) or {int(c): m for c, m in book['cards'].items()}
    book_root, report, missing = build_book(book, stock, cards)
    if missing:
        sys.exit(f'{len(missing)} card mobs would have no book entry (the book would print "HP : (null)"): '
                 f'{missing[:20]}')
    art = search_art()

    with Archive(CUSTOM, 83, 'gms') as wz:
        before = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
        ui = wz.image(UI_IMG)
        mb = ui.child('MonsterBook')
        if mb is None:
            mb = Node('MonsterBook', 'property')
            ui.children.append(mb)
        mb.put('search', art, replace=True) if mb.child('search') else mb.children.append(art)
        targets = {BOOK_IMG: book_root, UI_IMG: ui}
        drop = sorted(p for p in before if p.startswith('Mob/'))   # partial stock images: see the docstring

        wanted = {p: semantic_hash(n) for p, n in targets.items()}
        stale = {p for p, h in wanted.items() if before.get(p) != h}
        if not stale and not drop:
            print(f'{CUSTOM.relative_to(ROOT)} is up to date ({report["entries"]} book entries)')
            return 0
        if args.check:
            print(f'{CUSTOM.relative_to(ROOT)} is stale: {sorted(stale)[:10]} drop {drop}')
            return 1
        for path in sorted(stale):
            blob = write_img(targets[path], 'gms')
            reparsed, _ = parse_img(blob, path, 'gms')
            if semantic_hash(reparsed) != wanted[path]:
                sys.exit(f'{path} does not round-trip')
            wz.put_image(path, blob, replace=path in before)
        for path in drop:
            wz.remove_image(path)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)

    with Archive(tmp, 83, 'gms') as out:
        after = {p: semantic_hash(out.image(p)) for p, _ in out.images()}
        ui_old = None
        for path, h in before.items():
            if path in targets:
                continue
            if path in drop:
                if path in after:
                    os.unlink(tmp)
                    sys.exit(f'{path} was not removed')
                continue
            if after.get(path) != h:
                os.unlink(tmp)
                sys.exit(f'unexpected change in {path}')
        for path, h in wanted.items():
            if after.get(path) != h:
                os.unlink(tmp)
                sys.exit(f'{path} did not land')
        # the existing UIWindowEx nodes (MobHPRatioInfo, DailyCheckin, ...) are untouched
        with Archive(CUSTOM, 83, 'gms') as old:
            ui_old = {c.name: semantic_hash(c) for c in old.image(UI_IMG).children}
        ui_new = {c.name: semantic_hash(c) for c in out.image(UI_IMG).children if c.name != 'MonsterBook'}
        if any(ui_new.get(k) != v for k, v in ui_old.items() if k != 'MonsterBook'):
            os.unlink(tmp)
            sys.exit('unexpected change in UI/UIWindowEx.img')
    os.chmod(tmp, 0o644)
    os.replace(tmp, CUSTOM)
    digest = hashlib.sha256(CUSTOM.read_bytes()).hexdigest()
    print(f'wrote {CUSTOM.relative_to(ROOT)} sha256 {digest}')
    print(f'  {BOOK_IMG}: {report["entries"]} entries ({report["from_server"]} from the server, '
          f'{report["stock_only"]} kept stock, {report["reward_changed"]} with a changed Dropping list, '
          f'{report["maps_from_stock"]} with stock Found In maps), {book["drop_rows"]} drop rows')
    print(f'  removed {drop or "no"} partial Mob images; {UI_IMG}/MonsterBook/search: {len(SEARCH_ART)} canvases')
    return 0


if __name__ == '__main__':
    sys.exit(main())
