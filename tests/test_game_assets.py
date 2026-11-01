import struct
import unittest
from game_archive import yaz0, rarc
from j3d_model import texture, tiled_rgba, convert


class GameAssets(unittest.TestCase):
    def test_yaz_overlap(self):
        data=b'Yaz0'+struct.pack('>I',6)+bytes(8)+bytes([0x80])+b'a'+bytes([0x30,0])
        self.assertEqual(yaz0(data),b'aaaaaa')

    def test_yaz_rejects_invalid_backreference(self):
        for stream in (b'',bytes([0,0,0]),bytes([0x80])+b'a'+bytes([0xF0,0])):
            with self.assertRaises(ValueError):yaz0(b'Yaz0'+struct.pack('>I',6)+bytes(8)+stream)

    def test_archive_and_model_reject_truncation(self):
        for size in range(32):
            with self.assertRaises(ValueError):rarc(b'RARC'+bytes(size))
            with self.assertRaises(ValueError):convert(b'J3D2bmd3'+bytes(size))

    def test_gx_rgb565_and_ia8(self):
        self.assertEqual(texture(4,4,4,b'\xf8\0'*16),bytes([255,0,0,255])*16)
        self.assertEqual(texture(3,4,4,b'\x80\x40'*16),bytes([64,64,64,128])*16)
        with self.assertRaises(ValueError):texture(4,8,8,bytes(31))

    def test_cmpr_transparency(self):
        self.assertEqual(texture(14,8,8,bytes(4)+bytes([255])*4+bytes(24))[:4],bytes(4))

    def test_morton_bytes_and_vertical_orientation(self):
        pixels=b''.join(bytes([x,y,13,255]) for y in range(8) for x in range(8))
        out=tiled_rgba(pixels,8,8)
        self.assertEqual(out[:4],bytes([255,13,0,0]))
        self.assertEqual(out[4:8],bytes([255,13,0,1]))
        self.assertEqual(out[8:12],bytes([255,13,1,0]))
