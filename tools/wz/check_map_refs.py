#!/usr/bin/env python3
"""Check that every map's back/obj/tile reference resolves in the client's own Data/.

The v83 client renders a map straight out of its own WZ: for each back entry it reads
Back/<bS>.img/<no>, for each obj entry Obj/<oS>.img/<l0>/<l1>/<l2>, for each tile entry
Tile/<tS>.img/<u>/<no>, and draws the canvas it finds. A missing node, a canvas with no
origin, or a link that points at nothing is a null dereference on map load, which closes
the client without a crash report. This walks every reference the way the client does and
prints one line per map it cannot load.

Two modes:
  * map images (default)  - check the map .img from the client's Data/Map/Map*, which is
                            what the client actually loads.
  * --server-maps DIR     - check a server's Map.wz XML dump instead (only the map list and
                            the map img's existence then differ; the references still come
                            from the client's own img).

Usage:
  check_map_refs.py --data Data 410000350 410000300
  check_map_refs.py --data Data --server-maps servermaps --list crash-maps.txt
"""
from __future__ import annotations

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from native_wz import FormatError, Node, parse_img, pixels  # noqa: E402

LINK_KEYS = ('_inlink', '_outlink', 'source')
DATA_ROOTS = ('Map', 'Effect', 'Character', 'Mob', 'Npc', 'Skill', 'Item', 'String', 'Sound', 'UI')


class Problem(Exception):
    pass


def canonical(node, path):
    """First child named `path`'s last part, the way the client's WZ lookup works."""
    cur = node
    for part in path.split('/'):
        if part == '':
            continue
        nxt = next((c for c in cur.children if c.name == part), None)
        if nxt is None:
            raise Problem(f'{path} missing (no {part} under {cur.name})')
        cur = nxt
    return cur


def node_at(node, path):
    cur = node
    for part in path.split('/'):
        nxt = next((c for c in cur.children if c.name == part), None)
        if nxt is None:
            return None
        cur = nxt
    return cur


def walk_path(root, parts):
    cur = root
    for part in parts:
        cur = next((c for c in cur.children if c.name == part), None)
        if cur is None:
            return None
    return cur


def resolve_uol(parts, value):
    """A UOL's value is a path relative to the node that holds it (its parent directory)."""
    stack = list(parts)
    for part in value.split('/'):
        if part in ('', '.'):
            continue
        if part == '..':
            if not stack:
                return None
            stack.pop()
        else:
            stack.append(part)
    return stack


def int_of(node, name, default=0):
    """The client reads these through a variant that converts a numeric string too."""
    n = node.child(name)
    if n is None:
        return default
    if n.kind in ('int', 'short', 'long'):
        return int(n.value)
    if n.kind == 'string' and n.value.strip().lstrip('-').isdigit():
        return int(n.value.strip())
    raise Problem(f'{name} is <{n.kind}>, not a number')


def str_of(node, name, default=''):
    n = node.child(name)
    if n is None:
        return default
    if n.kind == 'string':
        return n.value
    raise Problem(f'{name} is <{n.kind}>, not a string')


def part_of(node, name, default='0'):
    """l0/l1/l2 are path parts: the client formats them as they are, numbers or names."""
    n = node.child(name)
    if n is None:
        return default
    if n.kind == 'string':
        return n.value
    if n.kind in ('int', 'short', 'long'):
        return str(int(n.value))
    raise Problem(f'{name} is <{n.kind}>, not a path part')


