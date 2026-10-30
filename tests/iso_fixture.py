"""Tiny synthetic disc structure, not Nintendo game data."""
import hashlib
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from disc_asset import Revision


def image(alternate=False):
    data = bytearray(0x3000)
    data[:6] = b'GPVE01'
    struct.pack_into('>I', data, 0x1c, 0xc2339f3d)
    struct.pack_into('>III', data, 0x420, 0x500, 0x800, 37)
    struct.pack_into('>I', data, 0x500, 256)
    struct.pack_into('>I', data, 0x590, 16)
    data[0x600:0x610] = b'SYNTHETIC DOL!!!'
    struct.pack_into('>III', data, 0x800, 0x01000000, 0, 2)
    struct.pack_into('>III', data, 0x80c, 1, 0x1000, 0x1960)
    data[0x818:0x825] = b'\0opening.bnr\0'
    data[0x1000:0x1004] = b'BNR1'
    offset = 0x1020
    for by in range(0, 32, 4):
        for bx in range(0, 96, 4):
            for y in range(4):
                for x in range(4):
                    red = (bx + x) * 31 // 95
                    green = (by + y) * 31 // 31
                    blue = 31 if ((bx//8 + by//8 + alternate) & 1) else 0
                    struct.pack_into('>H', data, offset, 0x8000 | red << 10 | green << 5 | blue)
                    offset += 2
    revision = Revision('SYNTHETIC TEST FIXTURE, not a game revision',
                        hashlib.sha1(data[0x500:0x610]).hexdigest(), True)
    return data, {('GPVE01', 0, 0): revision}
