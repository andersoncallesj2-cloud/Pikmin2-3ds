#!/usr/bin/env python3
"""Reproduce selected verbatim function extracts from the supplied decompilation."""
import argparse
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
SELECTION = {
    'src/Dolphin/mtx/mtx.c': ['C_MTXLookAt'],
    'src/Dolphin/mtx/mtx44.c': ['C_MTXPerspective', 'C_MTXOrtho'],
    'src/sysCommonU/sysMath.cpp': ['qdist2', 'roundAng', 'angDist'],
}


def extract(text, name):
    match = re.search(r'^(?:void|f32) ' + re.escape(name) + r'\([^;]*?\)\n\{', text, re.M)
    if not match:
        raise RuntimeError(f'Missing expected original function: {name}')
    opening = text.index('{', match.start())
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=HERE.parents[1] / 'decomp/pikmin2-main')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    chunks = ['/* Generated from the supplied Pikmin 2 source; see provenance.json. */', '#include "math_compat.h"']
    provenance = {}
    for relative, names in SELECTION.items():
        blob = (args.source / relative).read_bytes()
        functions = {}
        for name in names:
            code = extract(blob.decode(), name)
            chunks.append(f'/* Original: {relative} :: {name} */\n' + code)
            functions[name] = hashlib.sha256(code.encode()).hexdigest()
        provenance[relative] = {'original_file_sha256': hashlib.sha256(blob).hexdigest(),
                                'extracted_functions_sha256': functions}
    outputs = {'original_math.c': '\n\n'.join(chunks) + '\n',
               'provenance.json': json.dumps(provenance, indent=2) + '\n'}
    for name, content in outputs.items():
        if args.check:
            if (HERE / name).read_text() != content:
                raise SystemExit(f'{name} differs from expected original extracts')
        else:
            (HERE / name).write_text(content)
    print('Verified original math extracts' if args.check else 'Generated original math extracts')


if __name__ == '__main__':
    main()
