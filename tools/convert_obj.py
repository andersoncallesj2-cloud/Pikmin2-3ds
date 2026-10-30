#!/usr/bin/env python3
"""Convert a user-supplied OBJ/MTL to a bounded untextured diagnostic mesh.

No model is downloaded or distributed by this tool. This is a static mesh test,
not a Pikmin 2 J3D asset converter or game-engine model loader.
"""
import argparse
import math
from pathlib import Path
import struct
import warnings
import zlib

MAX_TRIANGLES = 6000


def convert(source, destination, up='y', height=2.4):
    source = Path(source)
    if source.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('OBJ exceeds the 16 MiB input limit')
    vertices, triangles, materials = [], [], {}
    color = (220, 220, 220, 255)
    for line in source.read_text(encoding='utf-8-sig').splitlines():
        words = line.split('#', 1)[0].split()
        if not words:
            continue
        if words[0] == 'mtllib':
            # Never read paths outside the user-selected model directory.
            joined = ' '.join(words[1:])
            names = [joined] if (source.parent / joined).is_file() else words[1:]
            for name in names:
                mtl = (source.parent / name).resolve()
                if not mtl.is_relative_to(source.parent.resolve()):
                    raise ValueError('MTL path escapes the model directory')
                if not mtl.is_file():
                    warnings.warn(f'Missing {mtl.name}; using gray for unavailable material colors')
                    continue
                if mtl.stat().st_size > 1024 * 1024:
                    raise ValueError('Oversized MTL file')
                material = None
                for row in mtl.read_text(encoding='utf-8-sig').splitlines():
                    parts = row.split('#', 1)[0].split()
                    if not parts:
                        continue
                    if parts[0] == 'newmtl' and len(parts) >= 2:
                        material = ' '.join(parts[1:])
                    elif parts[0] == 'Kd' and material and len(parts) == 4:
                        rgb = tuple(float(x) for x in parts[1:])
                        if not all(math.isfinite(x) for x in rgb):
                            raise ValueError('Non-finite material color')
                        materials[material] = tuple(round(max(0, min(1, x)) * 255) for x in rgb) + (255,)
        elif words[0] == 'v':
            if len(words) < 4 or len(vertices) >= 100000:
                raise ValueError('Invalid vertex or vertex limit exceeded')
            v = tuple(float(x) for x in words[1:4])
            if not all(math.isfinite(x) for x in v):
                raise ValueError('Non-finite vertex')
            vertices.append(v if up == 'y' else (v[0], v[2], -v[1]))
        elif words[0] == 'usemtl':
            color = materials.get(' '.join(words[1:]), (220, 220, 220, 255))
        elif words[0] == 'f':
            if not 4 <= len(words) <= 65:
                raise ValueError('Only faces with 3–64 corners are supported')
            indices = []
            for word in words[1:]:
                n = int(word.split('/')[0])
                i = n - 1 if n > 0 else len(vertices) + n
                if n == 0 or not 0 <= i < len(vertices):
                    raise ValueError('Invalid face vertex index')
                indices.append(i)
            # Fan triangulation is appropriate for convex exported faces.
            for j in range(1, len(indices) - 1):
                triangles.append((indices[0], indices[j], indices[j + 1], color))
                if len(triangles) > MAX_TRIANGLES:
                    raise ValueError('Model exceeds 6000 triangles; simplify it before export')
    if not triangles or not math.isfinite(height) or not .01 <= height <= 10:
        raise ValueError('No triangles or invalid target height')
    used = [vertices[i] for triangle in triangles for i in triangle[:3]]
    lo = [min(v[k] for v in used) for k in range(3)]
    hi = [max(v[k] for v in used) for k in range(3)]
    if not math.isfinite(hi[1] - lo[1]) or hi[1] - lo[1] <= 1.e-9:
        raise ValueError('Model has no vertical extent; check the up axis')
    scale = height / (hi[1] - lo[1])
    origin = (lo[0]/2 + hi[0]/2, lo[1], lo[2]/2 + hi[2]/2)
    payload = bytearray()
    for triangle in triangles:
        for i in triangle[:3]:
            position = tuple((vertices[i][k] - origin[k]) * scale for k in range(3))
            if not all(math.isfinite(x) for x in position) or max(abs(x) for x in position) > 20:
                raise ValueError('Extreme aspect ratio exceeds the diagnostic bounds')
            payload.extend(struct.pack('<3f4B', *position, *triangle[3]))
    header = struct.pack('<4sIIIII', b'P2M1', 1, len(triangles) * 3, 16, len(payload), zlib.crc32(payload))
    destination = Path(destination)
    if destination.exists():
        raise ValueError('Output exists; choose a new path or remove the old generated file')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('xb') as out:
        out.write(header)
        out.write(payload)
    return len(triangles)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('obj', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--up', choices=['y', 'z'], default='y')
    parser.add_argument('--height', type=float, default=2.4)
    args = parser.parse_args()
    try:
        n = convert(args.obj, args.output, args.up, args.height)
    except (OSError, ValueError, UnicodeError) as exc:
        parser.exit(1, f'Conversion failed: {exc}\n')
    print(f'Converted {n} triangles to {args.output}; static mesh, material colors only, no textures/animation.')
