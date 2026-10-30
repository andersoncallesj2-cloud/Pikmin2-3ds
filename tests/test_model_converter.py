import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('convert_obj', ROOT / 'tools/convert_obj.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

OBJ = 'v -1 0 0\nv 1 0 0\nv 0 2 0\nf 1 2 3\n'


class ModelTests(unittest.TestCase):
    def test_convert_and_native_loader(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / 'model.obj').write_text(OBJ)
            out = d / 'model.p2mesh'
            self.assertEqual(module.convert(d / 'model.obj', out), 1)
            b = out.read_bytes()
            magic, version, count, stride, size, checksum = struct.unpack_from('<4sIIIII', b)
            self.assertEqual((magic, version, count, stride, size), (b'P2M1', 1, 3, 16, 48))
            self.assertEqual(checksum, zlib.crc32(b[24:]))
            self.assertAlmostEqual(struct.unpack_from('<f', b, 24 + 2 * 16 + 4)[0], 2.4, places=5)
            exe = d / 'loader-test'
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(ROOT / 'port3ds/include'),
                            '-I' + str(ROOT / 'port3ds/modeldiag/source'),
                            str(ROOT / 'tests/model_loader_test.c'),
                            str(ROOT / 'port3ds/modeldiag/source/model.c'), '-lm', '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(out)], check=True)

    def test_material_and_negative_indices(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / 'colors.mtl').write_text('newmtl red\nKd 1 0 0\n')
            (d / 'model.obj').write_text('mtllib colors.mtl\nusemtl red\n' + OBJ.replace('f 1 2 3', 'f -3 -2 -1'))
            module.convert(d / 'model.obj', d / 'out')
            self.assertEqual((d / 'out').read_bytes()[36:40], b'\xff\x00\x00\xff')

    def test_optional_flat_light(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / 'model.obj').write_text(OBJ)
            module.convert(d / 'model.obj', d / 'out', flat_light=True)
            self.assertEqual((d / 'out').read_bytes()[36:40], bytes([135, 135, 135, 255]))

    def test_bad_inputs_do_not_create_output(self):
        for contents in [OBJ.replace('f 1 2 3', 'f 0 2 3'), OBJ.replace('v -1', 'v nan'),
                         OBJ.replace('f 1 2 3', 'f 999 2 3'), 'mtllib ../secret.mtl\n' + OBJ,
                         OBJ.replace(' 2 0\n', ' 0 0\n')]:
            with self.subTest(contents=contents), tempfile.TemporaryDirectory() as tmp:
                d = Path(tmp)
                (d / 'model.obj').write_text(contents)
                with self.assertRaises(ValueError):
                    module.convert(d / 'model.obj', d / 'out')
                self.assertFalse((d / 'out').exists())

    def test_triangle_budget_and_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / 'model.obj').write_text(OBJ + 'f 1 2 3\n' * 6000)
            with self.assertRaisesRegex(ValueError, '6000'):
                module.convert(d / 'model.obj', d / 'out')
            (d / 'model.obj').write_text(OBJ)
            (d / 'out').write_bytes(b'user data')
            with self.assertRaisesRegex(ValueError, 'exists'):
                module.convert(d / 'model.obj', d / 'out')
            self.assertEqual((d / 'out').read_bytes(), b'user data')
