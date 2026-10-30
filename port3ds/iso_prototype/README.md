# Native ISO-derived resource prototype

This target loads an actual converted `opening.bnr` extracted from a user's
validated disc, through RomFS, and draws the banner as a 3D pixel card using the
checked Citro3D backend. It is an intermediate resource-loading milestone, **not
model/level rendering or playable Pikmin 2**. No banner fallback is compiled in.

Build it using [tools/build.py](../../tools/build.py), not by placing private
assets into the source directory. The builder stages source and ROMFS in a new
ignored workspace and compiles there. The Makefile is also usable directly if
you deliberately prepare a private staged copy with its required RomFS files.

## Runtime controls

- Starts with safe clear colour and a resource status/CRC overlay.
- A shows/hides the ISO-derived card plus the diagnostic floor/spheres.
- L/R orbits the camera; B resets it; X switches colour paths; Y toggles depth.
- START exits. Do not continue testing if the console freezes.

Normal asset display needs **interleaved colour ON** and **depth ON**. Fixed
colour is intentionally a troubleshooting switch and hides the image colours.
A nonblank clear screen alone does not demonstrate loading/rendering the asset.

The app validates the converted file's magic, version, 96x32 dimensions, exact
size and CRC before drawing. It displays the source identity and first 32 hex
digits of the raw banner SHA-256, which can be compared with the build report.
Missing or corrupt resources produce a visible error, never a stock substitute.

The initial GPU representation uses two constant-colour triangles per pixel:
18,432 banner vertices plus 10,080 diagnostic vertices = **28,512**, within the
32,768-vertex checked allocation. There is no GPU texture upload or new shader
format in this milestone. Source RGB5A3 tiles become row-major RGB8; alpha is
composited on black. No image resizing, model decimation or gameplay changes
are performed. This is appropriate for a tiny banner, not a proposed rendering
method for large game textures.

Runtime asset pixels use 9,216 static bytes. The existing linear VBO is 917,504
bytes; render targets and service allocations remain additional measured costs.
Free linear/VRAM and vertex counts are shown, rather than assuming the physical
New 3DS RAM is fully available. This app's descriptor still selects the existing
64 MiB application mode. No real-hardware memory/performance target is established.

## Original source and native boundaries

The app executes the existing extracted original `Section` lifecycle and original
`C_MTXLookAt` via their narrow, documented adapters. Original debug sphere/line
methods render the reference geometry. Source hashes are recorded per build.
RomFS, libctru input/lifecycle and PICA/Citro3D submission are native 3DS code.

| Original supplied path | Invoked functions | Port boundary |
| --- | --- | --- |
| `src/sysGCU/section.cpp` | `Section::run`, `loading`, `fadeIn`, `main`, `fadeOut`, frame/render/update/draw methods | Extracted bodies in `P2Engine` namespace; diagnostic callbacks replace platform services |
| `src/Dolphin/mtx/mtx.c` | `C_MTXLookAt` | Scalar vector helpers and native matrix submission |
| `src/sysGCU/graphics.cpp` | `Graphics::drawSphere`, `Graphics::drawLine` | Minimal type shim; GX lines become Citro3D triangle ribbons |

Exact extraction metadata and original CC0 attribution remain in
[`engine/provenance.json`](../engine/provenance.json),
[`game_math/provenance.json`](../game_math/provenance.json),
[`graphics/provenance.json`](../graphics/provenance.json) and
[`LICENSE.source.md`](../LICENSE.source.md). The source-only audit is in
[`docs/ENGINE_INTEGRATION.md`](../../docs/ENGINE_INTEGRATION.md).

It does not initialize original System/GameFlow/BootSection/SingleGameSection,
JKR resource lifetimes, J3D models, original gameplay objects, collision or JAudio.
No desktop SDL/OpenGL backend or replacement captain/Pikmin simulation is used.
Audio is deliberately absent and DSP firmware is not required.

Hardware rendering remains blocked on the user's New 3DS XL test. The same checked
nonblocking queue path as HW01 is reused; it does not prove that the original
hardware freeze is fixed. HW01 and HW02 are preserved as independent targets.

## Validation scope

Integration tests generate a **synthetic disc fixture** with its own executable
hash, injected through a test-only Python API. The production CLI does not accept
that fixture and offers no checksum bypass. Both the report and running test app
say `SYNTHETIC TEST FIXTURE`. Test success is not a real Pikmin ISO success claim.

Two independent compilations/packages succeed with different fixture banner
bytes. Both CIA/3DSX outputs change, embedded asset presence and NCCH section
hashes are checked, and source fixtures remain unchanged. CIA emulator tests
verify install/boot, visible card, source/CRC overlay, camera control and exit.
No real game dump or physical console was available for this pipeline session.

The public fixture smoke build is legal synthetic data, not an original game
banner. **A CIA produced from a real ISO contains private copyrighted data and
must not be committed or published.**