class Client:
    """The client's Data/Map, parsed lazily and cached per img.

    `data` is the client's Data directory: images are addressed from Data/Map (that is what
    the game's UOLs under Map.wz look like, e.g. `Tile/enH0.img`, `Map/Map4/<id>.img`), and
    an absolute link may also name another Data root (e.g. `Effect/...`).
    """

    def __init__(self, data_dir, verbose=False, life_index=None):
        self.data = data_dir
        self.maps = os.path.join(data_dir, 'Map')
        self.cache = {}
        self.verbose = verbose
        # Npc/<id>, Mob/<id>, Reactor/<id> file names, one per line. The map images alone
        # cannot answer a life entry; this is the client Data/ directory listing. The file
        # names are zero padded, the map entries are not, so compare the numbers.
        self.life_index = life_index

    def has_life_art(self, kind, art_id):
        if self.life_index is None:
            return True
        return int(art_id) in self.life_index.get(kind, ())

    def map_path(self, map_id):
        # Map.wz keeps the maps as Map/Map/Map<d>/<9 digits>.img: 000000000.img, 410000350.img.
        return f'Map/Map{map_id // 100000000}/{map_id:09d}.img'

    def _parse(self, relative):
        relative = relative.lstrip('/')
        if relative in self.cache:
            return self.cache[relative]
        node = None
        for base in (self.maps, self.data):
            path = os.path.join(base, relative)
            if not os.path.isfile(path):
                continue
            try:
                with open(path, 'rb') as handle:
                    node, _ = parse_img(handle.read(), relative)
            except (FormatError, OSError) as exc:
                raise Problem(f'{relative} does not parse: {exc}')
            if self.verbose:
                print(f'# parsed {path}', file=sys.stderr)
            break
        self.cache[relative] = node
        return node

    def map_img(self, map_id):
        return self._parse(self.map_path(map_id))

    def set_img(self, kind, name):
        return self._parse(f'{kind}/{name}.img')

    def resolve_link(self, img_name, link):
        """A link is a UOL: a path from Data/Map (`Map/Tile/x.img/...`), or from the img."""
        link = link.replace('\\', '/')
        candidates = []
        if link.startswith('Map/'):
            candidates.append(link[4:])
        base = img_name.rsplit('/', 1)[0]
        candidates.append(f'{base}/{link}')
        candidates.append(f'{img_name}/{link}')
        candidates.append(link)
        for candidate in candidates:
            at = candidate.find('.img/')
            if at < 0:
                img_part, rest = candidate, ''
            else:
                img_part, rest = candidate[:at + 4], candidate[at + 5:]
            node = self._parse(img_part)
            if node is None:
                continue
            return node if rest == '' else node_at(node, rest)
        return None


def canvas_origin(canvas):
    origin = canvas.child('origin')
    return None if origin is None else origin


def check_canvas(client, img_name, root, canvas, where):
    """A drawn canvas needs an origin, unless it is a link the client resolves for itself."""
    problems = []
    if canvas.kind != 'canvas':
        problems.append((f'{where} is <{canvas.kind}>, not a canvas', 'note'))
        return problems
    origin = canvas_origin(canvas)
    link = None
    for key in LINK_KEYS:
        node = canvas.child(key)
        if node is not None and node.kind == 'string':
            link = (key, node.value)
            break
    if link is None:
        if origin is None:
            problems.append((f'{where} has no origin', 'note'))
        return problems
    key, target = link
    resolved = client.resolve_link(img_name, target)
    if resolved is None:
        # The hook logs the miss and leaves the canvas empty once it is guarded (rc13); before
        # that the failed GetObjectA threw out of the map load.
        problems.append((f'{where} {key}="{target}" points at nothing', 'note'))
        return problems
    if resolved.kind == 'canvas':
        if origin is None and canvas_origin(resolved) is None:
            # b103: a linked canvas with no origin at all threw E_POINTER out of the hook and
            # closed the client on map load. rc12 stopped calling GetUnknown in that case.
            problems.append((f'{where} {key}="{target}" and neither canvas has an origin '
                             '(fixed-in-rc12)', 'fixed'))
        elif origin is None:
            problems.append((f'{where} {key}="{target}" and it has no origin (fixed-in-rc12)', 'fixed'))
    return problems


def check_frames(client, img_name, root, node, where, parts, depth=0):
    """/l2 is a property of frame canvases (animated), the canvas itself, or a uol."""
    if node.kind == 'uol':
        if depth > 8:
            return [(f'{where} uol chain does not end', 'note')]
        target = resolve_uol(parts[:-1], node.value)
        resolved = None if target is None else walk_path(root, target)
        if resolved is None:
            return [(f'{where} uol "{node.value}" points at nothing', 'note')]
        return check_frames(client, img_name, root, resolved, where, target, depth + 1)
    if node.kind == 'canvas':
        return check_canvas(client, img_name, root, node, where)
    problems = []
    frames = [c for c in node.children if c.kind in ('canvas', 'uol')]
    if not frames:
        problems.append((f'{where} has no canvas ({[c.name for c in node.children]})', 'note'))
        return problems
    for frame in frames:
        problems.extend(check_frames(client, img_name, root, frame, f'{where}/{frame.name}',
                                     parts + [frame.name], depth + 1))
    return problems


