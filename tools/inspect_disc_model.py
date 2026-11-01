#!/usr/bin/env python3
"""Extract and inspect a small original BMD; does not convert or render it.

Layout reference: projectPiki include/JSystem/J3D/J3DFileBlock.h.
All game data remains in a newly created, Git-ignored private work directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import disc_asset

RESOURCE = 'user/Ebisawa/testdata/title_red_piki.bmd'
REQUIRED = {'INF1', 'VTX1', 'EVP1', 'DRW1', 'JNT1', 'SHP1', 'MAT3', 'TEX1'}


def inspect(data):
    if len(data) < 32 or data[:8] != b'J3D2bmd3':
        raise ValueError('Expected a J3D2 bmd3 resource')
    size, count = struct.unpack_from('>II', data, 8)
    if size != len(data) or not 1 <= count <= 64:
        raise ValueError('Invalid BMD size or block count')
    offset = 32
    blocks = []
    tags = set()
    for _ in range(count):
        if offset + 8 > size:
            raise ValueError('Truncated BMD block header')
        tag, length = struct.unpack_from('>4sI', data, offset)
        name = tag.decode('ascii')
        if length < 8 or length > size - offset or name in tags:
            raise ValueError('Invalid/duplicate BMD block')
        block = {'tag': name, 'offset': offset, 'bytes': length}
        if name in {'EVP1', 'DRW1', 'JNT1', 'SHP1', 'MAT3', 'TEX1'}:
            if length < 12:
                raise ValueError('Truncated BMD count field')
            block['count'] = struct.unpack_from('>H', data, offset + 8)[0]
        tags.add(name)
        blocks.append(block)
        offset += length
    if offset != size or not REQUIRED.issubset(tags):
        raise ValueError('BMD extent mismatch or required block missing')
    return {'format': 'J3D2bmd3', 'bytes': size,
            'sha256': hashlib.sha256(data).hexdigest(), 'blocks': blocks,
            'status': 'EXTRACTED AND STRUCTURALLY INSPECTED; NOT CONVERTED OR RENDERED',
            'dependencies': ['VTX1/SHP1 geometry and GX display lists',
                             'MAT3 materials and TEX1 texture data',
                             'JNT1/DRW1 joint transforms; EVP1 skinning if count > 0'],
            'not_validated': ['vertex/display-list indices', 'material/texture offsets and formats',
                              'joint matrices', 'animation', '3DS conversion or rendering']}


def run(iso, work_dir):
    resources = {RESOURCE: None}
    _, identity = disc_asset.extract(iso, resources=resources)
    data = resources[RESOURCE]
    if data is None:
        raise ValueError(f'Supported executable but model resource missing: {RESOURCE}')
    report = {'disc': identity, 'resource': RESOURCE, 'model': inspect(data)}
    work = Path(work_dir).resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work / '.gitignore').write_text('*\n', encoding='ascii')
    # Fixed names only: never join a disc-supplied path to the output directory.
    (work / 'title-red-piki.bmd').write_bytes(data)
    (work / 'model-inspection.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    try:
        run(args.iso, args.work_dir)
    except (OSError, ValueError, struct.error) as exc:
        parser.exit(1, f'Model inspection failed: {exc}\n')
