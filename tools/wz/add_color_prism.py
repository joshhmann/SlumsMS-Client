#!/usr/bin/env python3
"""Put the Coloring Prism window art and item 5782000 into src/Custom.wz (Linux; no WZ editor needed).

Sources (assets/colorprism/, BunsMS-authored, see authored-provenance.json and docs/PROVENANCE.md):
  ColorPrism.img  -> UI/UIWindowEx.img/ColorPrism: backgrnd + backgrndLook (301x406) and the
                     trackTone/trackChroma/trackBright slider gradients (176x8). The authored
                     LayerBt chips are not used by coloringprism.cpp and are left out.
                     UIWindowEx.img is not a stock image, so resman.cpp serves the whole node
                     from Custom.wz; a new sub-property of the stock UIWindow.img would not merge.
  0578.img        -> Item/Cash/0578.img, a NEW image (the stock client has no 0578.img), with
                     05782000/info icon + iconRaw (34x33), cash 1, slotMax 100. The authored
                     icons carry origin (0,0); stock cash icons sit on origin (0, height), so the
                     origin is rewritten to (0,33) or the icon draws one slot too low.
The item NAME (String/Cash.img/5782000) is a sub-node of a stock image and cannot be added here.

Idempotent: re-running with the same assets changes nothing. Every other image of Custom.wz is
checked to be semantically unchanged before the file is replaced.

    python3 tools/wz/add_color_prism.py [--check]
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
ASSETS = ROOT / 'assets' / 'colorprism'
UI_IMG = 'UI/UIWindowEx.img'
UI_NODE = 'ColorPrism'
ITEM_IMG = 'Item/Cash/0578.img'
WANT_UI = {'backgrnd': (301, 406), 'backgrndLook': (301, 406),
           'trackTone': (176, 8), 'trackChroma': (176, 8), 'trackBright': (176, 8)}


def build_ui():
    src, _ = parse_img((ASSETS / 'ColorPrism.img').read_bytes(), 'ColorPrism.img', None)
    node = src.get(UI_NODE)
    node.children = [c for c in node.children if c.name in WANT_UI]
    for name, (w, h) in WANT_UI.items():
        c = node.child(name)
        if c is None or c.kind != 'canvas' or (c.value['width'], c.value['height']) != (w, h):
            sys.exit(f'ColorPrism.img: {name} missing or not {w}x{h}')
    return node


def build_item():
    root, _ = parse_img((ASSETS / '0578.img').read_bytes(), ITEM_IMG, None)
    info = root.get('05782000/info')
    for name in ('icon', 'iconRaw'):
        c = info.get(name)
        if c.kind != 'canvas':
            sys.exit(f'0578.img: {name} is not a canvas')
        origin = c.child('origin')
        origin.value = (0, c.value['height'])
    for name, want in (('cash', 1), ('slotMax', 100)):
        if info.get(name).value != want:
            sys.exit(f'0578.img: info/{name} is not {want}')
    return root


def images(path):
    with Archive(path, 83, 'gms') as wz:
        return {p: wz.image(p) for p, _ in wz.images()}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz is up to date')
    args = ap.parse_args()

    ui, item = build_ui(), build_item()
    want_ui, want_item = semantic_hash(ui), semantic_hash(item)
    before = images(CUSTOM)
    have_ui = before.get(UI_IMG) and before[UI_IMG].child(UI_NODE)
    up_to_date = (have_ui is not None and semantic_hash(have_ui) == want_ui
                  and ITEM_IMG in before and semantic_hash(before[ITEM_IMG]) == want_item)
    if up_to_date:
        print(f'{CUSTOM.relative_to(ROOT)} already has {UI_IMG}/{UI_NODE} and {ITEM_IMG}')
        return 0
    if args.check:
        print(f'{CUSTOM.relative_to(ROOT)} lacks the Coloring Prism art or item')
        return 1

    with Archive(CUSTOM, 83, 'gms') as wz:
        img = wz.image(UI_IMG)
        img.put(UI_NODE, ui, replace=True)
        for path, root in ((UI_IMG, img), (ITEM_IMG, item)):
            blob = write_img(root, 'gms')
            reparsed, _ = parse_img(blob, path, 'gms')
            if semantic_hash(reparsed) != semantic_hash(root):
                sys.exit(f'{path}: re-encoded image does not round-trip')
            wz.put_image(path, blob, replace=True)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)

    after = images(tmp)
    for path, old in before.items():
        if path == UI_IMG:
            o = {c.name: semantic_hash(c) for c in old.children if c.name != UI_NODE}
            n = {c.name: semantic_hash(c) for c in after[path].children if c.name != UI_NODE}
            if o != n:
                os.unlink(tmp)
                sys.exit(f'unexpected change in {path}')
        elif path != ITEM_IMG and semantic_hash(after[path]) != semantic_hash(old):
            os.unlink(tmp)
            sys.exit(f'unexpected change in {path}')
    if (semantic_hash(after[UI_IMG].get(UI_NODE)) != want_ui
            or semantic_hash(after[ITEM_IMG]) != want_item):
        os.unlink(tmp)
        sys.exit('art did not land')
    os.replace(tmp, CUSTOM)
    digest = hashlib.sha256(CUSTOM.read_bytes()).hexdigest()
    print(f'wrote {CUSTOM.relative_to(ROOT)}: {UI_IMG}/{UI_NODE} ({want_ui[:12]}), '
          f'{ITEM_IMG} ({want_item[:12]}), sha256 {digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
