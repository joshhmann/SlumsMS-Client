"""classify: which mob attacks the v83 attack-info builder (0x0067FD81) fails on, and the type that fixes them."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_wz import Node  # noqa: E402
from add_mob_attack_type import classify, mob_image, needs_type  # noqa: E402


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


def ball_art():
    return Node('ball', 'canvas')


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
        # 3 and 4 are the attack tiles: v83 reads start/areaCount/attackCount and range lt/rb, not sp
        self.assertEqual(needs_type(mob([[circle()]], [3])), [])
        self.assertEqual(needs_type(mob([[circle()]], [4])), [])

    def test_newer_ball_types_map_to_circle_with_a_ball(self):
        # 13 (bullet) and 201 (ballStart/ballEnd) carry a ball: the later client fires it with the same art
        for newer in (9, 13, 201):
            self.assertEqual(needs_type(mob([[circle(), ball_art()]], [newer])), [('attack1', 2)], newer)

    def test_newer_plain_circle_types_map_to_type_1(self):
        for newer in (11, 14, 19, 21):
            self.assertEqual(needs_type(mob([[circle()]], [newer])), [('attack1', 1)], newer)

    def test_old_style_attack_takes_the_ball_art_as_the_type(self):
        # no info/attack/<N-1> at all: 8144002 has range r+sp and info/ball, 9500488 only r+sp
        self.assertEqual(needs_type(mob([[circle(), ball_art()]], [])), [('attack1', 2)])
        self.assertEqual(needs_type(mob([[circle()]], [])), [('attack1', 1)])

    def test_attack_without_a_range_is_reported_not_guessed(self):
        # 2400378 and the 9020000 skills: v83 needs range/sp and the pack has no range at all
        self.assertEqual(classify(mob([[]], [11])), ([], [('attack1', 'no range/sp: attack has no range node')]))

    def test_range_without_sp_is_reported(self):
        only_r = Node('range', 'property', children=[Node('r', 'int', 100)])
        self.assertEqual(classify(mob([[only_r]], [11]))[1],
                         [('attack1', 'no range/sp: range has only r')])

    def test_unmapped_newer_type_is_reported(self):
        self.assertEqual(classify(mob([[circle()]], [4]))[1],
                         [('attack1', 'newer type 4 is not in TYPE_MAP')])

    def test_image_holds_only_the_leaf(self):
        node = mob_image('8211000', [('attack1', 2)])
        self.assertEqual(node.name, 'Mob/8211000.img')
        leaf = node.children[0].children[0].children[0]
        self.assertEqual((node.children[0].name, leaf.name, leaf.kind, leaf.value), ('attack1', 'type', 'int', 2))


if __name__ == '__main__':
    unittest.main()
