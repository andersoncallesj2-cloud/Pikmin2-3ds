#!/usr/bin/env python3
"""Reproduce the original retail Section engine lifecycle without editing it."""
import argparse
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
NAMES = ['init', 'loading', 'fadeIn', 'main', 'fadeOut', 'run',
         'beginFrame', 'endFrame', 'beginRender', 'endRender', 'update', 'draw']


def extract(text, name):
    match = re.search(r'^(?:void|bool) Section::' + re.escape(name) + r'\([^;]*?\)\n\{', text, re.M)
    if not match:
        raise RuntimeError(f'Missing original Section::{name}')
    opening = text.index('{', match.start())
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=HERE.parents[1] / 'decomp/pikmin2-main')
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    relative = 'src/sysGCU/section.cpp'
    blob = (args.source / relative).read_bytes()
    chunks = ['/* Verbatim uploaded Section method bodies; see provenance.json. */',
              '#include "engine_compat.h"', 'namespace P2Engine {']
    function_hashes = {}
    for name in NAMES:
        code = extract(blob.decode(), name)
        chunks.append(code)
        function_hashes['Section::' + name] = hashlib.sha256(code.encode()).hexdigest()
    chunks.append('} // namespace P2Engine')
    provenance = {relative: {'original_file_sha256': hashlib.sha256(blob).hexdigest(),
                            'extracted_functions_sha256': function_hashes}}
    outputs = {'original_section.cpp': '\n\n'.join(chunks) + '\n',
               'provenance.json': json.dumps(provenance, indent=2) + '\n'}
    for name, contents in outputs.items():
        if args.check:
            if (HERE / name).read_text() != contents:
                raise SystemExit(f'{name} differs from original source extracts')
        else:
            (HERE / name).write_text(contents)
    print('Verified original Section extracts' if args.check else 'Generated original Section extracts')


if __name__ == '__main__':
    main()
