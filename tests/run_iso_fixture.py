#!/usr/bin/env python3
"""Real compiler/packager integration test using clearly labeled synthetic data.

Production CLI has no checksum-skip or synthetic input flag. This test injects
the fixture's revision table through the Python API, not a game support claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from iso_fixture import image
from build import run

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--work-dir', type=Path, required=True)
p.add_argument('--makerom', required=True)
args = p.parse_args()
args.work_dir.mkdir(parents=True, exist_ok=False)
results = []
for i in (0, 1):
    data, revisions = image(bool(i))
    source = args.work_dir / f'fixture-{i}.iso'
    source.write_bytes(data)
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    report, work = run(source, 'both', args.work_dir / f'run-{i}', args.makerom, revisions)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    assert report['disc']['test_fixture']
    assert all(s['status'] == 'PASS' for s in report['stages'].values())
    results.append(report)
assert results[0]['disc']['dol_sha1'] == results[1]['disc']['dol_sha1']
assert results[0]['disc']['asset_sha256'] != results[1]['disc']['asset_sha256']
assert results[0]['outputs']['cia']['ncch_sha256'] != results[1]['outputs']['cia']['ncch_sha256']
assert results[0]['outputs']['3dsx']['sha256'] != results[1]['outputs']['3dsx']['sha256']
(args.work_dir / 'integration-result.json').write_text(json.dumps({
    'test_data': 'synthetic only, not Nintendo assets', 'two_real_compilations': True,
    'two_real_cia_packages': True, 'different_input_assets_change_outputs': True,
    'real_pikmin_dump_tested': False, 'hardware_tested': False,
    'runs': [str(args.work_dir / f'run-{i}/output/build-report.json') for i in (0,1)]
}, indent=2) + '\n')
print('PASS: two real compilations/packages; changing the fixture asset changes both outputs; real dump/hardware still untested.')
