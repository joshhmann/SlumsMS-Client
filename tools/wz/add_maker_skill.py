#!/usr/bin/env python3
"""Put the stock Maker skill nodes back into the client through src/Custom.wz (1.6.2, ticket 112).

MapleRoot removed the Maker skill from its client Skill data; the 1.6.2 server gives it back (ticket 112 option (a)).
The client still has the Maker names (String/Skill.img) and the Maker window (UI/UIWindow.img/Maker), so only the
skill nodes are missing. This copies them unchanged from a stock v83 Skill.wz:

  Skill/000.img/skill/0001007     Beginner
  Skill/1000.img/skill/10001007   Noblesse
  Skill/2000.img/skill/20001007   Legend (Aran)

Each holds icon / iconMouseOver / iconDisabled (32x32, origin 0,32), invisible 1, disable 1 and level 1-3 -> h1-h3,
the same as the server's node XML (162-maker, tools/data-align/maker-skill/). Stock v83 has no Evan 20011007, and this
fork's Evan isn't playable (ticket 98), so 2001.img is left out.

The three images exist in stock, so resman.cpp merges these into them; "skill/<id>" is a folder the stock image
lacks, which the Serialize hook adds whole (1.6.1-rc11 and later). The stock nodes are pinned by hash, so only an
unchanged stock Skill.wz is accepted. Every other image of Custom.wz is checked to be unchanged.

    python3 tools/wz/add_maker_skill.py --stock-skill <stock v83 Skill.wz> [--client-skill Data/Skill] [--check]
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
# image -> (skill id, semantic hash of the stock node in GMS v83 Skill.wz, sha256 c68f7f46...d0f1)
MAKER = {
    '000.img': ('0001007', '0b18d0e0b0ef49373178d417d2ddd03212541c854411409c3fcaf6c010080a32'),
    '1000.img': ('10001007', 'c644b84fc26af33152bdaa70f3723ebe2cc413e1791d267f51524c8508fe213a'),
    '2000.img': ('20001007', '2283936a6b7b5310ab664ab04e51c1027c56dc8e8f88fa1a31e0e6d03e2d3834'),
}


def stock_nodes(path):
    nodes = {}
    with Archive(path, 83, 'gms') as wz:
        for img, (sid, want) in MAKER.items():
            node = wz.image(img).get(f'skill/{sid}')
            if semantic_hash(node) != want:
                sys.exit(f'stock {img}/skill/{sid} is not the GMS v83 node ({semantic_hash(node)[:12]})')
            nodes[img] = node
    return nodes


def check_client(folder):
    """The client's own Skill images must still lack the nodes (or carry the same ones)."""
    for img, (sid, want) in MAKER.items():
        root, _ = parse_img((Path(folder) / img).read_bytes(), img)
        have = root.get('skill').child(sid)
        state = 'absent' if have is None else 'stock' if semantic_hash(have) == want else 'DIFFERENT'
        if state == 'DIFFERENT':
            sys.exit(f'client {img}/skill/{sid} differs from stock')
        print(f'client {img}/skill/{sid}: {state}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stock-skill', help='a stock GMS v83 Skill.wz (read only)')
    ap.add_argument('--client-skill', help="the client's Data/Skill folder (checks it lacks the nodes)")
    ap.add_argument('--check', action='store_true', help='only report whether Custom.wz already has the nodes')
    args = ap.parse_args()
    if args.client_skill:
        check_client(args.client_skill)

    with Archive(CUSTOM, 83, 'gms') as wz:
        before = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
        have = {img: wz.image(f'Skill/{img}') for img in MAKER if f'Skill/{img}' in before}
    present = [img for img, (sid, want) in MAKER.items()
               if img in have and have[img].child('skill') and have[img].get('skill').child(sid)
               and semantic_hash(have[img].get(f'skill/{sid}')) == want]
    if len(present) == len(MAKER):
        print(f'{CUSTOM.relative_to(ROOT)} already has the Maker skill nodes')
        return 0
    if args.check:
        print(f'{CUSTOM.relative_to(ROOT)} lacks Maker skill nodes in {sorted(set(MAKER) - set(present))}')
        return 1
    if not args.stock_skill:
        sys.exit('--stock-skill is required to add the nodes')

    nodes = stock_nodes(args.stock_skill)
    targets = {f'Skill/{img}' for img in MAKER}
    with Archive(CUSTOM, 83, 'gms') as wz:
        for img, node in nodes.items():
            path = f'Skill/{img}'
            root = Node(path, 'property', children=[Node('skill', 'property', children=[node])])
            blob = write_img(root, 'gms')
            reparsed, _ = parse_img(blob, path, 'gms')
            if semantic_hash(reparsed) != semantic_hash(root):
                sys.exit(f're-encoded {path} does not round-trip')
            wz.put_image(path, blob, replace=path in before)
        fd, tmp = tempfile.mkstemp(suffix='.wz', dir=CUSTOM.parent)
        os.close(fd)
        wz.save(tmp)
    with Archive(tmp, 83, 'gms') as wz:
        after = {p: semantic_hash(wz.image(p)) for p, _ in wz.images()}
        landed = {img: semantic_hash(wz.image(f'Skill/{img}').get(f'skill/{sid}')) for img, (sid, _) in MAKER.items()}
    changed = [p for p in before if p not in targets and after.get(p) != before[p]]
    wrong = [img for img, (_, want) in MAKER.items() if landed[img] != want]
    if changed or set(after) - set(before) - targets or wrong:
        os.unlink(tmp)
        sys.exit(f'unexpected change: {changed or wrong or sorted(set(after) - set(before))}')
    os.replace(tmp, CUSTOM)
    digest = hashlib.sha256(CUSTOM.read_bytes()).hexdigest()
    print(f'wrote {CUSTOM.relative_to(ROOT)}: Maker skill in Skill/{{{",".join(MAKER)}}}, sha256 {digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
