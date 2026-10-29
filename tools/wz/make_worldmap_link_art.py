#!/usr/bin/env python3
"""Draw the Temple of Time world map's "Grandis" link (assets/worldmap/grandis-link.png, 86x90, our own art).

The stock page (Map/WorldMap/WorldMap080.img) links the Gate to the Future and the Gate of the Present, each a lit
copy of its gate with a ribbon label. The third gate, right of the Gate of the Present, gets a gold glow over its
doorway and a "Grandis" ribbon; tools/wz/add_worldmap_link.py bakes the PNG into Custom.wz as MapLink/2. The PNG is
committed; re-run this only to change the art (it needs the DejaVu Serif Bold font).

    python3 tools/wz/make_worldmap_link_art.py [--preview stock-WorldMap080.png out.png]
"""
import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'assets' / 'worldmap' / 'grandis-link.png'
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf'

# Page pixels (WorldMap080 BaseImg, 640x472, origin 320,235): where the art sits and the third gate's doorway.
X0, Y0, W, H = 218, 232, 86, 90
DOOR = (237, 247, 264, 294)


def arch(draw, box, **kw):
    x0, y0, x1, y1 = box
    r = (x1 - x0) // 2
    draw.pieslice((x0, y0, x1, y0 + 2 * r), 180, 360, **kw)
    draw.rectangle((x0, y0 + r, x1, y1), **kw)


def layer(rgb, alpha):
    out = Image.new('RGBA', alpha.size, rgb + (0,))
    out.putalpha(alpha)
    return out


def make():
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    door = (DOOR[0] - X0, DOOR[1] - Y0, DOOR[2] - X0, DOOR[3] - Y0)
    inner = Image.new('L', (W, H), 0)
    arch(ImageDraw.Draw(inner), door, fill=255)
    halo = Image.new('L', (W, H), 0)
    arch(ImageDraw.Draw(halo), (door[0] - 4, door[1] - 4, door[2] + 4, door[3] + 2), fill=255)
    halo = ImageChops.subtract(halo.filter(ImageFilter.GaussianBlur(3)), inner).point(lambda v: int(v * 0.9))
    img.alpha_composite(layer((255, 240, 170), halo))
    img.alpha_composite(layer((255, 226, 120), inner.point(lambda v: int(v * 0.42))))
    rim = Image.new('L', (W, H), 0)
    arch(ImageDraw.Draw(rim), door, outline=255, width=2)
    img.alpha_composite(layer((255, 248, 210), rim))

    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, 10)
    text = 'Grandis'
    tw = draw.textlength(text, font=font)
    lx0, lx1, ly0, ly1 = int((W - tw) / 2) - 6, int((W + tw) / 2) + 6, H - 17, H - 3
    blue = (46, 86, 170, 255)
    draw.rounded_rectangle((lx0, ly0, lx1, ly1), radius=3, fill=(252, 240, 205, 255), outline=blue, width=2)
    draw.polygon([(lx0 - 4, ly0 + 3), (lx0 + 1, ly0 + 3), (lx0 + 1, ly1 - 3)], fill=blue)
    draw.polygon([(lx1 + 4, ly1 - 3), (lx1 - 1, ly1 - 3), (lx1 - 1, ly0 + 3)], fill=blue)
    draw.text(((W - tw) / 2, ly0 + 1), text, font=font, fill=(70, 45, 20, 255))
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--preview', nargs=2, metavar=('PAGE_PNG', 'OUT_PNG'),
                    help='also draw the art over a PNG of the stock page')
    args = ap.parse_args()
    img = make()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print(f'wrote {OUT.relative_to(ROOT)} ({W}x{H}, page position {X0},{Y0})')
    if args.preview:
        page = Image.open(args.preview[0]).convert('RGBA')
        page.alpha_composite(img, (X0, Y0))
        page.save(args.preview[1])


if __name__ == '__main__':
    main()
