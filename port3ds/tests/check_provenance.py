#!/usr/bin/env python3
"""Check compiled extracts against the exact supplied original files."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root.parent / 'decomp/pikmin2-main')
args = parser.parse_args()
for name in ('game_math', 'engine'):
    subprocess.run([sys.executable, str(root / name / 'extract_upstream.py'), '--check',
                    '--source', str(args.source)], check=True)
graphics = json.loads((root / 'graphics/provenance.json').read_text())
original = args.source / graphics['source']
assert hashlib.sha256(original.read_bytes()).hexdigest() == graphics['source_sha256']
text = (root / 'graphics/original_graphics.cpp').read_text()
bodies = text.split('// BEGIN ORIGINAL METHOD BODIES\n', 1)[1].split('\n// END ORIGINAL METHOD BODIES', 1)[0]
assert hashlib.sha256(bodies.encode()).hexdigest() == graphics['method_bodies_sha256']
original_text = original.read_text()
for method in ('void Graphics::drawLine', 'void Graphics::drawSphere'):
    signature = bodies[bodies.index(method):].split('\n', 1)[0]
    start = original_text.index(signature)
    opening = original_text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (original_text[end] == '{') - (original_text[end] == '}')
        end += 1
    assert original_text[start:end] in bodies
assert (root / 'audio/original/THPAudio.c').read_bytes() == (args.source / 'src/Dolphin/thp/THPAudio.c').read_bytes()
print('Graphics method bodies and preserved THP decoder match the uploaded source.')
