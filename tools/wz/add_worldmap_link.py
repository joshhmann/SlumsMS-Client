#!/usr/bin/env python3
"""Link the Temple of Time world map page to the Grandis pages in src/Custom.wz (1.6.1-rc11, Hyper Teleport Rock).

The client already ships the Grandis world map (Map/WorldMap/WorldMap180.img: Pantheon, Savage Terminal, Vulpes,
Ristonia, ... with every lower-Grandis map on a sub-page), but no page links to it: WorldMap080 (the Temple of Time)
links only WorldMap144 (Gate to the Future) and WorldMap179 (Gate of the Present). So the world map, and the Hyper
Teleport Rock that works through it, could reach Grandis only from inside Grandis. This adds

  Map/WorldMap/WorldMap080.img/MapLink/2   link/linkMap "WorldMap180", link/linkImg (assets/worldmap/grandis-link.png,
                                           origin 102,3: page pixel 218,232), toolTip "Grandis"

WorldMap080.img is a stock image, so resman.cpp merges this into it; MapLink/2 is a folder the stock image lacks,
which the Serialize hook adds whole since 1.6.1-rc11 (older DLLs ignore it). WorldMap180's own parentMap is
WorldMap080, so right-click leads back. Every other image of Custom.wz is checked to be unchanged.

    python3 tools/wz/add_worldmap_link.py [--stock-worldmap Data/Map/WorldMap] [--check]
"""
import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, Node, canvas, parse_img, write_img, semantic_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
ART = ROOT / 'assets' / 'worldmap' / 'grandis-link.png'
IMG = 'Map/WorldMap/WorldMap080.img'
LINK = '2'
TARGET = 'WorldMap180'
ORIGIN = (102, 3)        # base origin (320, 235) minus the art's page position (218, 232)


def link_node():
    art = Image.open(ART).convert('RGBA')
    return Node(LINK, 'property', children=[
        Node('link', 'property', children=[
            Node('linkMap', 'string', TARGET),
            canvas('linkImg', art, [Node('origin', 'vector', ORIGIN), Node('z', 'int', 1)])]),
        Node('toolTip', 'string', 'Grandis')])


def check_stock(folder):
    """The stock pages must be the ones this link was drawn for."""
    folder = Path(folder)
    page, _ = parse_img((folder / 'WorldMap080.img').read_bytes(), 'WorldMap080.img')
    links = {c.name: c.get('link/linkMap').value for c in page.get('MapLink').children}
    if links != {'0': 'WorldMap144', '1': 'WorldMap179'}:
        sys.exit(f'stock WorldMap080 links changed: {links}')
    if page.get('BaseImg').children[0].child('origin').value != (320, 235):
        sys.exit('stock WorldMap080 base image moved')
    grandis, _ = parse_img((folder / f'{TARGET}.img').read_bytes(), f'{TARGET}.img')
    if grandis.get('info/parentMap').value != 'WorldMap080':
        sys.exit(f'stock {TARGET} is not a Temple of Time sub-page')
    print(f'stock pages OK: WorldMap080 links {sorted(links.values())}, {TARGET} parent WorldMap080')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stock-worldmap', help="the client's Data/Map/WorldMap folder (checks the stock pages)")
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has the link')
    args = ap.parse_args()
    if args.stock_worldmap:
        check_stock(args.stock_worldmap)

    want = link_node()
    with Archive(CUSTOM, 83, 'gms') as wz:
        before = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
        have = wz.image(IMG) if IMG in before else None
    present = have.get('MapLink').child(LINK) if have is not None and have.child('MapLink') else None
    if present is not None and semantic_hash(present) == semantic_hash(want):
        print(f'{CUSTOM.relative_to(ROOT)} already has {IMG}/MapLink/{LINK}')
        return 0
    if args.check:
        print(f'{CUSTOM.relative_to(ROOT)} lacks {IMG}/MapLink/{LINK}')
        return 1

    root = Node(IMG, 'property', children=[Node('MapLink', 'property', children=[want])])
    blob = write_img(root, 'gms')
    reparsed, _ = parse_img(blob, IMG, 'gms')
    if semantic_hash(reparsed) != semantic_hash(root):
        sys.exit('re-encoded image does not round-trip')
    with Archive(CUSTOM, 83, 'gms') as wz:
        wz.put_image(IMG, blob, replace=IMG in before)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)
    with Archive(tmp, 83, 'gms') as wz:
        after = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
        landed = semantic_hash(wz.image(IMG).get(f'MapLink/{LINK}'))
    changed = [p for p in before if p != IMG and after.get(p) != before[p]]
    if changed or set(after) - set(before) - {IMG} or landed != semantic_hash(want):
        os.unlink(tmp)
        sys.exit(f'unexpected change: {changed or sorted(set(after) - set(before))}')
    os.replace(tmp, CUSTOM)
    digest = hashlib.sha256(CUSTOM.read_bytes()).hexdigest()
    art = hashlib.sha256(ART.read_bytes()).hexdigest()
    print(f'wrote {CUSTOM.relative_to(ROOT)}: {IMG}/MapLink/{LINK} -> {TARGET} (art {art[:12]}), sha256 {digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
