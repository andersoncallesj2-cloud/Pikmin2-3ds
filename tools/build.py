#!/usr/bin/env python3
"""Build a native, ISO-dependent banner resource prototype; not full Pikmin 2."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

import disc_asset
import disc_image

ROOT = Path(__file__).resolve().parents[1]


def sdk_environment(work):
    env = os.environ.copy()
    sdk = env.get('DEVKITARM')
    if os.name == 'nt' and sdk and not (Path(sdk) / '3ds_rules').is_file():
        make = shutil.which('make')
        if make:
            candidate = Path(make).resolve().parents[3] / 'devkitARM'
            if (candidate / '3ds_rules').is_file():
                sdk = str(candidate)
    if not sdk or not (Path(sdk) / '3ds_rules').is_file():
        raise ValueError('Activate installed devkitPro devkitARM/libctru/Citro3D first (DEVKITARM/3ds_rules missing)')
    sdk = Path(sdk).resolve()
    env['DEVKITARM'] = sdk.as_posix()
    env['DEVKITPRO'] = sdk.parent.as_posix()
    env['PATH'] = os.pathsep.join([str(sdk / 'bin'), str(sdk.parent / 'tools/bin'), env.get('PATH', '')])
    if os.name == 'nt':
        temp = Path(work) / 'tmp'
        temp.mkdir()
        env['TMPDIR'] = temp.as_posix()
    compiler = sdk / 'bin' / ('arm-none-eabi-gcc.exe' if os.name == 'nt' else 'arm-none-eabi-gcc')
    return env, compiler


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify_cia(path, asset=None):
    data = path.read_bytes()
    if len(data) < 32:
        raise ValueError('Packager produced a truncated CIA')
    header, _, _, cert, ticket, tmd, _, size = struct.unpack_from('<IHHIIIIQ', data)
    align = lambda n: (n + 63) & ~63
    start = align(align(align(align(header) + cert) + ticket) + tmd)
    if start + size > len(data) or size < 512:
        raise ValueError('CIA content extent is invalid')
    ncch = data[start:start+size]
    if ncch[0x100:0x104] != b'NCCH' or not (ncch[0x18f] & 4):
        raise ValueError('Expected unencrypted homebrew NCCH')
    for offset, digest_offset in ((0x1a0, 0x1c0), (0x1b0, 0x1e0)):
        section, section_size, hash_size = struct.unpack_from('<III', ncch, offset)
        if offset == 0x1b0 and asset is None and (section, section_size, hash_size) == (0, 0, 0):
            if any(ncch[digest_offset:digest_offset+32]):
                raise ValueError('Absent RomFS has a nonzero digest')
            continue
        a, length = section*512, hash_size*512
        if not section or not 0 < hash_size <= section_size or a + section_size*512 > len(ncch):
            raise ValueError('NCCH section bounds invalid')
        if hashlib.sha256(ncch[a:a+length]).digest() != ncch[digest_offset:digest_offset+32]:
            raise ValueError('NCCH section header hash mismatch')
    if asset is not None and asset not in ncch:
        raise ValueError('CIA does not contain the converted ISO asset')
    return {'sha256': sha(data), 'bytes': len(data), 'ncch_sha256': sha(ncch),
            'asset_embedded': asset is not None, 'section_header_hashes': 'match',
            'signature_scope': 'homebrew; not Nintendo-signed'}


def run(iso, target='cia', work_dir=None, makerom=None, revisions=None, dolphin_tool=None, start_test=1):
    if target not in ('cia', '3dsx', 'both'):
        raise ValueError('Target must be cia, 3dsx or both')
    if start_test not in range(1, 8):
        raise ValueError('Start test must be between 1 and 7')
    iso = Path(iso).resolve()
    if work_dir is None:
        (ROOT / 'build').mkdir(exist_ok=True)
        work = Path(tempfile.mkdtemp(prefix='iso-prototype-', dir=ROOT / 'build'))
    else:
        work = Path(work_dir).resolve()
        if work.exists() and any(work.iterdir()):
            raise ValueError('Work directory is not empty; use a fresh directory to preserve earlier files')
        for protected in ('port3ds', 'tools', 'tests'):
            if work.is_relative_to(ROOT / protected):
                raise ValueError('Work directory must be outside source directories')
        work.mkdir(parents=True, exist_ok=True)
    # Every fresh workspace opts out of Git, even if a custom path is inside
    # another checkout. Outputs contain private data from the user's disc.
    (work / '.gitignore').write_text('*\n')
    output = work / 'output'
    output.mkdir()
    report = {'prototype': 'ISO-derived opening.bnr resource probe / ISO-PAL02 diagnostics', 'stages': {},
              'built_at_utc': datetime.now(timezone.utc).isoformat(),
              'diagnostic_start_test': start_test,
              'hardware': 'UNVERIFIED: previous New 3DS rendering failure remains open',
              'gameplay': 'not implemented', 'work_dir': str(work), 'outputs': {}}
    report_path = output / 'build-report.json'
    stage = 'ISO Validation'
    build_env = os.environ.copy()

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        lines = ['PIKMIN 2 3DS BUILDER — RESOURCE PROTOTYPE',
                 f'Data: {"SYNTHETIC TEST FIXTURE" if report.get("disc", {}).get("test_fixture") else "input validation/result below"}']
        lines += [f'{name}: {value["status"]}' + (': ' + value['error'] if 'error' in value else '')
                  for name, value in report['stages'].items()]
        lines += [f'{name}: {info["path"]}' for name, info in report['outputs'].items()]
        lines += ['Hardware: UNVERIFIED. Full Pikmin 2 gameplay: NOT IMPLEMENTED.']
        (output / 'build-report.txt').write_text('\n'.join(lines) + '\n')

    def passed(name, **evidence):
        report['stages'][name] = {'status': 'PASS', **evidence}
        print(f'{name}: PASS', flush=True)
        save()

    def command(args, cwd, logfile):
        try:
            with logfile.open('wb') as log:
                subprocess.run([str(x) for x in args], cwd=cwd, env=build_env,
                               stdout=log, stderr=subprocess.STDOUT, check=True)
        except subprocess.CalledProcessError as exc:
            report['failed_command'] = {'argv': [str(x) for x in args],
                                        'exit_code': exc.returncode, 'log': str(logfile)}
            tail = logfile.read_text(errors='replace')[-4000:]
            raise ValueError(f'{args[0]} exited {exc.returncode}; log: {logfile}\n{tail}') from exc

    print('PIKMIN 2 3DS BUILDER — resource prototype, no gameplay', flush=True)
    try:
        with iso.open('rb') as source:
            magic = source.read(4)
        if magic == b'RVZ\x01':
            stage = 'RVZ Preparation'
            iso, preparation = disc_image.prepare_rvz(iso, work,
                disc_asset.REVISIONS if revisions is None else revisions, dolphin_tool)
            report['input_preparation'] = preparation
            passed(stage, **preparation)
            stage = 'ISO Validation'
        banner, identity = disc_asset.extract(iso, revisions)
        report['disc'] = identity
        passed(stage, disc_id=identity['disc_id'], revision=identity['revision_name'], dol_sha1=identity['dol_sha1'])
        print('Game Revision: ' + ('SYNTHETIC TEST OVERRIDE' if identity['test_fixture'] else 'KNOWN EXECUTABLE MATCH')
              + ' — ' + identity['revision_name'], flush=True)
        if identity['test_fixture']:
            print('DATA SOURCE: SYNTHETIC TEST FIXTURE — not a real Pikmin 2 dump', flush=True)
        stage = 'Asset Extraction'
        extracted = work / 'extracted'
        extracted.mkdir()
        (extracted / 'opening.bnr').write_bytes(banner)
        passed(stage, source='opening.bnr', bytes=len(banner), sha256=sha(banner))
        stage = 'Asset Conversion'
        asset = disc_asset.convert(banner)
        converted = work / 'converted'
        converted.mkdir()
        (converted / 'banner.p2b').write_bytes(asset)
        passed(stage, conversion='BE tiled RGB5A3 -> row-major RGB8, alpha on black',
               width=96, height=32, sha256=sha(asset), bytes=len(asset))

        stage = 'Runtime Compilation'
        if any(c.isspace() for c in str(work)):
            raise ValueError('The devkitPro make template needs a workspace path without whitespace; select --work-dir accordingly (ISO paths may contain spaces)')
        build_env, compiler = sdk_environment(work)
        report['compiler_version'] = subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0]
        runtime = work / 'runtime'
        # Copy only source/shared runtime data, never previous generated assets,
        # user data, build outputs, private model files or existing ROMFS files.
        ignore = shutil.ignore_patterns('build', '__pycache__', 'romfs', 'validation',
                                       '*.o', '*.elf', '*.map', '*.smdh', '*.cia', '*.3dsx')
        for directory in ('include', 'game_math', 'graphics', 'engine', 'hwdiag', 'assets', 'iso_prototype'):
            shutil.copytree(ROOT / 'port3ds' / directory, runtime / directory, ignore=ignore)
        app = runtime / 'iso_prototype'
        # Also avoids mkdir -p traversing restricted ancestors in Windows MSYS.
        (app / 'build').mkdir()
        romfs = app / 'romfs'
        romfs.mkdir()
        (romfs / 'banner.p2b').write_bytes(asset)
        (romfs / 'start-test.txt').write_bytes(f'{start_test}\n'.encode('ascii'))
        title = 'SYNTHETIC TEST FIXTURE' if identity['test_fixture'] else 'USER DISC: Pikmin 2 resource'
        (romfs / 'source.txt').write_text(f'{title}\n{identity["disc_id"]} revision {identity["revision"]}\n{identity["asset_sha256"]}\nopening.bnr\n', encoding='ascii')
        logs = work / 'logs'
        logs.mkdir()
        command(['make', '-j2'], app, logs / 'compile.log')
        elf = (app / 'pikmin2-iso-prototype.elf').read_bytes()
        if elf[:4] != b'\x7fELF' or elf[4:6] != b'\x01\x01' or struct.unpack_from('<H', elf, 18)[0] != 40:
            raise ValueError('Runtime output is not a fresh 32-bit little-endian ARM ELF')
        x3ds = (app / 'pikmin2-iso-prototype.3dsx').read_bytes()
        if x3ds[:4] != b'3DSX' or asset not in x3ds:
            raise ValueError('3DSX is missing its converted RomFS resource')
        report['runtime'] = {'elf_sha256': sha(elf), '3dsx_sha256': sha(x3ds),
                             'romfs_asset_sha256': sha(asset), 'source_c_sha256': sha((app / 'source/main.c').read_bytes())}
        report['source_hashes'] = {name: sha((runtime / name).read_bytes()) for name in (
            'iso_prototype/Makefile', 'engine/original_section.cpp', 'game_math/original_math.c',
            'graphics/original_graphics.cpp', 'hwdiag/source/diagnostic_gpu.c')}
        passed(stage, architecture='ARM11/ARMv6K', original_components='Section lifecycle, camera/game math, debug graphics',
               logs=str(logs / 'compile.log'))
        if target in ('3dsx', 'both'):
            dest = output / 'Pikmin2-3DS.3dsx'
            shutil.copy2(app / 'pikmin2-iso-prototype.3dsx', dest)
            report['outputs']['3dsx'] = {'path': str(dest), 'sha256': sha(dest.read_bytes()), 'asset_embedded': True}

        if target in ('cia', 'both'):
            stage = 'CIA Packaging'
            packager = shutil.which(makerom or 'makerom')
            if not packager:
                raise ValueError('makerom not found; install it or pass --makerom /path/to/makerom')
            packager = str(Path(packager).resolve())
            report['packager_sha256'] = sha(Path(packager).read_bytes())
            dest = output / 'Pikmin2-3DS.cia'
            command([packager, '-f', 'cia', '-o', dest, '-rsf', 'prototype.rsf', '-target', 't',
                     '-elf', 'pikmin2-iso-prototype.elf', '-icon', 'pikmin2-iso-prototype.smdh',
                     '-banner', '../assets/menu.bnr', '-desc', 'app:4'], app, logs / 'package.log')
            details = verify_cia(dest, asset)
            report['outputs']['cia'] = {'path': str(dest), **details}
            passed(stage, **details)
        report['result'] = 'BUILD PASSED; runtime/hardware results require separate testing'
        save()
        for value in report['outputs'].values():
            print('\nOUTPUT: ' + value['path'], flush=True)
        print('\nHardware: UNVERIFIED. Real-disc validation: ' + ('NOT TESTED (synthetic fixture)' if identity['test_fixture'] else 'known executable and bounded asset matched; on-device test still required'))
        return report, work
    except (OSError, ValueError, UnicodeError, struct.error, subprocess.CalledProcessError) as exc:
        if stage == 'CIA Packaging':
            # Only this fresh build's own failed CIA is removed, never user data.
            (output / 'Pikmin2-3DS.cia').unlink(missing_ok=True)
        report['stages'][stage] = {'status': 'FAIL', 'error': str(exc)}
        report['result'] = 'FAILED; do not use partial output'
        save()
        print(f'{stage}: FAIL: {exc}\nReport: {report_path}', flush=True)
        raise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--iso', type=Path, required=True)
    p.add_argument('--target', choices=['cia', '3dsx', 'both'], default='cia')
    p.add_argument('--work-dir', type=Path)
    p.add_argument('--makerom')
    p.add_argument('--dolphin-tool', help='Installed DolphinTool executable for RVZ input')
    p.add_argument('--start-test', type=int, choices=range(1, 8), default=1,
                   help='Initial graphics diagnostic (default: safe clear; 7: ISO banner)')
    args = p.parse_args()
    try:
        run(args.iso, args.target, args.work_dir, args.makerom, dolphin_tool=args.dolphin_tool, start_test=args.start_test)
    except (OSError, ValueError, UnicodeError, struct.error, subprocess.CalledProcessError) as exc:
        p.exit(1, f'Build failed: {exc}\n')


if __name__ == '__main__':
    main()
