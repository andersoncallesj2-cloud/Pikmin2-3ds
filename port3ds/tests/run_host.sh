#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT HUP INT TERM
cc="${CC:-cc}"
cxx="${CXX:-c++}"
flags="-O1 -g -Wall -Wextra -Werror -ffp-contract=off"
if [ "${SANITIZE:-0}" = 1 ]; then
    flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer"
    # LeakSanitizer cannot operate inside the executor's ptrace sandbox.
    export ASAN_OPTIONS="detect_leaks=0"
fi
"$cc" -std=c11 $flags -Iinclude -Igame_math -c game_math/original_math.c -o "$work/original_math.o"
"$cc" -std=c11 $flags -Iinclude -Igame_math -c game_math/port_math.c -o "$work/port_math.o"
"$cc" -std=c11 $flags -Iinclude -Igame_math tests/math_test.c "$work/original_math.o" "$work/port_math.o" -lm -o "$work/math"
"$work/math"
"$cxx" -std=c++11 $flags -Iinclude tests/graphics_test.cpp graphics/original_graphics.cpp "$work/original_math.o" "$work/port_math.o" -lm -o "$work/graphics"
"$work/graphics"
"$cc" -std=c11 $flags -Iaudio/compat -Iaudio audio/thp_decode_port.c audio/thp_frame.c tests/audio_decode_test.c -o "$work/audio"
"$work/audio"
"$cxx" -std=c++11 $flags -fno-rtti -fno-exceptions -Iinclude -Iengine engine/original_section.cpp engine/port_engine.cpp tests/engine_test.cpp -o "$work/engine"
"$work/engine"
