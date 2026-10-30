#!/usr/bin/env python3
"""Prepare local private model data for the model-diagnostic SD folder."""
from pathlib import Path
import argparse
from convert_obj import convert


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('obj', type=Path)
    parser.add_argument('--sd-folder', type=Path, default=root / 'sd-card')
    args = parser.parse_args()
    destination = args.sd_folder.resolve() / '3ds/pikmin2-modeldiag/olimar.p2mesh'
    try:
        count = convert(args.obj.resolve(), destination, height=3.6, flat_light=True)
    except (OSError, ValueError, UnicodeError) as exc:
        parser.exit(1, f'Model preparation failed: {exc}\n')
    print(f'\nMODEL READY: {count} triangles\n{destination}\n')
    print(f'Copy the CONTENTS of this folder to the SD card root:\n{args.sd_folder.resolve()}\n')
    print('Launch Pikmin 2 Model Diagnostic; press R six times for test 7.')
    print('B turns the model. Textures/animation are not implemented by this static diagnostic.')


if __name__ == '__main__':
    main()
