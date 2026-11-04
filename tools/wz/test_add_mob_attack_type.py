"""needs_type: which mob attacks the v83 attack-info builder (0x0067FD81) fails on, and the type that fixes them."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Node  # noqa: E402
from add_mob_attack_type import needs_type, mob_image  # noqa: E402


def mob(attack_info, newer_types):
    info = Node('info', 'property', children=[Node('attack', 'property', children=[
        Node(str(i), 'property', children=[Node('type', 'int', t)]) for i, t in enumerate(newer_types)])])
    return Node('m.img', 'property', children=[info] + [
        Node(f'attack{i + 1}', 'property', children=[Node('info', 'property', children=c)])
        for i, c in enumerate(attack_info)])


def circle():
    return Node('range', 'property', children=[Node('r', 'int', 550), Node('sp', 'vector', (-48, -33))])


def box():
    return Node('range', 'property', children=[Node('lt', 'vector', (-1, -1)), Node('rb', 'vector', (1, 1))])


class NeedsType(unittest.TestCase):
    def test_rose_garden_8211000(self):
        # attack1/info has range r+sp and no type; info/attack/0/type = 2 (the ball attack)
        self.assertEqual(needs_type(mob([[circle()]], [2])), [('attack1', 2)])

    def test_slot_is_attack_number_minus_one(self):
        self.assertEqual(needs_type(mob([[box()], [circle()]], [0, 1])), [('attack2', 1)])

    def test_type_already_there(self):
        self.assertEqual(needs_type(mob([[circle(), Node('type', 'int', 1)]], [2])), [])

    def test_box_range_reads_type_zero_fine(self):
        self.assertEqual(needs_type(mob([[box()]], [2])), [])

    def test_types_v83_does_not_know_stay_out(self):
        self.assertEqual(needs_type(mob([[circle()]], [11])), [])
        self.assertEqual(needs_type(mob([[circle()]], [])), [])

    def test_image_holds_only_the_leaf(self):
        node = mob_image('8211000', [('attack1', 2)])
        self.assertEqual(node.name, 'Mob/8211000.img')
        leaf = node.children[0].children[0].children[0]
        self.assertEqual((node.children[0].name, leaf.name, leaf.kind, leaf.value), ('attack1', 'type', 'int', 2))


if __name__ == '__main__':
    unittest.main()