def check_map(client, map_id):
    """Every reference the client's own map img makes. Returns [(reason, class)].

    class 'fatal' is a reference the client cannot survive on load: it asks for a whole image
    that is not in its Data (Back/Obj/Tile/<set>.img), and that lookup fails inside the map
    load. 'fixed' is the b103 linked-canvas shape rc12 already handles. 'note' is everything
    else - a name or an index the client's tables do not have, which it draws around - and it
    is counted, never listed as a crash.
    """
    try:
        img = client.map_img(map_id)
    except Problem as exc:
        return [(f'{exc}', 'fatal')]
    if img is None:
        return [(f'no client map img {client.map_path(map_id)}', 'fatal')]
    problems = []
    img_name = client.map_path(map_id)

    def add(reason, cls='note'):
        problems.append((reason, cls))

    backs = img.child('back')
    for back in (backs.children if backs else []):
        tag = f'back/{back.name}'
        try:
            set_name = str_of(back, 'bS')
            if not set_name:
                # A type 0 back is a colour, not an image; 100000101 and 1988 others do this.
                continue
            no = int_of(back, 'no')
            set_img = client.set_img('Back', set_name)
            if set_img is None:
                add(f'{tag} needs Back/{set_name}.img, which the client lacks', 'fatal')
                continue
            target = node_at(set_img, f'back/{no}')
            if target is None:
                add(f'{tag} Back/{set_name}.img/back/{no} missing')
                continue
            problems.extend(check_frames(client, f'Back/{set_name}.img', set_img, target,
                                         f'{tag} Back/{set_name}.img/back/{no}', ['back', str(no)]))
        except Problem as exc:
            add(f'{tag} {exc}')

    # life/<n> is spawned from Npc/<id>.img or Mob/<id>.img; type 'n' is an NPC and 'm' a mob.
    life = img.child('life')
    for entry in (life.children if life else []):
        tag = f'life/{entry.name}'
        try:
            kind = {'n': 'Npc', 'm': 'Mob'}.get(str_of(entry, 'type'))
            if kind is None:
                continue
            art = int_of(entry, 'id')
            if not client.has_life_art(kind, art):
                add(f'{tag} needs {kind}/{art}.img, which the client lacks')
        except Problem as exc:
            add(f'{tag} {exc}')

    # reactor/<n> draws Reactor/<id>.img; the entry already carries its art name in `name`.
    reactors = img.child('reactor')
    for entry in (reactors.children if reactors else []):
        tag = f'reactor/{entry.name}'
        try:
            art = int_of(entry, 'id')
            if not client.has_life_art('Reactor', art):
                add(f'{tag} needs Reactor/{art}.img, which the client lacks')
        except Problem as exc:
            add(f'{tag} {exc}')

    for layer in img.children:
        if not layer.name.isdigit():
            continue
        for section in layer.children:
            if section.name == 'obj':
                for obj in section.children:
                    tag = f'layer {layer.name} obj/{obj.name}'
                    try:
                        set_name = str_of(obj, 'oS')
                        if not set_name:
                            continue
                        l0, l1, l2 = part_of(obj, 'l0'), part_of(obj, 'l1'), part_of(obj, 'l2')
                        set_img = client.set_img('Obj', set_name)
                        if set_img is None:
                            add(f'{tag} needs Obj/{set_name}.img, which the client lacks', 'fatal')
                            continue
                        path = f'{l0}/{l1}/{l2}'
                        target = node_at(set_img, path)
                        if target is None:
                            add(f'{tag} Obj/{set_name}.img/{path} missing')
                            continue
                        problems.extend(check_frames(client, f'Obj/{set_name}.img', set_img, target,
                                                     f'{tag} Obj/{set_name}.img/{path}', path.split('/')))
                    except Problem as exc:
                        add(f'{tag} {exc}')
            elif section.name == 'tile':
                if not section.children:
                    continue
                # tS lives on the layer itself (layer 3 of 410000350: 3/info/tS).
                info = layer.child('info')
                if info is None or info.child('tS') is None:
                    add(f'layer {layer.name} has tiles but no tS', 'fatal')
                    continue
                try:
                    set_name = str_of(info, 'tS')
                except Problem as exc:
                    add(f'layer {layer.name} tile info {exc}')
                    continue
                if not set_name:
                    add(f'layer {layer.name} tile has no tS', 'fatal')
                    continue
                set_img = client.set_img('Tile', set_name)
                if set_img is None:
                    add(f'layer {layer.name} tile needs Tile/{set_name}.img, which the client lacks', 'fatal')
                    continue
                for tile in section.children:
                    if tile.name == 'info':
                        continue
                    tag = f'layer {layer.name} tile/{tile.name}'
                    try:
                        u = str_of(tile, 'u')
                        no = int_of(tile, 'no')
                    except Problem as exc:
                        add(f'{tag} {exc}')
                        continue
                    if not u:
                        add(f'{tag} has no u')
                        continue
                    target = node_at(set_img, f'{u}/{no}')
                    if target is None:
                        add(f'{tag} Tile/{set_name}.img/{u}/{no} missing')
                        continue
                    problems.extend(check_frames(client, f'Tile/{set_name}.img', set_img, target,
                                                 f'{tag} Tile/{set_name}.img/{u}/{no}', [u, str(no)]))
    return problems


