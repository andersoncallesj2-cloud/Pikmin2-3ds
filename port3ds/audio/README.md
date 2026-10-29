# Initial audio subsystem port

`original/THPAudio.c` is a byte-for-byte copy of the supplied decompilation's
`src/Dolphin/thp/THPAudio.c`. This is the real THP ADPCM decoder used by Pikmin 2's
movie-audio path, called from `src/sysGCU/THPAudioDecode.c`.

`thp_decode_port.c` retains that decoder's control flow, channel layout,
prediction, rounding and saturation. Three mechanical portability changes avoid
undefined signed shifts: negative residual scaling and accumulator shifting use
64-bit multiplication; nibble sign extension uses explicit subtraction. The
loop counter uses the sample-count unsigned type. The narrow compatibility
header replaces SDK typedefs and does not emulate the OS.

`thp_frame.c` checks input lengths and capacity, bounds sample count and channel
offsets, and converts the big-endian record header to native endianness before
calling `THPAudioDecode`. It accepts a single THP audio component, not an entire
movie. Output order follows the game's original routine (right,left).

`ndsp_audio.c` routes decoded stereo PCM to the 3DS NDSP service. The whistle is
new synthetic ADPCM data decoded through the real ported game routine, not an
original Pikmin audio asset. NDSP initialization failure disables audio and
reports a status string; rendering and controls can continue. No blocking audio
queue or busy wait runs on the game thread. Homebrew may require a valid dumped
DSP firmware at the platform's normal location.

This does **not** port JAudio/JAS, the GameCube DSP microcode, music sequencing,
THP video playback, or movie streaming. No original game sound was supplied or
tested. Real THP components can be decoded by the checked wrapper in future.

Build all `audio/*.c` (not `audio/original/THPAudio.c`) with include directories
`include`, `audio`, and `audio/compat`; link `-lctru -lm` on 3DS. The source
decompilation's CC0 license applies to the preserved/ported decoder.

Host verification: `cc -std=c11 -O1 -g -fsanitize=undefined,address
-Iaudio/compat -Iaudio audio/thp_decode_port.c audio/thp_frame.c
tests/audio_decode_test.c -o /tmp/audio-test && ASAN_OPTIONS=detect_leaks=0 /tmp/audio-test`.
The sanitizer run passed 169 independent checks. LeakSanitizer is disabled only
because the command sandbox uses ptrace, which it does not support; address and
undefined-behavior sanitizers remain enabled.
