#!/usr/bin/env python3
"""Drop the rebirth warning from the two Lion King's Castle portal tooltips in src/Custom.wz (1.6.4).

The client's String/ToolTipHelp.img gives the two portals into Lion King's Castle

  Mapobject/211040600/0   Title "Road to Lion King's Castle"   Desc "WARNING: Must have at least 1 rebirth"
  Mapobject/211060000/0   Title "To the Lion King's Castle"    Desc "WARNING: Must have at least 1 rebirth"

1.6.4 removes that gate on the server (scripts/portal/in_lkc.js, 211060000.4.js), so the warning would be wrong.
A Custom.wz merge can replace a leaf but not delete one, so Desc becomes the castle's own world-map line instead.
ToolTipHelp.img is a stock image: resman.cpp merges these two leaves into it; every other image is checked unchanged.

    python3 tools/wz/set_lkc_tooltip.py [--stock <client Data/String/ToolTipHelp.img>] [--check]
"""
import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, Node, parse_img, write_img, semantic_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
IMG = 'String/ToolTipHelp.img'
PORTALS = ('211040600', '211060000')
OLD = 'WARNING: Must have at least 1 rebirth'
NEW = "The path to the Black Mage's general."   # the castle's line on the El Nath world map


def want():
    return Node(IMG, 'property', children=[Node('Mapobject', 'property', children=[
        Node(m, 'property', children=[Node('0', 'property', children=[Node('Desc', 'string', NEW)])])
        for m in PORTALS])])


def check_stock(path):
    root, _ = parse_img(Path(path).read_bytes(), IMG)
    for m in PORTALS:
        desc = root.get(f'Mapobject/{m}/0/Desc')
        if desc is None or desc.value != OLD:
            sys.exit(f'stock {IMG} Mapobject/{m}/0/Desc is {desc.value if desc else None!r}, not the rebirth warning')
    print(f'stock {IMG}: both portals carry the rebirth warning')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stock', help="the client's Data/String/ToolTipHelp.img (checks the warning is there)")
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has the change')
    args = ap.parse_args()
    if args.stock:
        check_stock(args.stock)

    node = want()
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
    print(f'wrote {CUSTOM.relative_to(ROOT)}: {IMG} Desc for {", ".join(PORTALS)}, '
          f'sha256 {hashlib.sha256(CUSTOM.read_bytes()).hexdigest()}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
