"""Preparation failures must not invoke conversion or produce usable output."""
from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import disc_image
from build import run


def header(iso_size=12288, recorded_size=72):
    data = bytearray(72)
    data[:4] = b'RVZ\x01'
    struct.pack_into('>QQ', data, 36, iso_size, recorded_size)
    data[52:] = hashlib.sha1(data[:52]).digest()
    return data


class RvzPreparationTests(unittest.TestCase):
    def attempt(self, data, message):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'original with spaces.rvz'
            source.write_bytes(data)
            with self.assertRaisesRegex(ValueError, message):
                disc_image.prepare_rvz(source, root, {}, dolphin_tool='absent-DolphinTool')
            self.assertEqual(source.read_bytes(), data)
            self.assertFalse((root / 'prepared/disc.iso').exists())

    def test_rejects_corrupt_header(self):
        data = header()
        data[16] ^= 1
        self.attempt(data, 'checksum')

    def test_rejects_recorded_size_mismatch(self):
        self.attempt(header(recorded_size=999), 'recorded size')

    def test_rejects_oversized_expansion(self):
        self.attempt(header(iso_size=2**40), 'GameCube disc limit')

    def test_missing_converter_reports_requirement(self):
        self.attempt(header(), 'requires installed DolphinTool')

    def test_builder_reports_preparation_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = header()
            source = root / 'original.rvz'
            source.write_bytes(data)
            with patch('disc_image.shutil.which', return_value=None), redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(ValueError, 'DolphinTool'):
                    run(source, work_dir=root / 'work')
            report = json.loads((root / 'work/output/build-report.json').read_text())
            self.assertEqual(list(report['stages']), ['RVZ Preparation'])
            self.assertEqual(report['stages']['RVZ Preparation']['status'], 'FAIL')
            self.assertFalse(report['outputs'])
            self.assertEqual(source.read_bytes(), data)
