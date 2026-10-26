#!/usr/bin/env python3
"""Put the SlumsMS wordmark on the login screen in place of the MapleStory logo.

The login screen's logo is Map/Obj/login.img/Title/logo/0: two canvases, frame 0 fading in (a0 0 -> a1 255) and
frame 1 holding (a0 255), both 397x219 with origin (198, 180), so the logo's centre is at the object's x and its
bottom 39 px below the object's y. This replaces both frames with the given art, scaled to --width, keeping the
stock anchor (centred, bottom 39 px below) moved up --lift and left --left and the frames' delays and alpha keys. Runs on Linux.

    python3 tools/login-logo/set_logo.py <Map/Obj/login.img> assets/brand/slumsms-wordmark.png [--width 440]

Prints the result and the file size; stage the changed Data/Map/Obj/login.img.
"""
import argparse
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'wz'))
from native_wz import Node, canvas, parse_img, write_img  # noqa: E402

STOCK_BELOW = 219 - 180   # the stock logo's bottom edge sits this far below the object's position
# The stock canvas has empty space under and right of "MapleStory"; art that fills its canvas sits lower and further
# right on the same anchor, so it is lifted and moved left by default (checked on the 800x600 login, 2026-10-26).
LIFT, LEFT = 45, 20


def fit(art, width):
    im = Image.open(art).convert('RGBA')
    # Drop near-invisible halo pixels around the art before cropping to it.
    alpha = im.getchannel('A').point(lambda p: p if p > 8 else 0)
    im.putalpha(alpha)
    im = im.crop(alpha.getbbox())
    height = round(im.height * width / im.width)
    return im.resize((width, height), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('login', type=Path, help='the client Data/Map/Obj/login.img to update in place')
    ap.add_argument('art', type=Path, help='the logo, a PNG with transparency')
    ap.add_argument('--width', type=int, default=440, help='logo width in pixels on the 800x600 login screen')
    ap.add_argument('--lift', type=int, default=LIFT, help='pixels above the stock anchor')
    ap.add_argument('--left', type=int, default=LEFT, help='pixels left of the stock anchor')
    a = ap.parse_args()

    root, key = parse_img(a.login.read_bytes(), a.login.name)
    logo = root.get('Title/logo/0')
    if logo is None or [c.name for c in logo.children if c.kind == 'canvas'] != ['0', '1']:
        sys.exit('no Title/logo/0 with frames 0 and 1 in ' + str(a.login))
    im = fit(a.art, a.width)
    origin = (im.width // 2 + a.left, im.height - STOCK_BELOW + a.lift)
    for i, old in enumerate(c for c in logo.children if c.kind == 'canvas'):
        keep = [c for c in old.children if c.name != 'origin']   # z, delay, a0, a1
        new = canvas(old.name, im, [Node('origin', 'vector', origin)] + keep)
        logo.children[logo.children.index(old)] = new
    before = a.login.stat().st_size
    a.login.write_bytes(write_img(root, key))
    print(f'Title/logo/0: {im.width}x{im.height}, origin {origin}')
    print(f'{a.login}: {before / 1e6:.2f} MB -> {a.login.stat().st_size / 1e6:.2f} MB')


if __name__ == '__main__':
    main()
