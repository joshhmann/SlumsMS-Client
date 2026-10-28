#!/usr/bin/env python3
"""Damage-number glyphs '.', 'K', 'M', 'B' for the short damage numbers (ticket 119), baked into src/Custom.wz.

  add_damage_glyphs.py --stock-basiceff <Data>/Effect/BasicEff.img [--preview out.png] [--check]

The client picks a damage digit's sprite by c - '0', so the formatter writes '.', 'K', 'M', 'B' as ':', ';', '<', '='
and the sprites must be children "10", "11", "12", "13" of each digit set (NoRed0/1, NoBlue0/1, NoViolet0/1,
NoCri0/1). Each glyph is drawn in that set's own style, measured from its stock digits:
  - the fill is the set's vertical gradient, read row by row from the centre column of its "1";
  - a light ring (the white/grey just outside the fill) and a dark ring around that;
  - the canvas is as tall as the set's "8" with origin (width/2, height-1), like the digits.
Letter shapes come from DejaVu Sans Bold (free licence), only as masks. Custom.wz gets Effect/BasicEff.img with just
these 32 canvases; leaf additions to a stock image are what resman.cpp's Custom.wz overlay allows.
"""
import argparse
import os
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from native_wz import Archive, Node, canvas, parse_img, pixels, semantic_hash, write_img  # noqa: E402
from PIL import Image, ImageDraw, ImageFilter, ImageFont  # noqa: E402

ROOT = HERE.parent.parent
CUSTOM = ROOT / 'src' / 'Custom.wz'
IMG = 'Effect/BasicEff.img'
SETS = ['NoRed0', 'NoRed1', 'NoBlue0', 'NoBlue1', 'NoViolet0', 'NoViolet1', 'NoCri0', 'NoCri1']
GLYPHS = {'10': '.', '11': 'K', '12': 'M', '13': 'B'}
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def rgba(node):
    v = node.value
    raw = pixels(node)
    f = v['format'] + v['format2']
    w, h = v['width'], v['height']
    if f == 2:
        return Image.frombytes('RGBA', (w, h), raw, 'raw', 'BGRA')
    if f == 1:
        out = bytearray()
        for i in range(0, len(raw), 2):
            lo, hi = raw[i], raw[i + 1]
            out += bytes(((hi & 15) * 17, (lo >> 4) * 17, (lo & 15) * 17, (hi >> 4) * 17))
        return Image.frombytes('RGBA', (w, h), bytes(out))
    if f == 513:
        return Image.frombytes('RGB', (w, h), raw, 'raw', 'BGR;16').convert('RGBA')
    raise ValueError(f'canvas format {f}')


def is_fill(p):
    r, g, b, a = p
    return a > 200 and max(r, g, b) - min(r, g, b) > 40   # coloured, not the white/grey/black rings