def section_of(reason):
    """The map section a reason belongs to: back, obj, tile, life, reactor, map."""
    head = reason.split(' ', 1)[0]
    if head.startswith('layer'):
        parts = reason.split(' ')
        return f'layer {parts[1]} {parts[2].split("/")[0]}' if len(parts) > 2 else 'layer'
    if head == 'no':
        return 'map'
    return head.split('/')[0]


def server_map_ids(server_dir):
    ids = []
    for name in sorted(os.listdir(server_dir)):
        sub = os.path.join(server_dir, name)
        if not os.path.isdir(sub):
            continue
        for img in sorted(os.listdir(sub)):
            if img.endswith('.img.xml') and img[:-8].isdigit():
                ids.append(int(img[:-8]))
    return ids


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--data', required=True, help='client Data directory (holds Map/)')
    parser.add_argument('--server-maps', help='server Map.wz XML dump: check every map in it')
    parser.add_argument('--life-index', help='client Data listing, one Npc/<id>|Mob/<id>|Reactor/<id>'
                                             ' file name per line, to check life entries')
    parser.add_argument('--list', help='write the crash list here (mapid reason, sorted)')
    parser.add_argument('--manual', help='rows the data cannot show, one "mapid reason" per line'
                                         ' (a map the owner\'s client closed on, for instance);'
                                         ' they are listed and counted apart from the findings')
    parser.add_argument('--verbose', action='store_true')
    parser.add_argument('maps', nargs='*', type=int, help='map ids to check')
    args = parser.parse_args(argv)

    life_index = None
    if args.life_index:
        life_index = {}
        with open(args.life_index, encoding='utf-8') as handle:
            for line in handle:
                line = line.strip()
                if not line or '/' not in line:
                    continue
                kind, _, name = line.partition('/')
                if name.isdigit():
                    life_index.setdefault(kind, set()).add(int(name))
    client = Client(args.data, args.verbose, life_index)
    manual = []
    if args.manual:
        with open(args.manual, encoding='utf-8') as handle:
            for line in handle:
                line = line.split('#', 1)[0].strip()
                if not line:
                    continue
                map_id, _, reason = line.partition(' ')
                manual.append((int(map_id), reason.strip() or 'manual'))
    ids = list(args.maps)
    if args.server_maps:
        ids = sorted(set(ids) | set(server_map_ids(args.server_maps)))
    if not ids:
        parser.error('no maps to check')

    found = []
    for map_id in ids:
        for reason, cls in check_map(client, map_id):
            found.append((map_id, reason, cls))

    counts = {}
    for _map_id, reason, cls in found:
        key = (cls, section_of(reason))
        counts[key] = counts.get(key, 0) + 1

    # One row per listed map: its first fatal (or b103, marked fixed-in-rc12) reason.
    listed = {}
    for map_id, reason, cls in found:
        if cls == 'note':
            continue
        if map_id in listed:
            listed[map_id][1] += 1
        else:
            listed[map_id] = [f'{reason} [fixed-in-rc12]' if cls == 'fixed' else reason, 0]

    if args.list:
        with open(args.list, 'w', encoding='utf-8') as handle:
            handle.write(f'# {len(ids)} maps scanned, {len(listed)} listed, {len(found)} findings'
                         + (f', {len(manual)} manual rows' if manual else '') + '\n')
            handle.write('# listed = a whole set image the client lacks (fatal) or the b103 linked-canvas\n')
            handle.write('# shape rc12 fixes (marked fixed-in-rc12). Everything counted below as a note is a\n')
            handle.write('# name or an index the client tables do not have, which it draws around.\n')
            for (cls, section), n in sorted(counts.items()):
                handle.write(f'#   {n:6d} {cls:6s} {section}\n')
            if manual:
                handle.write('# manual rows: known map problems the data checks cannot see\n')
                for map_id, reason in manual:
                    handle.write(f'{map_id} {reason}\n')
            for map_id in sorted(listed):
                reason, more = listed[map_id]
                handle.write(f'{map_id} {reason}' + (f' (+{more} more)' if more else '') + '\n')
    for map_id, reason, cls in found:
        if cls != 'note':
            print(f'{map_id} {reason}')
    print(f'# {len(ids)} maps scanned, {len(found)} findings in {len(listed)} listed maps', file=sys.stderr)
    for (cls, section), n in sorted(counts.items()):
        print(f'#   {n:6d} {cls:6s} {section}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
