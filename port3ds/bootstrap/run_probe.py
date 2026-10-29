#!/usr/bin/env python3
"""Compile five original Pikmin 2 bootstrap units and report remaining linkage."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
from prepare_overlay import prepare

UNITS = [
    'src/sysBootupU/sysBootup.cpp', 'src/sysGCU/appThread.cpp',
    'src/sysGCU/gameflow.cpp', 'src/sysGCU/section.cpp', 'src/sysGCU/system.cpp',
]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--compiler', default='arm-none-eabi-g++')
    args = ap.parse_args()
    source, out = args.source.resolve(), args.output.resolve()
    cc = shutil.which(args.compiler)
    if not cc:
        ap.error('ARM C++ compiler not found: ' + args.compiler)
    cc = Path(cc).resolve()
    before = {name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in UNITS}
    overlay = prepare(source, out)
    flags = ['-c', '-std=gnu++17', '-fpermissive', '-fno-exceptions', '-fno-rtti',
             '-fno-short-enums', '-ffunction-sections', '-fdata-sections',
             '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard', '-mtp=soft',
             '-DVERSION_GPVE01', '-include', str(out / 'intrinsics.h'),
             '-I' + str(overlay), '-I' + str(overlay / 'stl')]
    result = []
    objects = []
    for unit in UNITS:
        obj = out / (Path(unit).stem + '.o')
        # Never mistake an object left by an earlier successful run for this run.
        obj.unlink(missing_ok=True)
        command = [str(cc), *flags, str(source / unit), '-o', str(obj)]
        run = subprocess.run(command, capture_output=True, text=True, errors='replace')
        (out / (Path(unit).stem + '.log')).write_text(run.stdout + run.stderr)
        errors = run.stderr.count('error:')
        result.append(dict(source=unit, command=command, exit_code=run.returncode,
                           errors=errors, original_sha256=before[unit]))
        print(unit, 'exit=' + str(run.returncode), 'errors=' + str(errors), flush=True)
        if run.returncode == 0:
            objects.append(obj)
    unchanged = all(hashlib.sha256((source / name).read_bytes()).hexdigest() == h
                    for name, h in before.items())
    report = dict(compiler=subprocess.run([str(cc), '--version'], capture_output=True, text=True).stdout,
                  source=str(source), units=result, source_files_unchanged=unchanged,
                  compiled=len(objects), linked_engine=False, executable_emitted=False,
                  note='Compile milestone only. Objects use normal little-endian ARM ABI; original binary resource and union layouts still need endian adaptation.')
    if len(objects) == len(UNITS):
        suffix = '.exe' if cc.name.endswith('.exe') else ''
        compiler_name = cc.name[:-4] if suffix else cc.name
        stem = compiler_name[:-3] if compiler_name.endswith('g++') else 'arm-none-eabi-'
        nm, ld = cc.with_name(stem + 'nm' + suffix), cc.with_name(stem + 'ld' + suffix)
        if nm.is_file():
            listing = subprocess.run([str(nm), '-C', *map(str, objects)], capture_output=True, text=True)
            (out / 'symbols-demangled.txt').write_text(listing.stdout + listing.stderr)
            raw = subprocess.run([str(nm), *map(str, objects)], capture_output=True, text=True)
            defined, undefined = set(), set()
            for line in raw.stdout.splitlines():
                words = line.split()
                if len(words) == 2 and words[0] == 'U':
                    undefined.add(words[1])
                elif len(words) >= 3:
                    defined.add(words[-1])
            unresolved = sorted(undefined - defined)
            (out / 'unresolved-symbols.txt').write_text('\n'.join(unresolved) + '\n')
            report['unresolved_symbol_count'] = len(unresolved)
        if ld.is_file():
            combined = out / 'bootstrap-relocatable.o'
            combined.unlink(missing_ok=True)
            command = [str(ld), '-r', *map(str, objects), '-o', str(combined)]
            linked = subprocess.run(command, capture_output=True, text=True)
            (out / 'relocatable-link.log').write_text(linked.stdout + linked.stderr)
            report['relocatable_link_exit_code'] = linked.returncode
            report['relocatable_link_command'] = command
            # -r never establishes runnable readiness, including when it passes.
            if linked.returncode:
                combined.unlink(missing_ok=True)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if len(objects) == len(UNITS) and unchanged else 1


if __name__ == '__main__':
    sys.exit(main())
