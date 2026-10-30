import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from iso_fixture import image
import disc_asset


class DiscAssetTests(unittest.TestCase):
    def extract(self, data, revisions):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'disc with spaces.iso'
            p.write_bytes(data)
            before = hashlib.sha256(p.read_bytes()).digest()
            result = disc_asset.extract(p, revisions)
            self.assertEqual(before, hashlib.sha256(p.read_bytes()).digest())
            return result

    def test_extraction_and_conversion(self):
        data, revisions = image()
        banner, identity = self.extract(data, revisions)
        self.assertEqual(banner, data[0x1000:0x2960])
        self.assertTrue(identity['test_fixture'])
        converted = disc_asset.convert(banner)
        self.assertEqual(struct.unpack_from('<4sIIII', converted), (b'P2BN', 1, 96, 32, 9216))
        self.assertEqual(struct.unpack_from('<I', converted, 20)[0], zlib.crc32(converted[24:]))
        self.assertEqual(converted[24:27], bytes([0, 0, 0]))
        self.assertEqual(converted[-3:], bytes([255, 255, 0]))

    def test_real_revision_table_rejects_synthetic_executable(self):
        data, _ = image()
        with self.assertRaisesRegex(ValueError, 'checksum'):
            self.extract(data, None)

    def test_corruption(self):
        cases = []
        data, revisions = image()
        for offset, fmt, value in [(0x1c, '>I', 0), (0x428, '>I', 0xffffffff),
                                   (0x808, '>I', 0xffffffff), (0x80c, '>I', 0xffffff),
                                   (0x810, '>I', 0xffffff), (0x590, '>I', 0xffffffff)]:
            bad = bytearray(data)
            struct.pack_into(fmt, bad, offset, value)
            cases.append(bad)
        cases.append(data[:0x1100])
        for bad in cases:
            with self.subTest(length=len(bad)), self.assertRaises(ValueError):
                self.extract(bad, revisions)

    def test_unsupported_region_and_revision(self):
        data, revisions = image()
        for offset in (3, 6, 7):
            bad = bytearray(data)
            bad[offset] ^= 1
            with self.assertRaisesRegex(ValueError, 'Unsupported'):
                self.extract(bad, revisions)

    def test_bad_banner_and_changed_content(self):
        data, revisions = image()
        banner, _ = self.extract(data, revisions)
        bad = b'BAD!' + banner[4:]
        with self.assertRaises(ValueError):
            disc_asset.convert(bad)
        second, revisions = image(True)
        other, _ = self.extract(second, revisions)
        self.assertNotEqual(disc_asset.convert(banner), disc_asset.convert(other))
