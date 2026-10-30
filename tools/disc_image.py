"""Private, bounded RVZ preparation using the installed DolphinTool CLI.

RVZ header layout reference: Dolphin 2503 DiscIO/WIABlob.h, WIAHeader1.
No Dolphin implementation or game assets are embedded here.
"""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def prepare_rvz(source, work, revisions, dolphin_tool=None):
    source, work = Path(source), Path(work)
    with source.open('rb') as stream:
        header = stream.read(72)
    if len(header) != 72 or header[:4] != b'RVZ\x01':
        raise ValueError('Invalid RVZ header: file extension alone is not sufficient')
    if hashlib.sha1(header[:52]).digest() != header[52:72]:
        raise ValueError('RVZ header checksum mismatch')
    iso_size, compressed_size = struct.unpack_from('>QQ', header, 36)
    initial = source.stat()
    if compressed_size != initial.st_size:
        raise ValueError('RVZ recorded size does not match the input file')
    if not 0x440 <= iso_size <= 1459978240:
        raise ValueError('RVZ expanded size is outside the GameCube disc limit')
    if shutil.disk_usage(work).free < iso_size + 16 * 1024 * 1024:
        raise ValueError('Not enough private workspace space for the expanded ISO')
    tool = shutil.which(dolphin_tool) if dolphin_tool else (
        shutil.which('DolphinTool') or shutil.which('dolphin-tool'))
    if not tool:
        raise ValueError('RVZ requires installed DolphinTool; pass --dolphin-tool /path/to/DolphinTool')
    tool = str(Path(tool).resolve())
    owned = work / 'prepared'
    owned.mkdir()  # Never reuse or overwrite another conversion's files.
    converted = owned / 'disc.iso'
    log_path = owned / 'dolphin-conversion.log'
    before = digest(source)
    try:
        probe = subprocess.run([tool, 'header', '--input', str(source), '--json'],
                               capture_output=True, text=True, check=True, timeout=60)
        info = json.loads(probe.stdout)
        supported = {(key[0], key[2]) for key in revisions}
        if (info.get('game_id'), info.get('revision')) not in supported:
            raise ValueError(f'Unsupported RVZ game/revision: {info.get("game_id")}, revision {info.get("revision")}')
        with log_path.open('wb') as log:
            subprocess.run([tool, 'convert', '--input', str(source), '--output', str(converted),
                            '--format', 'iso', '--user', str(owned / 'dolphin-user')],
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=900)
        if not converted.is_file() or converted.stat().st_size != iso_size:
            raise ValueError('DolphinTool did not produce the expected complete ISO')
        final = source.stat()
        if ((initial.st_size, initial.st_mtime_ns) != (final.st_size, final.st_mtime_ns)
                or digest(source) != before):
            raise ValueError('Source RVZ changed during conversion')
        return converted, {'format': 'RVZ', 'input_bytes': initial.st_size,
                           'input_sha256': before, 'source_preserved': True,
                           'expanded_bytes': iso_size, 'iso_sha256': digest(converted),
                           'converter_sha256': digest(tool), 'dolphin_header': info,
                           'log': str(log_path)}
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        converted.unlink(missing_ok=True)  # Only the new, owned partial output.
        if isinstance(exc, subprocess.CalledProcessError):
            detail = (exc.stderr or '')[-2000:] if isinstance(exc.stderr, str) else ''
            raise ValueError(f'DolphinTool exited {exc.returncode}; log: {log_path}; {detail}') from exc
        if isinstance(exc, subprocess.TimeoutExpired):
            raise ValueError(f'DolphinTool timed out; log: {log_path}') from exc
        raise
