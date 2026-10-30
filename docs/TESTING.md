# HW01: New 3DS XL rendering test

**The rendering failure is NOT confirmed fixed.** The user reports that the
previous build showed bottom-screen text, no scene, and apparently froze so
START could not exit. Emulator success is not hardware evidence.

## Install this separate diagnostic

Download `downloads/pikmin2-hwdiag.3dsx` or `downloads/pikmin2-hwdiag.cia`.

1. Prefer the 3DSX first: copy it to `/3ds/pikmin2-hwdiag/` on your SD card,
   then launch it with Homebrew Launcher. No game dump or DSP firmware is needed.
2. Alternatively install the CIA using your existing homebrew installer. It is
   **Pikmin 2 HW Diagnostic**, title `000400000F2A7300`, separate from Port Lab.
3. It starts at **1 CLEAR COLOUR**: expect a bright magenta top screen and a
   bottom-screen heartbeat number that advances. Wait at least five seconds.
4. Press **R / D-pad Right** once for each next stage. **L / Left** goes back.
   Wait five seconds at each stage and note the first unexpected result.

| Test | Expected top screen | Default colour / depth |
| --- | --- | --- |
| 1 CLEAR COLOUR | Solid magenta | No geometry |
| 2 BASIC TRIANGLE | Large yellow triangle | Fixed / off |
| 3 BASIC CUBE | Small solid yellow cube | Fixed / on |
| 4 CHECKERBOARD | Green checkerboard | Interleaved / on |
| 5 THREE SPHERES | Three wire spheres, no floor | Interleaved / on |
| 6 FULL DEBUG SCENE | Checkerboard plus three spheres | Interleaved / on |

**X** switches FIXED versus interleaved colour. **A** toggles depth. Stage changes
restore the defaults above. Fixed colour intentionally makes the whole scene
one yellow colour, so test 4 will lose its alternating squares in that mode.
**START** exits. No audio runs in this diagnostic.

If a stage stays blank but the heartbeat advances, photograph both screens.
If possible, try X once, then A once and report whether either makes geometry
appear. If the heartbeat stops or exit fails, **stop testing later stages**;
send the last visible test/status. Do not repeat a known hanging stage just to
collect more evidence. Relaunching always starts safely at clear colour.

## Retrieve the log

The app attempts to append to:

`/3ds/pikmin2-hwdiag/hardware.log`

The bottom screen says whether the file opened. Remove the SD only after leaving
or shutting down the app/console normally. Send the log plus a photo of both
screens and tell us **CIA versus 3DSX**, console model, last test, colour/depth
setting, whether the heartbeat advanced, and whether START exited. No ISO, game
assets or firmware are needed for this report.

## What the checks actually mean

PASS means that a particular API returned success or an allocation/uniform was
found. It does **not** prove the GPU displayed pixels. Attributes/buffer/cache
checks remain `not run` on clear colour, because that stage submits no vertices.
The API result is the last real Result value, not a fabricated GPU error code.

The diagnostic polls `C3D_FrameBegin(C3D_FRAME_NONBLOCK)` rather than entering an
unbounded GPU queue wait. Queue-busy milliseconds are measured from unsuccessful
polls. After two seconds it reports missing completion. It does not overwrite
an in-flight vertex buffer, and the CPU console is flushed independently of
GPU progress. Heartbeat/loop FPS are CPU-loop measurements, not game performance.

START avoids Citro3D's blocking cleanup if the queue is observed busy and uses
process exit with in-flight allocations left intact. This does not guarantee
recovery from a genuinely wedged GPU/driver or kernel. Such a fault still requires
hardware observations; the timeout path has not been validated on a hung console.

## Acceptance gate

Do not resume major engine integration until this exact diagnostic's triangle,
checkerboard and spheres work stably on the user's New 3DS XL. Emulator rendering,
API status, screenshots and successful compilation alone do not satisfy this gate.

## ISO resource prototype

This is a separate title, **Pikmin 2 ISO Prototype**, ID `000400000F2A7500`.
Test HW01 first; stop if any known stage freezes the console.

1. Build privately with [tools/build.py](BUILDING.md), or use the explicitly
   synthetic `downloads/pikmin2-iso-fixture.cia` / `.3dsx` for a data-free smoke
   test. The public build contains a colour pattern, not a Pikmin banner/model.
2. Install the CIA through your existing homebrew installer, or copy the 3DSX to
   `/3ds/pikmin2-iso-prototype/` and launch with Homebrew Launcher. RomFS is inside
   each application; no external SD asset copy or DSP firmware is needed.
3. On launch expect magenta top-screen clear colour, an advancing frame number
   and a bottom-screen resource CRC. For the public synthetic build expect
   `SYNTHETIC TEST FIXTURE`, raw banner SHA prefix
   `b2e7835f8b5e3e587c56d7b6dad0b666` and CRC `8dad7166`.
4. Press **A**. Expect a rectangular image above/behind the three spheres and
   checkerboard. With a real supported ISO this must be that dump's banner;
   compare the displayed raw SHA prefix with `build-report.json`. A clear screen
   alone does not prove resource rendering. Default vertex count is 28,512.
5. Hold **L/R** to orbit, **B** to reset. **X** changes the colour path (fixed
   colour deliberately obscures image colours). **Y** changes depth. Restore
   interleaved colour and depth ON to judge the image. **START** should exit.
6. Report launch format, console model, whether frames advance, first failure,
   resource CRC/hash, GPU/queue/vertex status and whether START exits. A photo of
   both screens is useful. Do not publicly upload real game images or packages.

The ISO prototype does not write a separate SD log; photograph the overlay and
use HW01's log for lower-level failures. If exit fails or the heartbeat freezes,
stop further stages. The same nonblocking path is reused, but recovery from a
wedged hardware GPU remains unproven. The real-hardware issue is still open.
