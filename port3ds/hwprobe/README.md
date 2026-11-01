# HW03: independent static GPU control

The user tested ISO-PAL01 as a CIA on a New 3DS XL: clear colour appeared;
geometry became a distorted yellow wedge; the bottom heartbeat remained alive;
the overlay reported `GPU queue busy >2s (no completion)`; START froze during exit.
The photograph shows stage 7 selected but only 36 vertices in the last submitted
frame, so later selections do not establish that those stages ever reached the
GPU. No original-model/gameplay work should proceed through this hardware gate.

HW03 is a separate title (`000400000F2A7600`, **Pikmin 2 HW03 Control**) and needs
no ISO or original code/assets. It deliberately removes the Section adapter,
camera, dynamic VBO writes and resource loader. It contains five stages:

| Test | Path | Expected image |
| --- | --- | --- |
| 1 | Clear/transfer only | Magenta background |
| 2 | Float4 positions, fixed colour; 3-instruction shader with no uniform/constants | Centred white triangle, pointing sideways because this path bypasses display-tilt projection |
| 3 | Float4 position + float4 colour; same clip shader | Centred RGB-gradient triangle |
| 4 | Tight float3 VBO; official simple-triangle-style orthographic projection | White screen-space triangle |
| 5 | Tight float3 VBO; Citro3D perspective, no original camera | White perspective triangle |

The four VBO regions are 128-byte aligned within one immutable 512-byte linear
allocation. Setup logs physical address, shader metadata and projection values.
The normal Citro3D default depth state is retained; no depth/colour switches or
original GX adapter are involved. These are controls, not a proven rendering fix.

## Build and test

```sh
python tools/build_hwprobe.py --makerom /path/to/makerom
```

This creates a new ignored workspace and prints CIA/3DSX paths in its JSON report.
Use `--start-test 2` to isolate the clip triangle on a fresh launch; 1 is default.
The previous ISO workflow remains available through `tools/build.py`.

Install the HW03 CIA alongside the old title, or launch its 3DSX from
`/3ds/pikmin2-hw03/`. Use D-pad Left/Right, waiting five seconds at each stage;
START requests exit. On the configured Azahar keyboard profile these are arrows
and Enter. If a stage distorts or stalls, stop there and report that stage plus
**Last submitted test**, **Complete polls**, **STALLED**, whether the heartbeat
continues, and whether START exits. The SD log is
`/3ds/pikmin2-hw03/hardware.log`. A timeout locks test selection and rendering so
the actual failing submission is not obscured by later button presses.

The queue must drain before resources are freed. If it does not, the app leaves
in-flight storage intact and returns through libctru/APT instead of raw
`svcExitProcess`. Recovering an already wedged GPU is still **not guaranteed**.
No attempt is made to reset GPU registers or forcefully reclaim in-flight memory.

PAL02 applies the same failure latch and bounded shutdown to the existing
ISO-derived diagnostic. Its rendering path remains under investigation; it is
not evidence that the original hardware problem is fixed.

## Validation

Both targets compile/link for ARM11 and package as CIA and 3DSX. HW03's CIA has
no RomFS; its ExeFS hash/bounds are checked. The ISO builder still requires RomFS
and exact embedded converted bytes. Synthetic tests reject a damaged ExeFS and
an absent required RomFS. Existing disc/build tests pass.

Physical HW03/PAL02 results are pending. The visible hardware fault is recorded
as a failure, not reclassified as success because the banner worked in Azahar.
This control uses new native test code; no new original gameplay function is linked.

Azahar 2126.2 observations: the default 3DSX boots with magenta clear and advancing
completion polls; a separate `--start-test 2` build displays the centred white
clip-space triangle with more than 3,600 completion polls, `busy:0ms`, no errors
and no stall. Other stages, CIA launch, fault recovery and START exit are not
verified in this session. These emulator observations do not pass the hardware gate.

Primary references reviewed: installed devkitPro `simple_tri`, Citro3D
`renderqueue.c`, `buffers.c`, `attribs.c`, and libctru `shaderProgram.c`, `apt.c`,
`gfx.c`. Citro3D's `C3D_FrameSync` waits for VBlank rather than draining the command
queue. libctru's `aptExit` performs the CIA close-application sequence; raw
`svcExitProcess` skips it. These establish the shutdown corrections but do not
establish the original rendering root cause.