def style(digit_one, digit_eight):
    """(height, per-row fill colours, light ring colour, dark ring colour, ring widths) from the set's digits."""
    one = digit_one.load()
    w1, h1 = digit_one.size
    col = [one[w1 // 2, y] for y in range(h1)]
    fills = [p for p in col if is_fill(p)]
    first = next(i for i, p in enumerate(col) if is_fill(p))
    rows = {}
    for y in range(h1):
        rows[y] = col[y][:3] if is_fill(col[y]) else None
    # carry the nearest fill colour into rows the column doesn't fill
    last = fills[0][:3]
    for y in range(h1):
        if rows[y] is None:
            rows[y] = last
        else:
            last = rows[y]
    mid = h1 // 2
    row = [one[x, mid] for x in range(w1)]
    lead = next(i for i, p in enumerate(row) if is_fill(p))
    dark = sum(1 for p in row[:lead] if p[3] > 0 and max(p[:3]) < 0x60)
    light = lead - next(i for i, p in enumerate(row) if p[3] > 0 and max(p[:3]) >= 0x60) if any(
        p[3] > 0 and max(p[:3]) >= 0x60 for p in row[:lead]) else 2
    last_fill = max(i for i, p in enumerate(col) if is_fill(p))
    return digit_eight.size[1], rows, max(1, min(light, 3)), max(1, min(dark, 3)), (first, last_fill), h1


def draw(ch, height, rows, light, dark, span, h1):
    """span: the first and last rows the digits' fill covers (in the "1"), mapped onto this canvas height."""
    pad = light + dark
    scale = (height - 1) / max(1, h1 - 1)
    top, bottom = round(span[0] * scale), round(span[1] * scale)
    cap = bottom - top + 1
    mask = Image.new('L', (height * 2, height), 0)
    d = ImageDraw.Draw(mask)
    if ch == '.':
        r = max(2, cap // 5)
        d.ellipse((pad + 1, bottom - 2 * r, pad + 1 + 2 * r, bottom), fill=255)
    else:
        size = cap
        while size > 4:   # largest font whose cap height (minus the bolding below) fits the digits' fill
            font = ImageFont.truetype(FONT, size)
            box = d.textbbox((0, 0), ch, font=font)
            if box[3] - box[1] <= cap - 2:
                break
            size -= 1
        d.text((pad + 1 - box[0], bottom - 1 - (box[3] - box[1]) - box[1] + 1), ch, font=font, fill=255)
        mask = mask.filter(ImageFilter.MaxFilter(3))   # the digits' strokes are chunkier than the font's
    fill = mask.point(lambda v: 255 if v >= 128 else 0)
    ring1 = fill.filter(ImageFilter.MaxFilter(2 * light + 1))
    ring2 = ring1.filter(ImageFilter.MaxFilter(2 * dark + 1))
    bbox = ring2.getbbox()
    out = Image.new('RGBA', mask.size, (0, 0, 0, 0))
    po, pf, p1, p2 = out.load(), fill.load(), ring1.load(), ring2.load()
    for y in range(mask.size[1]):
        src = min(h1 - 1, max(0, round(y * (h1 - 1) / max(1, height - 1))))
        c = rows[src]
        for x in range(mask.size[0]):
            if pf[x, y]:
                po[x, y] = c + (255,)
            elif p1[x, y]:
                po[x, y] = (0xEE, 0xEE, 0xEE, 255)
            elif p2[x, y]:
                po[x, y] = (0, 0, 0, 255)
    out = out.crop((bbox[0], 0, bbox[2], height))
    return out


def build(stock_path):
    root, _ = parse_img(open(stock_path, 'rb').read(), IMG)
    image = Node('', 'property')
    previews = []
    for s in SETS:
        st = root.child(s)
        h, rows, light, dark, span, h1 = style(rgba(st.child('1')), rgba(st.child('8')))
        kids = []
        for name, ch in GLYPHS.items():
            im = draw(ch, h, rows, light, dark, span, h1)
            kids.append(canvas(name, im, [Node('origin', 'vector', (im.width // 2, h - 1))]))
            previews.append(im)
        image.children.append(Node(s, 'property', None, kids))
    return image, previews


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stock-basiceff', required=True)
    ap.add_argument('--preview')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    image, previews = build(a.stock_basiceff)
    if a.preview:
        w = sum(p.width + 4 for p in previews)
        h = max(p.height for p in previews)
        sheet = Image.new('RGBA', (w, h), (60, 60, 60, 255))
        x = 0
        for p in previews:
            sheet.alpha_composite(p, (x, h - p.height))
            x += p.width + 4
        sheet.resize((w * 3, h * 3), Image.NEAREST).save(a.preview)
    blob = write_img(image, 'gms')
    want = semantic_hash(parse_img(blob, IMG, 'gms')[0])
    with Archive(CUSTOM, 83, 'gms') as wz:
        have = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
        if have.get(IMG) == want:
            print(f'{CUSTOM.relative_to(ROOT)}: {IMG} glyphs are up to date')
            return 0
        if a.check:
            print(f'{CUSTOM.relative_to(ROOT)}: {IMG} glyphs are missing or stale')
            return 1
        wz.put_image(IMG, blob, replace=IMG in have)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)
    os.replace(tmp, CUSTOM)
    print(f'{CUSTOM.relative_to(ROOT)}: wrote {IMG} with {len(SETS) * len(GLYPHS)} glyphs')
    return 0


if __name__ == '__main__':
    sys.exit(main())
