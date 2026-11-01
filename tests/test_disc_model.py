import struct
import unittest

from inspect_disc_model import inspect, REQUIRED


class DiscModelTests(unittest.TestCase):
    def fixture(self):
        blocks = b''.join(struct.pack('>4sIHH', name.encode(), 12, 1, 0)
                          for name in sorted(REQUIRED))
        return struct.pack('>8sII16x', b'J3D2bmd3', 32 + len(blocks), len(REQUIRED)) + blocks

    def test_block_inventory(self):
        report = inspect(self.fixture())
        self.assertEqual({b['tag'] for b in report['blocks']}, REQUIRED)
        self.assertIn('NOT CONVERTED OR RENDERED', report['status'])

    def test_corrupt_extents_and_missing_sections(self):
        data = self.fixture()
        for bad in (data[:30], data[:-1], data + b'X', b'BADMAGIC' + data[8:]):
            with self.subTest(size=len(bad)), self.assertRaises(ValueError):
                inspect(bad)
        bad = bytearray(data)
        struct.pack_into('>I', bad, 36, 0xffffffff)
        with self.assertRaises(ValueError):
            inspect(bad)
