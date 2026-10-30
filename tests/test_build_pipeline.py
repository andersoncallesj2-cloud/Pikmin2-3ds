from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from iso_fixture import image
from build import run


class BuilderFailureTests(unittest.TestCase):
    def test_missing_sdk_stops_without_compilation_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data, revisions = image()
            iso = root / 'synthetic.iso'
            iso.write_bytes(data)
            work = root / 'run'
            with patch.dict(os.environ, {'DEVKITARM': ''}), redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(ValueError, 'devkitPro'):
                    run(iso, work_dir=work, revisions=revisions)
            report = json.loads((work / 'output/build-report.json').read_text())
            self.assertEqual(report['stages']['Asset Extraction']['status'], 'PASS')
            self.assertEqual(report['stages']['Runtime Compilation']['status'], 'FAIL')
            self.assertNotIn('CIA Packaging', report['stages'])
            self.assertFalse((work / 'output/Pikmin2-3DS.cia').exists())
            self.assertEqual(iso.read_bytes(), data)

    def test_existing_work_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            sentinel = work / 'user-file.txt'
            sentinel.write_text('keep')
            with self.assertRaisesRegex(ValueError, 'not empty'):
                run(work / 'absent.iso', work_dir=work)
            self.assertEqual(sentinel.read_text(), 'keep')

    def test_bad_dump_produces_failure_report_not_a_cia(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'bad.iso').write_bytes(b'not a game image')
            with redirect_stdout(io.StringIO()), self.assertRaises(ValueError):
                run(root / 'bad.iso', work_dir=root / 'build')
            report = json.loads((root / 'build/output/build-report.json').read_text())
            self.assertEqual(list(report['stages']), ['ISO Validation'])
            self.assertEqual(report['stages']['ISO Validation']['status'], 'FAIL')
            self.assertFalse(report['outputs'])
