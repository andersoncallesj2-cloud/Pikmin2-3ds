#!/bin/sh
# Build original engine bootstrap objects. A successful result is NOT a linked game.
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=${SOURCE_DIR:-"$script_dir/../../decomp/pikmin2-main"}
output_dir=${OUTPUT_DIR:-"$script_dir/build"}
python=${PYTHON_CMD:-python3}
if [ -n "${COMPILER:-}" ]; then
    compiler=$COMPILER
elif [ -n "${DEVKITARM:-}" ]; then
    compiler=$DEVKITARM/bin/arm-none-eabi-g++
else
    compiler=arm-none-eabi-g++
fi
"$python" "$script_dir/run_probe.py" --source "$source_dir" --output "$output_dir" --compiler "$compiler"
