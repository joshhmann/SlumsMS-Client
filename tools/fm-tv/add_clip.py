#!/usr/bin/env python3
"""Add a video clip to the Free Market's TV (the MapleTV in the FM entrance), as one more channel.

The FM's TV is the client's MapleTV NPC. Each time it is created (every entry to the map) the client plays one clip
from UI/MapleTV.img/TVmedia, picked as rand() % the number of clips (CNpc::Init 0x006D0F0A; the count is read when
MapleTV loads, CMapleTVMan +0x3C0). So adding a clip there is all it takes: the TV changes on every entry, client
data only, no server work. The MapleRoot client ships 3 copies of one Ricardo dance clip there.

Frames: ffmpeg samples the mp4 at --fps and fits it in the screen's 240x180 (--fit pad: black bars, or crop), and each frame is stored as
RGB565 (format 513, 2 bytes a pixel, zlib) with origin (0, 180) and the delay, like the stock clips. Runs on Linux.

    python3 tools/fm-tv/add_clip.py <UI/MapleTV.img> clip.mp4 [--fps 12] [--fit pad|crop] [--replace K]
    python3 tools/fm-tv/add_clip.py <UI/MapleTV.img> --drop K

Without --replace the clip is appended (TVmedia/<count>); --replace K overwrites clip K; --drop K removes clip K and
renumbers the ones after it (the client loads TVmedia/0 .. count-1). Prints the result and the file size; stage the
changed Data/UI/MapleTV.img.
"""
import argparse
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'wz'))
from native_wz import Node, parse_img, write_img  # noqa: E402

W, H = 240, 180   # the MapleTV screen; the stock clips are this size with origin (0, 180)


def frames(mp4, fps, work, fit):
    if fit == 'crop':   # fill the screen, cutting the edges
        vf = f'fps={fps},scale={W}:{H}:force_original_aspect_ratio=increase:flags=lanczos,crop={W}:{H}'
    else:               # the whole picture, black bars where the shape differs (the stock clip is on black too)
        vf = (f'fps={fps},scale={W}:{H}:force_original_aspect_ratio=decrease:flags=lanczos,'
              f'pad={W}:{H}:(ow-iw)/2:(oh-ih)/2:black')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(mp4), '-vf', vf, str(work / 'f%04d.png')], check=True)
    return [Image.open(p).convert('RGB') for p in sorted(work.glob('f*.png'))]


def canvas565(name, im, delay):
    a = np.asarray(im, dtype=np.uint16)
    v = ((a[:, :, 0] >> 3) << 11) | ((a[:, :, 1] >> 2) << 5) | (a[:, :, 2] >> 3)
    children = [Node('origin', 'vector', (0, H)), Node('z', 'int', 0), Node('delay', 'int', delay)]
    return Node(name, 'canvas', dict(width=im.width, height=im.height, format=513, format2=0, reserved=bytes(4),
        marker=0, zlib=zlib.compress(v.astype('<u2').tobytes(), 9)), children)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('mapletv', type=Path, help='the client Data/UI/MapleTV.img to update in place')
    ap.add_argument('clip', type=Path, nargs='?')
    ap.add_argument('--fps', type=int, default=12)
    ap.add_argument('--replace', type=int, default=None, help='overwrite TVmedia/K instead of appending')
    ap.add_argument('--fit', choices=('pad', 'crop'), default='pad', help='pad: whole picture (default); crop: fill')
    ap.add_argument('--drop', type=int, default=None, help='remove TVmedia/K and renumber the clips after it')
    a = ap.parse_args()

    root, key = parse_img(a.mapletv.read_bytes(), a.mapletv.name)
    media = root.child('TVmedia')
    if media is None:
        sys.exit('no TVmedia in ' + str(a.mapletv))
    if a.drop is not None:
        old = media.child(str(a.drop))
        if old is None:
            sys.exit(f'no TVmedia/{a.drop}')
        media.children.remove(old)
        for c in media.children:
            if int(c.name) > a.drop:
                c.name = str(int(c.name) - 1)
        media.children.sort(key=lambda c: int(c.name))
        a.mapletv.write_bytes(write_img(root, key))
        print(f'dropped TVmedia/{a.drop}; {len(media.children)} clips now')
        return
    if a.clip is None:
        sys.exit('a clip is needed unless --drop')
    with tempfile.TemporaryDirectory() as tmp:
        imgs = frames(a.clip, a.fps, Path(tmp), a.fit)
    if not imgs:
        sys.exit('no frames')
    delay = round(1000 / a.fps)
    k = len(media.children) if a.replace is None else a.replace
    clip = Node(str(k), 'property', children=[canvas565(str(i), im, delay) for i, im in enumerate(imgs)])
    old = media.child(str(k))
    if old is None:
        if a.replace is not None:
            sys.exit(f'no TVmedia/{k} to replace')
        media.children.append(clip)
    else:
        media.children[media.children.index(old)] = clip
    before = a.mapletv.stat().st_size
    a.mapletv.write_bytes(write_img(root, key))
    print(f'TVmedia/{k}: {len(imgs)} frames {W}x{H} at {a.fps} fps ({delay} ms); {len(media.children)} clips now')
    print(f'{a.mapletv}: {before / 1e6:.2f} MB -> {a.mapletv.stat().st_size / 1e6:.2f} MB')


if __name__ == '__main__':
    main()
