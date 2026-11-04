#!/usr/bin/env python3
"""Give newer mobs the attackN/info/type the v83 client reads (b251, Rose Garden 211080100) in src/Custom.wz.

The v83 attack-info builder (0x0067FD81) reads <mob>.img/attackN/info/type (string-pool 0x5EB, stored at record+0xC).
Type 0 -- also what a missing key reads as -- is the box range: the builder then takes range/lt and range/rb
(0x1604 / 0x160F) as vectors and raises E_POINTER through 0x0068069C when either is absent. Newer data moved the
type to <mob>.img/info/attack/<N-1>/type and gives circle ranges (range/r + range/sp), so every such attack ends the
session the first time the client builds it (Rose Garden's 8211000: the map shows, then the client closes).

The v83 builder knows three range shapes and nothing else (0x00680593 dispatch, 0x0068071B compares 3 and 4):
type 0 is the box range (range/lt + range/rb, both raised at 0x00680694/0x006806B7), types 3 and 4 are the attack
tiles (start/areaCount/attackCount plus the same lt/rb, raised at 0x00680AAD), and every other value -- 1, 2, 9,
13, 201, anything not zero -- takes one circle branch: range/sp is required (E_POINTER at 0x006807C6) and range/r
is an optional int. No other key of attackN/info is ever required: the effect/hit/ball/areaWarning art and the
attach/attachfacing/effectAfter/attackAfter ints are all read tolerantly in the common tail (0x00680B71-0x006811D6).
Stock v83 spells the circle two ways: type 1 without a ball, type 2 with one (of the 679 attacks already listed,
264 are type 1; the pack's four stock references agree -- 7130000, 3300004 and 5120003 attack1 are type 1 with no
info/ball, 8140703 attack1 is type 2 with it).

So a newer attack gets the v83 type that reads the data it already carries: TYPE_MAP for the newer values it knows,
the newer value itself when it is 1 or 2, and for older data with no newer type the ball art picks 2 or 1. For every
listed attack this adds

  Mob/<id>.img/attackN/info/type   int, from TYPE_MAP or from the newer value itself when v83 shares it

Mob images are stock images, so resman.cpp merges these leaves into them; nothing else of the mob changes.
mob_attack_type.json is the list (`--scan` rebuilds it from a client's Data/Mob, `--scan --merge` only appends).
Attacks the pack gives no buildable shape (no range node, a range without sp) or a newer type TYPE_MAP does not
know stay out: v83 would need art or a range that is not in the data, so they are only reported.

    python3 tools/wz/add_mob_attack_type.py [--scan <client Data/Mob> [--merge]] [--check]
"""
import argparse
import hashlib
import json
import os
import sys
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, Node, parse_img, write_img, semantic_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
LIST = Path(__file__).resolve().parent / 'mob_attack_type.json'
V83_TYPES = (1, 2)   # circle melee, circle with a ball: the newer values v83 reads the same way

# newer (post-v83) attack type -> (v83 type this list writes, why). All of these take the circle branch in v83, so
# the choice is only "is the attack a ball attack (2) or a plain circle (1)". Types 3, 4 (attack tiles), 0 (box) and
# anything else stay out: v83 would read a different set of keys than the attack carries.
TYPE_MAP = {
    9: (2, 'bullet attack (bulletSpeed) with info/ball art: circle r+sp, v83 type 2 (ball)'),
    11: (1, 'mob-skill attack (userSkill), no info/ball art: plain circle, v83 type 1'),
    13: (2, 'bullet attack (bulletSpeed) with info/ball art: circle r+sp, v83 type 2 (ball)'),
    14: (1, 'magic attack (conMP, magic), no info/ball art: plain circle, v83 type 1'),
    19: (1, 'close melee (attackRatio, cooltime, fixDamR), no info/ball art: plain circle, v83 type 1'),
    21: (1, 'ballAngle attack, but no info/ball art in the data: plain circle, v83 type 1'),
    201: (2, 'ball from ballStart to ballEnd with info/ball art: circle r+sp, v83 type 2 (ball)'),
}


def children(node):
    return {c.name: c for c in node.children} if node is not None else {}


def classify(mob_root):
    """((attack, v83 type) to write, (attack, why it stays out)) for one mob image.

    An attack is only touched when attackN/info has no type: with none, the builder reads 0, the box range, and
    raises E_POINTER when range/lt or range/rb is missing. A box range in the data is therefore left alone (v83
    already reads it as type 0); a circle needs range/sp, which v83 can build as type 1 or 2.
    """
    kids = children(mob_root)
    info_attack = children(children(kids.get('info')).get('attack'))
    rows, out = [], []
    for name, act in kids.items():
        if not (name.startswith('attack') and name[6:].isdigit()):
            continue
        ainfo = children(act).get('info')
        if ainfo is None or 'type' in children(ainfo):
            continue
        keys = children(ainfo)
        shape = set(children(keys.get('range')))
        if {'lt', 'rb'} <= shape:
            continue
        if 'sp' not in shape:
            out.append((name, 'no range/sp: ' + ('attack has no range node' if not shape
                       else f'range has only {", ".join(sorted(shape))}')))
            continue
        entry = children(info_attack.get(str(int(name[6:]) - 1)))
        t = entry.get('type')
        if t is None:   # old-style data: no newer type to translate, take the shape it already has
            rows.append((name, 2 if 'ball' in keys else 1))
        elif t.value in V83_TYPES:   # the newer data still says 1 or 2: v83 reads it the same way
            rows.append((name, t.value))
        elif t.value in TYPE_MAP:
            rows.append((name, TYPE_MAP[t.value][0]))
        else:
            out.append((name, f'newer type {t.value} is not in TYPE_MAP'))
    return rows, out


def needs_type(mob_root):
    """(attack, type) pairs of one mob image that the v83 builder would fail on and the data can fix."""
    return classify(mob_root)[0]


def scan(folder):
    """(list rows, (mob, attack, why) left out) for every mob image in a client's Data/Mob folder."""
    rows, left = [], []
    for f in sorted(Path(folder).glob('*.img')):
        root, _ = parse_img(f.read_bytes(), f.name)
        keep, out = classify(root)
        rows += [dict(mob=f.stem, attack=a, type=t) for a, t in keep]
        left += [(f.stem, a, why) for a, why in out]
    return rows, left


def mob_image(mob, attacks):
    return Node(f'Mob/{mob}.img', 'property', children=[
        Node(a, 'property', children=[Node('info', 'property', children=[Node('type', 'int', t)])])
        for a, t in sorted(attacks)])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scan', help="rebuild mob_attack_type.json from a client's Data/Mob folder first")
    ap.add_argument('--merge', action='store_true', help='with --scan: append to the list instead of replacing it')
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has every leaf')
    args = ap.parse_args()
    if args.scan:
        rows, left = scan(args.scan)
        if args.merge:
            known = {(r['mob'], r['attack']) for r in json.loads(LIST.read_text())}
            new = [r for r in rows if (r['mob'], r['attack']) not in known]
            rows = json.loads(LIST.read_text()) + new
            print(f'{LIST.name}: +{len(new)} attacks')
        LIST.write_text('[\n' + ',\n'.join(json.dumps(r) for r in rows) + '\n]\n')
        print(f'{LIST.name}: {len(rows)} attacks in {len({r["mob"] for r in rows})} mobs')
        outs = Counter(why for _, _, why in left)
        print(f'left out: {len(left)} attacks -- ' + '; '.join(f'{n}x {why}' for why, n in sorted(outs.items())))
        for mob, attack, why in left:
            print(f'  {mob}/{attack}: {why}')

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
