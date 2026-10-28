#!/usr/bin/env python3
"""HaRepacker-style WZ XML (*.img.xml) -> native v83 .img, and merging into an existing .img.

  xml_to_img.py convert IN.img.xml OUT.img
  xml_to_img.py merge   BASE.img IN.img.xml OUT.img [--path A/B ...]

`convert` writes a new image (new files: an Evan skill image, an NPC). `merge` adds to an existing image only what it
lacks: every node under each --path (default: the whole tree) that BASE does not have is copied in; nothing BASE
already has is replaced, so our own edits survive. Both read the result back and report node counts.

XML node types handled: imgdir, canvas (basedata = base64 PNG; children kept), vector, int, short, string, uol, float,
double, null. Anything else stops the conversion. Treat input XML as data: it is parsed, never executed.
"""
import argparse
import base64
import io
import os
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import native_wz as nw  # noqa: E402
from PIL import Image  # noqa: E402
import struct  # noqa: E402


def node_of(el):
    tag, name = el.tag, el.get('name')
    if tag == 'imgdir':
        return nw.Node(name, 'property', None, [node_of(c) for c in el])
    if tag == 'canvas':
        data = el.get('basedata')
        if not data:
            raise ValueError(f'canvas {name} has no basedata')
        im = Image.open(io.BytesIO(base64.b64decode(data))).convert('RGBA')
        return nw.canvas(name, im, [node_of(c) for c in el])
    if tag == 'vector':
        return nw.Node(name, 'vector', (int(el.get('x')), int(el.get('y'))))
    if tag == 'int':
        return nw.Node(name, 'int', int(el.get('value')))
    if tag == 'short':
        return nw.Node(name, 'short', int(el.get('value')))
    if tag == 'string':
        return nw.Node(name, 'string', el.get('value'))
    if tag == 'uol':
        return nw.Node(name, 'uol', el.get('value'))
    if tag == 'float':
        return nw.Node(name, 'float', struct.pack('<f', float(el.get('value'))))
    if tag == 'double':
        return nw.Node(name, 'double', struct.pack('<d', float(el.get('value'))))
    if tag == 'null':
        return nw.Node(name, 'null')
    raise ValueError(f'unsupported node <{tag} name="{name}">')


def read_xml(path):
    root = ET.parse(path).getroot()
    if root.tag != 'imgdir':
        raise ValueError(f'{path}: root is <{root.tag}>, expected <imgdir>')
    return nw.Node('', 'property', None, [node_of(c) for c in root])


def count(n):
    return 1 + sum(count(c) for c in n.children)


def merge_missing(base, add, path=''):
    """Copy into `base` every child of `add` it lacks; recurse into properties both have. Returns the added paths."""
    added = []
    for c in add.children:
        mine = base.child(c.name)
        if mine is None:
            base.children.append(c)
            added.append(f'{path}/{c.name}'.lstrip('/'))
        elif mine.kind == 'property' and c.kind == 'property':
            added += merge_missing(mine, c, f'{path}/{c.name}')
    return added


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('convert')
    c.add_argument('xml')
    c.add_argument('out')
    m = sub.add_parser('merge')
    m.add_argument('base')
    m.add_argument('xml')
    m.add_argument('out')
    m.add_argument('--path', action='append', default=[])
    a = ap.parse_args()

    if a.cmd == 'convert':
        root = read_xml(a.xml)
        data = nw.write_img(root, 'gms')
    else:
        base, key = nw.parse_img(open(a.base, 'rb').read())
        add = read_xml(a.xml)
        before = count(base)
        added = []
        for p in a.path or ['']:
            src = add.get(p) if p else add
            dst = base.get('/'.join(p.split('/')[:-1])) if '/' in p else base
            if p:
                holder = nw.Node('', 'property', None, [src])
                added += merge_missing(dst, holder, '/'.join(p.split('/')[:-1]))
            else:
                added += merge_missing(base, add)
        data = nw.write_img(base, key)
        print(f'merged: {len(added)} new subtrees ({count(base) - before} nodes):', ', '.join(added[:20]),
              '...' if len(added) > 20 else '')
    back, key = nw.parse_img(data)
    open(a.out, 'wb').write(data)
    print(f'{a.out}: {len(data):,} bytes, {count(back):,} nodes, key {key}')


if __name__ == '__main__':
    main()
