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

    def test_pal_bnr2_and_non_ascii_filenames(self):
        data, revisions = image()
        data[:6] = b'GPVP01'
        revisions = {('GPVP01', 0, 0): next(iter(revisions.values()))}
        # Add a Japanese named resource, as found in actual retail FSTs.
        japanese = '\u30b3\u30d4\u30fc.bmd'.encode('cp932')
        names = b'\0opening.bnr\0' + japanese + b'\0'
        struct.pack_into('>I', data, 0x428, 36 + len(names))
        struct.pack_into('>I', data, 0x808, 3)
        struct.pack_into('>III', data, 0x818, 13, 0x900, 4)
        data[0x824:0x824+len(names)] = names
        data[0x900:0x904] = b'TEST'
        data[0x1000:0x1004] = b'BNR2'
        struct.pack_into('>I', data, 0x814, 0x1fa0)
        resources = {'\u30b3\u30d4\u30fc.bmd': None}
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'pal.iso'
            p.write_bytes(data)
            banner, info = disc_asset.extract(p, revisions, resources=resources)
        self.assertEqual(len(banner), 0x1fa0)
        self.assertEqual(len(disc_asset.convert(banner)), 9240)
        self.assertEqual(info['disc_id'], 'GPVP01')
        self.assertEqual(info['filesystem_entries'], 3)
        self.assertEqual(resources['\u30b3\u30d4\u30fc.bmd'], b'TEST')
        # Production remains strict: PAL header alone must not admit a fixture.
        with self.assertRaisesRegex(ValueError, 'checksum'):
            self.extract(data, None)

    def test_disc_filename_safety(self):
        for name in (b'..', b'evil/path', b'evil\\path', b'x:y', b'\x01bad'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'Unsafe'):
                disc_asset.disc_name(name)
