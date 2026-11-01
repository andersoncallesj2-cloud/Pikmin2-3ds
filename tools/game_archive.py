"""Bounded Yaz0/RARC reader. New port tooling, not original compiled JKR code.

Field references: projectPiki JKRArchive::{SArcHeader,SArcDataInfo,SDIFileEntry}.
Returns bytes in memory; no archive-controlled filename is written to disk.
"""
import struct
from disc_asset import disc_name

MAX_SIZE = 32 * 1024 * 1024


def yaz0(data):
    if data[:4] != b'Yaz0':
        return data
    if len(data) < 16:
        raise ValueError('Truncated Yaz0 header')
    size = struct.unpack_from('>I', data, 4)[0]
    if not 0 < size <= MAX_SIZE:
        raise ValueError('Yaz0 expanded size exceeds limit')
    out = bytearray()
    pos, bits, code = 16, 0, 0
    def take():
        nonlocal pos
        if pos >= len(data):
            raise ValueError('Truncated Yaz0 stream')
        value = data[pos]
        pos += 1
        return value
    while len(out) < size:
        if not bits:
            code, bits = take(), 8
        if code & 0x80:
            out.append(take())
        else:
            a, b = take(), take()
            distance = ((a & 15) << 8 | b) + 1
            length = (a >> 4) + 2 if a >> 4 else take() + 18
            if distance > len(out) or length > size - len(out):
                raise ValueError('Invalid Yaz0 backreference or expanded extent')
            for _ in range(length):
                out.append(out[-distance])
        code = (code << 1) & 255
        bits -= 1
    return bytes(out)


def rarc(compressed):
    data = yaz0(compressed)
    def read(offset, size):
        if offset < 0 or size < 0 or offset > len(data) or size > len(data)-offset:
            raise ValueError('RARC extent outside archive')
        return data[offset:offset+size]
    def u32(offset):
        return struct.unpack('>I', read(offset, 4))[0]
    if read(0, 4) != b'RARC' or len(data) > MAX_SIZE:
        raise ValueError('Expected bounded RARC archive')
    total, header, relative, length = (u32(i) for i in (4, 8, 12, 16))
    if total > len(data) or header != 0x20:
        raise ValueError('Invalid RARC header')
    base = header + relative
    read(base, length)
    nodes, node_offset, files, file_offset, strings, string_offset = (u32(i) for i in (32, 36, 40, 44, 48, 52))
    if not 1 <= nodes <= 4096 or not 1 <= files <= 65536:
        raise ValueError('RARC node/file limit exceeded')
    node_table = read(32+node_offset, nodes*16)
    file_table = read(32+file_offset, files*20)
    names = read(32+string_offset, strings)
    def name(offset):
        if offset >= len(names) or names.find(b'\0', offset) < 0:
            raise ValueError('Invalid RARC string')
        raw = names[offset:names.index(b'\0', offset)]
        return raw.decode('ascii') if raw in (b'.', b'..') else disc_name(raw)
    output, seen_paths = {}, set()
    def visit(index, prefix, parents):
        if index >= nodes or index in parents or len(parents) > 64:
            raise ValueError('Invalid/cyclic RARC directory')
        _, _, _, count, first = struct.unpack_from('>4sIHHI', node_table, index*16)
        if first + count > files:
            raise ValueError('RARC directory entry extent invalid')
        for i in range(first, first+count):
            _, _, flags_name, offset, size, _ = struct.unpack_from('>HHIIII', file_table, i*20)
            component = name(flags_name & 0xffffff)
            if component in ('.', '..'):
                continue
            path = prefix + component
            if path.casefold() in seen_paths:
                raise ValueError('Duplicate RARC path')
            seen_paths.add(path.casefold())
            flags = flags_name >> 24
            if flags & 2:
                visit(offset, path+'/', parents | {index})
            else:
                if offset > length or size > length-offset:
                    raise ValueError('RARC file outside data section')
                output[path] = yaz0(read(base+offset, size))
    visit(0, '', set())
    return output
