#!/usr/bin/env python3
"""Give newer mobs the attackN/info/type the v83 client reads (b251, Rose Garden 211080100) in src/Custom.wz.

The v83 attack-info builder (0x0067FD81) reads <mob>.img/attackN/info/type (string-pool 0x5EB, stored at record+0xC).
Type 0 -- also what a missing key reads as -- is the box range: the builder then takes range/lt and range/rb
(0x1604 / 0x160F) as vectors and raises E_POINTER through 0x0068069C when either is absent. Newer data moved the
type to <mob>.img/info/attack/<N-1>/type and gives circle ranges (range/r + range/sp), so every such attack ends the
session the first time the client builds it (Rose Garden's 8211000: the map shows, then the client closes).

The v83 meanings are the same as the newer ones for the two values this copies (stock v83: 118 circle attacks are
type 1, 110 circle attacks with a ball are type 2), so for every listed attack this adds

  Mob/<id>.img/attackN/info/type   int, copied from <id>.img/info/attack/<N-1>/type

Mob images are stock images, so resman.cpp merges these leaves into them; nothing else of the mob changes.
mob_attack_type.json is the list: client attacks with no attackN/info/type, a circle range, and a newer type of 1
or 2 (`--scan` rebuilds it from a client's Data/Mob). Other types the v83 builder does not know stay out.

    python3 tools/wz/add_mob_attack_type.py [--scan <client Data/Mob>] [--check]
"""
import argparse
import hashlib
import json
import os
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, Node, parse_img, write_img, semantic_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
LIST = Path(__file__).resolve().parent / 'mob_attack_type.json'
V83_TYPES = (1, 2)   # circle melee, circle with a ball: the newer values v83 reads the same way


def children(node):
    return {c.name: c for c in node.children} if node is not None else {}


def needs_type(mob_root):
    """(attack, type) pairs of one mob image that the v83 builder would fail on and the newer data can fix."""
    kids = children(mob_root)
    info_attack = children(children(kids.get('info')).get('attack'))
    out = []
    for name, act in kids.items():
        if not (name.startswith('attack') and name[6:].isdigit()):
            continue
        ainfo = children(act).get('info')
        if ainfo is None:
            continue
        keys = children(ainfo)
        if 'type' in keys:
            continue
        shape = set(children(keys.get('range')))
        if not {'r', 'sp'} <= shape or {'lt', 'rb'} <= shape:
            continue
        entry = children(info_attack.get(str(int(name[6:]) - 1)))
        t = entry.get('type')
        if t is not None and t.value in V83_TYPES:
            out.append((name, t.value))
    return out


def scan(folder):
    rows = []
    for f in sorted(Path(folder).glob('*.img')):
        root, _ = parse_img(f.read_bytes(), f.name)
        rows += [dict(mob=f.stem, attack=a, type=t) for a, t in needs_type(root)]
    return rows


def mob_image(mob, attacks):
    return Node(f'Mob/{mob}.img', 'property', children=[
        Node(a, 'property', children=[Node('info', 'property', children=[Node('type', 'int', t)])])
        for a, t in sorted(attacks)])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scan', help="rebuild mob_attack_type.json from a client's Data/Mob folder first")
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has every leaf')
    args = ap.parse_args()
    if args.scan:
        rows = scan(args.scan)
        LIST.write_text('[\n' + ',\n'.join(json.dumps(r) for r in rows) + '\n]\n')
        print(f'{LIST.name}: {len(rows)} attacks in {len({r["mob"] for r in rows})} mobs')

    per_mob = defaultdict(list)
    for r in json.loads(LIST.read_text()):
        if r['type'] not in V83_TYPES:
            sys.exit(f'{r}: type {r["type"]} is not one v83 reads the same way')
        per_mob[r['mob']].append((r['attack'], r['type']))
    want = {f'Mob/{m}.img': mob_image(m, a) for m, a in per_mob.items()}

    with Archive(CUSTOM, 83, 'gms') as wz:
        before = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
    todo = {p: n for p, n in want.items() if before.get(p) != semantic_hash(n)}
    stale = sorted(p for p in before if p.startswith('Mob/') and p not in want)
    if not todo and not stale:
        print(f'{CUSTOM.relative_to(ROOT)} already has all {len(want)} mob images')
        return 0
    if args.check:
        print(f'{CUSTOM.relative_to(ROOT)} lacks {len(todo)} of {len(want)} mob images, {len(stale)} stale')
        return 1

    with Archive(CUSTOM, 83, 'gms') as wz:
        for path, node in todo.items():
            blob = write_img(node, 'gms')
            if semantic_hash(parse_img(blob, path, 'gms')[0]) != semantic_hash(node):
                sys.exit(f'{path}: re-encoded image does not round-trip')
            wz.put_image(path, blob, replace=path in before)
        for path in stale:
            wz.remove_image(path)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)
    with Archive(tmp, 83, 'gms') as wz:
        after = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
    changed = [p for p in before if not p.startswith('Mob/') and after.get(p) != before[p]]
    wrong = [p for p, n in want.items() if after.get(p) != semantic_hash(n)]
    extra = [p for p in after if p not in before and p not in want]
    if changed or wrong or extra:
        os.unlink(tmp)
        sys.exit(f'unexpected change: changed {changed[:5]} wrong {wrong[:5]} extra {extra[:5]}')
    os.replace(tmp, CUSTOM)
    digest = hashlib.sha256(CUSTOM.read_bytes()).hexdigest()
    print(f'wrote {CUSTOM.relative_to(ROOT)}: {len(want)} mob images, '
          f'{sum(len(a) for a in per_mob.values())} attackN/info/type leaves, sha256 {digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
