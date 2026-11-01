#!/usr/bin/env python3
"""Build HW03: an asset-free static GPU control, not the ISO prototype."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

from build import ROOT, sdk_environment, sha, verify_cia


def run(work_dir=None, makerom=None, start_test=1):
    if start_test not in range(1, 6):
        raise ValueError('HW03 start test must be 1..5')
    if work_dir is None:
        (ROOT / 'build').mkdir(exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix='hw03-', dir=ROOT / 'build'))
    else:
        work = Path(work_dir).resolve()
        work.mkdir(parents=True, exist_ok=False)
    (work / '.gitignore').write_text('*\n')
    if any(c.isspace() for c in str(work)):
        raise ValueError('The devkitPro Makefile requires a workspace path without whitespace')
    output = work / 'output'
    output.mkdir()
    report = {'build': 'HW03 independent static GPU control', 'contains_game_assets': False,
              'hardware': 'UNVERIFIED', 'emulator': 'not tested', 'start_test': start_test,
              'built_at_utc': datetime.now(timezone.utc).isoformat(), 'outputs': {}}
    try:
        env, compiler = sdk_environment(work)
        report['compiler'] = subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0]
        app = work / 'runtime/hwprobe'
        shutil.copytree(ROOT / 'port3ds/hwprobe', app,
                        ignore=shutil.ignore_patterns('build', '*.3dsx', '*.elf', '*.cia', '*.smdh', '*.map'))
        shutil.copytree(ROOT / 'port3ds/assets', work / 'runtime/assets')
        (app / 'build').mkdir()
        report['source_hashes'] = {str(f.relative_to(app)): sha(f.read_bytes())
                                  for f in app.rglob('*') if f.is_file()}
        with (work / 'compile.log').open('wb') as log:
            subprocess.run(['make', '-j2', f'START_TEST={start_test}'], cwd=app, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        elf = (app / 'pikmin2-hw03.elf').read_bytes()
        if elf[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', elf, 18)[0] != 40:
            raise ValueError('Expected a little-endian ARM ELF')
        x3ds = app / 'pikmin2-hw03.3dsx'
        if x3ds.read_bytes()[:4] != b'3DSX':
            raise ValueError('Expected 3DSX output')
        shutil.copy2(x3ds, output / 'Pikmin2-HW03.3dsx')
        packager = shutil.which(makerom or 'makerom')
        if not packager:
            raise ValueError('makerom missing; pass --makerom /path/to/makerom')
        packager = str(Path(packager).resolve())
        with (work / 'package.log').open('wb') as log:
            subprocess.run([packager, '-f', 'cia', '-o', str(output / 'Pikmin2-HW03.cia'),
                            '-rsf', 'probe.rsf', '-target', 't', '-elf', 'pikmin2-hw03.elf',
                            '-icon', 'pikmin2-hw03.smdh', '-banner', '../assets/menu.bnr', '-desc', 'app:4'],
                           cwd=app, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        report['cia_checks'] = verify_cia(output / 'Pikmin2-HW03.cia')
        for f in output.iterdir():
            report['outputs'][f.name] = {'path': str(f), 'sha256': sha(f.read_bytes()), 'bytes': f.stat().st_size}
        report['result'] = 'ARM compilation and CIA/3DSX packaging passed; physical hardware test required'
    except Exception as exc:
        report['result'] = 'FAILED'
        report['error'] = str(exc)
        raise
    finally:
        (output / 'build-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work-dir', type=Path)
    p.add_argument('--makerom')
    p.add_argument('--start-test', type=int, choices=range(1, 6), default=1)
    args = p.parse_args()
    try:
        run(args.work_dir, args.makerom, args.start_test)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        p.exit(1, f'HW03 build failed: {exc}\n')
