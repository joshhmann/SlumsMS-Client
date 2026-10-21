#!/usr/bin/env python3
"""Build the Coloring Prism's RESET button art (assets/colorprism/BtReset/*.png) from the client's own UI.

The window's footer uses the stock 47x18 UI/Basic.img/BtOK2 and BtCancel2 pair, and the client has no stock
button that says RESET (UIWindow.img/TradingRoom/BtReset and CashTradingRoom/BtReset say CANCEL and Korean).
So the three states the window draws are made from BtCancel2 itself:

  * the letters come from stock buttons in the same 5x7 pixel face: R from BtReceive, E and S from BtSend,
    T from BtDel (only their shapes are taken, as masks);
  * every pixel of the lettering band is then painted from a table learned off BtCancel2, BtSend and BtOK2
    in the SAME state: the letter colour, and for each pixel around the letters the colour those buttons use
    for the same row and the same 8-neighbour pattern of letter pixels (their drop shadow). That keeps the
    state's own gradient and shadow, so RESET sits next to OK and CANCEL as if it shipped with them.

Re-painting CANCEL from its own mask with the same table reproduces the stock BtCancel2 to within a few pixels
(printed as a check). Needs Pillow and pycryptodome.

    python3 tools/wz/make_prism_reset_button.py <client Data>/UI/Basic.img
"""
import argparse
import sys
from collections import Counter, defaultdict
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import parse_img, pixels  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'assets' / 'colorprism' / 'BtReset'
STATES = ('normal', 'pressed', 'mouseOver')   # the window never disables Reset; BtCancel2's grey
                                              # disabled letters do not separate from its background
BAND = range(4, 15)          # rows the lettering and its shadow occupy on these 18px buttons
PAINT_X = range(5, 43)       # BtCancel2's lettering and shadow sit in x 6..41
RESET_X = 9                  # RESET is 29px wide; this centres it where CANCEL's 34px sit
NEIGHBOURS = ((-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1))


def canvas_image(root, path):
    node = root.get(path)
    v = node.value
    if v['format'] + v['format2'] != 2:
        sys.exit(f'{path}: expected a BGRA8888 canvas')
    return Image.frombytes('RGBA', (v['width'], v['height']), pixels(node), 'raw', 'BGRA')


def is_letter(px):
    r, g, b, a = px
    return a and min(r, g, b) >= 0xBB and r + g + b >= 3 * 0xC8


def letter_mask(im):
    w, _ = im.size
    return {(x, y) for y in range(5, 14) for x in range(3, w - 4) if is_letter(im.getpixel((x, y)))}


def glyphs(im):
    """Letter masks, left to right, each shifted to start at x 0."""
    mask = letter_mask(im)
    columns = sorted({x for x, _ in mask})
    runs, run = [], []
    for x in columns:
        if run and x != run[-1] + 1:
            runs.append(run)
            run = []
        run.append(x)
    if run:
        runs.append(run)
    return [{(x - r[0], y) for x, y in mask if r[0] <= x <= r[-1]} for r in runs]


def key(mask, x, y):
    return sum(1 << i for i, (dx, dy) in enumerate(NEIGHBOURS) if (x + dx, y + dy) in mask)


def learn(samples):
    seen = defaultdict(Counter)
    for im in samples:
        mask = letter_mask(im)
        w, _ = im.size
        for y in BAND:
            for x in range(3, w - 4):
                px = im.getpixel((x, y))
                if (x, y) in mask:
                    seen[('letter', y)][px] += 1
                else:
                    k = key(mask, x, y)
                    seen[(y, k)][px] += 1
                    seen[(y, 'count', bin(k).count('1'))][px] += 1
    return {k: c.most_common(1)[0][0] for k, c in seen.items()}


def paint(base, mask, table):
    im = base.copy()
    for y in BAND:
        for x in PAINT_X:
            if (x, y) in mask:
                px = table[('letter', y)]
            else:
                k = key(mask, x, y)
                px = table.get((y, k)) or table[(y, 'count', bin(k).count('1'))]
            im.putpixel((x, y), px)
    return im


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('basic_img', type=Path, help="the client's UI/Basic.img")
    args = ap.parse_args()
    root, _ = parse_img(args.basic_img.read_bytes(), 'Basic.img', None)

    send, receive, delete = (glyphs(canvas_image(root, f'{b}/normal/0')) for b in ('BtSend', 'BtReceive', 'BtDel'))
    letters = [receive[0], send[1], send[0], send[1], delete[4]]          # R E S E T
    reset, x = set(), RESET_X
    for g in letters:
        reset |= {(gx + x, gy) for gx, gy in g}
        x += max(gx for gx, _ in g) + 2

    OUT.mkdir(parents=True, exist_ok=True)
    for state in STATES:
        base = canvas_image(root, f'BtCancel2/{state}/0')
        table = learn([base] + [canvas_image(root, f'{b}/{state}/0') for b in ('BtSend', 'BtOK2')])
        again = paint(base, {(x, y) for x, y in letter_mask(base) if x < 44}, table)
        off = sum(again.getpixel(p) != base.getpixel(p) for p in ((x, y) for x in PAINT_X for y in BAND))
        paint(base, reset, table).save(OUT / f'{state}.png')
        print(f'{state}: wrote {OUT.relative_to(ROOT)}/{state}.png '
              f'(re-painting CANCEL differs from stock in {off} of {len(PAINT_X) * len(BAND)} px)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
