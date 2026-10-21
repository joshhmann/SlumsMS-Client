#!/usr/bin/env python3
"""Put the Daily Check-In window art into src/Custom.wz (runs on Linux; no WZ editor needed).

Copies UIWindow.img/DailyCheckin (backgrnd, 513x346) from the addon's packed UI.wz
(assets/dailycheckin/UI.wz) into Custom.wz as UI/UIWindowEx.img/DailyCheckin. UIWindowEx.img
does not exist in the stock client, so resman.cpp serves the whole image from Custom.wz (as for
MobHPRatioInfo); a new sub-property of the stock UIWindow.img would not be merged.

Idempotent: re-running replaces the node with the same bytes. Every other node of Custom.wz is
checked to be semantically unchanged before the file is replaced.

    python3 tools/wz/add_daily_checkin_art.py [--check]
"""
import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Archive, parse_img, write_img, semantic_hash, pixels  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CUSTOM = ROOT / 'src' / 'Custom.wz'
SOURCE = ROOT / 'assets' / 'dailycheckin' / 'UI.wz'
TARGET_IMG = 'UI/UIWindowEx.img'
NODE = 'DailyCheckin'


def snapshot(path):
    with Archive(path, 83, 'gms') as wz:
        return {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}, {
            p: wz.image(p) for p, _ in wz.images()}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has the art')
    args = ap.parse_args()

    with Archive(SOURCE, 83, 'gms') as src:
        art = src.image('UIWindow.img').get(NODE)
    bg = art.get('backgrnd')
    if (bg.value['width'], bg.value['height']) != (513, 346):
        sys.exit(f'unexpected art size {bg.value["width"]}x{bg.value["height"]}')
    want = semantic_hash(art)

    before, roots = snapshot(CUSTOM)
    present = roots[TARGET_IMG].child(NODE)
    if present is not None and semantic_hash(present) == want:
        print(f'{CUSTOM.relative_to(ROOT)} already has {TARGET_IMG}/{NODE} (art {want[:12]})')
        return 0
    if args.check:
        print(f'{CUSTOM.relative_to(ROOT)} lacks {TARGET_IMG}/{NODE}')
        return 1

    with Archive(CUSTOM, 83, 'gms') as wz:
        img = wz.image(TARGET_IMG)
        img.put(NODE, art, replace=True)
        blob = write_img(img, 'gms')
        reparsed, _ = parse_img(blob, TARGET_IMG, 'gms')
        if semantic_hash(reparsed) != semantic_hash(img):
            sys.exit('re-encoded image does not round-trip')
        wz.put_image(TARGET_IMG, blob, replace=True)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)

    after, new_roots = snapshot(tmp)
    for path, h in before.items():
        if path == TARGET_IMG:
            old = {c.name: semantic_hash(c) for c in roots[path].children}
            new = {c.name: semantic_hash(c) for c in new_roots[path].children}
            changed = [n for n in old if new.get(n) != old[n]]
            if changed or set(new) - set(old) != {NODE} and NODE not in old:
                os.unlink(tmp)
                sys.exit(f'unexpected change in {path}: {changed}')
        elif after.get(path) != h:
            os.unlink(tmp)
            sys.exit(f'unexpected change in {path}')
    if semantic_hash(new_roots[TARGET_IMG].get(NODE)) != want:
        os.unlink(tmp)
        sys.exit('art did not land')
    os.replace(tmp, CUSTOM)
    digest = hashlib.sha256(CUSTOM.read_bytes()).hexdigest()
    print(f'wrote {CUSTOM.relative_to(ROOT)}: {TARGET_IMG}/{NODE}/backgrnd 513x346 '
          f'(pixels {hashlib.sha256(pixels(bg)).hexdigest()[:12]}), sha256 {digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
