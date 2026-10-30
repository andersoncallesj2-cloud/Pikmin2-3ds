"""Bounded, read-only Pikmin 2 disc validation and banner extraction.

No gameplay assets or tools are downloaded. Initially allow only the known USA
retail executable from the supplied projectPiki config/GPVE01/config.yml.
"""
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import struct
import zlib


@dataclass(frozen=True)
class Revision:
    name: str
    dol_sha1: str
    test_fixture: bool = False


REVISIONS = {
    ('GPVE01', 0, 0): Revision('USA retail, known projectPiki GPVE01 executable',
                             '90d328bf8f190c90472e8c19e7e53c6ad0fe0d1a'),
}


def word(data, offset):
    return struct.unpack_from('>I', data, offset)[0]


def decode_banner(data):
    if (len(data), data[:4]) not in ((0x1960, b'BNR1'), (0x1fa0, b'BNR2')):
        raise ValueError('Banner magic and exact file size do not match BNR1/BNR2')
    output = bytearray(96 * 32 * 3)
    offset = 32
    for by in range(0, 32, 4):
        for bx in range(0, 96, 4):
            for y in range(4):
                for x in range(4):
                    p = struct.unpack_from('>H', data, offset)[0]
                    offset += 2
                    if p & 0x8000:
                        rgba = [((p >> s) & 31) * 255 // 31 for s in (10, 5, 0)]
                    else:
                        alpha = ((p >> 12) & 7) / 7
                        rgba = [round(((p >> s) & 15) * 17 * alpha) for s in (8, 4, 0)]
                    pos = ((by + y) * 96 + bx + x) * 3
                    output[pos:pos+3] = bytes(rgba)
    return bytes(output)


def extract(path, revisions=None):
    revisions = REVISIONS if revisions is None else revisions
    with Path(path).open('rb') as disc:
        initial = os.fstat(disc.fileno())
        size = initial.st_size

        def read(offset, length):
            if offset < 0 or length < 0 or offset > size or length > size - offset:
                raise ValueError('Truncated image or out-of-range disc read')
            disc.seek(offset)
            data = disc.read(length)
            if len(data) != length:
                raise ValueError('Short disc read')
            return data

        h = read(0, 0x440)
        if word(h, 0x1c) != 0xc2339f3d:
            raise ValueError('Expected an uncompressed GameCube ISO/GCM; RVZ/CISO are not accepted by this first builder')
        disc_id = h[:6].decode('ascii', errors='replace')
        key = (disc_id, h[6], h[7])
        if key not in revisions:
            raise ValueError(f'Unsupported disc/revision: {disc_id}, disc {h[6]}, revision {h[7]}; initially only GPVE01 disc 0 revision 0')
        revision = revisions[key]
        dol_offset = word(h, 0x420)
        if dol_offset < 0x440:
            raise ValueError('Invalid executable offset')
        dol_h = read(dol_offset, 256)
        starts = struct.unpack_from('>18I', dol_h)
        lengths = struct.unpack_from('>18I', dol_h, 0x90)
        if not any(lengths) or any(a < 256 for a, b in zip(starts, lengths) if b):
            raise ValueError('Invalid DOL section table')
        dol_size = max(a + b for a, b in zip(starts, lengths) if b)
        if dol_size > 32 * 1024 * 1024:
            raise ValueError('Executable exceeds the bounded read limit')
        dol_hash = hashlib.sha1(read(dol_offset, dol_size)).hexdigest()
        if dol_hash != revision.dol_sha1:
            raise ValueError('Executable checksum mismatch: unsupported, modified or damaged disc; expected checksums were not changed')

        fst_offset, fst_size = word(h, 0x424), word(h, 0x428)
        if fst_offset < 0x440 or not 12 <= fst_size <= 8 * 1024 * 1024:
            raise ValueError('Invalid filesystem table offset/size')
        fst = read(fst_offset, fst_size)
        count = word(fst, 8)
        if fst[0] != 1 or word(fst, 4) != 0 or not 1 <= count <= len(fst) // 12:
            raise ValueError('Invalid filesystem root')
        names = fst[count * 12:]
        stack = [(count, '', 0)]
        paths = set()
        banner = None
        for i in range(1, count):
            while i >= stack[-1][0]:
                stack.pop()
            kind_name, offset, length = struct.unpack_from('>III', fst, i * 12)
            kind, name_offset = kind_name >> 24, kind_name & 0xffffff
            if name_offset >= len(names):
                raise ValueError('Filesystem name offset out of bounds')
            end = names.find(b'\0', name_offset)
            if end < 0:
                raise ValueError('Unterminated filesystem name')
            name = names[name_offset:end].decode('ascii')
            if not name or name in ('.', '..') or any(ord(c) < 32 or c in '/\\:' for c in name):
                raise ValueError('Unsafe filesystem component')
            full = stack[-1][1] + name
            if full.casefold() in paths:
                raise ValueError('Duplicate filesystem path')
            paths.add(full.casefold())
            if kind == 1:
                if offset != stack[-1][2] or not i < length <= stack[-1][0]:
                    raise ValueError('Invalid directory parent or end index')
                stack.append((length, full + '/', i))
            elif kind == 0:
                if offset < 0x440 or offset > size or length > size - offset:
                    raise ValueError('Filesystem file outside image')
                if full == 'opening.bnr':
                    if length not in (0x1960, 0x1fa0):
                        raise ValueError('Unexpected opening.bnr size')
                    banner = read(offset, length)
            else:
                raise ValueError('Invalid filesystem node type')
        if banner is None:
            raise ValueError('Root opening.bnr not found')
        final = os.fstat(disc.fileno())
        if (initial.st_size, initial.st_mtime_ns) != (final.st_size, final.st_mtime_ns):
            raise ValueError('Input changed while being read')
    # Return actual extracted bytes. The builder writes only its fixed filenames.
    return banner, {'disc_id': disc_id, 'disc_number': h[6], 'revision': h[7],
                    'revision_name': revision.name, 'dol_sha1': dol_hash,
                    'test_fixture': revision.test_fixture, 'image_bytes': size,
                    'asset_path': 'opening.bnr', 'asset_bytes': len(banner),
                    'asset_sha256': hashlib.sha256(banner).hexdigest()}


def convert(banner):
    rgb = decode_banner(banner)
    return struct.pack('<4sIIIII', b'P2BN', 1, 96, 32, len(rgb), zlib.crc32(rgb)) + rgb
