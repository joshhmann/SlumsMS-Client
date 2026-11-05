#!/usr/bin/env python3
"""Supply the Time Temple object art newer maps place but the v83 client lacks, in src/Custom.wz (1.6.4).

Nine maps (272000000, 17021, 18021, 321190500, 327090010, 327090320, 328000000, 328090100, 328090200) place

  Map/Obj/acc10.img/timeTemple/pillar_old/<0..10>   (384 objects)
  Map/Obj/acc10.img/timeTemple/door/10              (3 objects)

which the client's acc10.img does not have (its timeTemple holds flying, door, foot, pillar, stair, acc). slums.dll
skips such an object since a1e48df, so the maps load without the pillars; this puts them back. Each missing node is a
copy of its stock sibling under the missing name: pillar_old/<n> = pillar/<n>, door/10 = door/1. No art from other
clients. acc10.img is a stock image: resman.cpp merges timeTemple one level down, adds pillar_old whole and door/10
into the stock door. Every other Custom.wz image is checked unchanged.

    python3 tools/wz/add_time_temple_art.py --stock <client Data/Map/Obj/acc10.img> [--check]
"""
import argparse
import copy
import hashlib
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, Node, parse_img, write_img, semantic_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
IMG = 'Map/Obj/acc10.img'
PILLARS = [str(n) for n in range(11)]          # every pillar_old index a map places
DOOR, DOOR_DONOR = '10', '1'


def renamed(node, name):
    node = copy.deepcopy(node)
    node.name = name
    return node


def want(stock):
    tt = stock.get('timeTemple')
    if tt.child('pillar_old') is not None or tt.get('door').child(DOOR) is not None:
        sys.exit(f'stock {IMG} already has timeTemple/pillar_old or door/{DOOR}; nothing to supply')
    pillar = tt.get('pillar')
    return Node(IMG, 'property', children=[Node('timeTemple', 'property', children=[
        Node('pillar_old', 'property', children=[renamed(pillar.get(n), n) for n in PILLARS]),
        Node('door', 'property', children=[renamed(tt.get(f'door/{DOOR_DONOR}'), DOOR)])])])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stock', required=True, help="the client's Data/Map/Obj/acc10.img (the donor art)")
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has the change')
    args = ap.parse_args()

    node = want(parse_img(Path(args.stock).read_bytes(), IMG)[0])
    with Archive(CUSTOM, 83, 'gms') as wz:
        before = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
    if before.get(IMG) == semantic_hash(node):
        print(f'{CUSTOM.relative_to(ROOT)} already has {IMG}')
        return 0
    if args.check:
        print(f'{CUSTOM.relative_to(ROOT)} lacks {IMG}')
        return 1
    blob = write_img(node, 'gms')
    if semantic_hash(parse_img(blob, IMG, 'gms')[0]) != semantic_hash(node):
        sys.exit('re-encoded image does not round-trip')
    with Archive(CUSTOM, 83, 'gms') as wz:
        wz.put_image(IMG, blob, replace=IMG in before)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)
    with Archive(tmp, 83, 'gms') as wz:
        after = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
    changed = [p for p in before if p != IMG and after.get(p) != before[p]]
    if changed or set(after) - set(before) - {IMG} or after.get(IMG) != semantic_hash(node):
        os.unlink(tmp)
        sys.exit(f'unexpected change: {changed or sorted(set(after) - set(before))}')
    os.replace(tmp, CUSTOM)
    print(f'wrote {CUSTOM.relative_to(ROOT)}: {IMG} timeTemple/pillar_old/0-10 and door/{DOOR}, '
          f'sha256 {hashlib.sha256(CUSTOM.read_bytes()).hexdigest()}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
