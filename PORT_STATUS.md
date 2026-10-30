# Port status — limited ISO resource pipeline; hardware gate open

Target: genuine original-engine native New Nintendo 3DS port. No remake or
emulation path. Current priority: **Phase 0, real-hardware renderer diagnosis**.

## Observed problem

The user reports the previous viewer launches on a New 3DS XL but shows no 3D
scene. Bottom text appears; the app may freeze and require a hard restart.
Exact launch format and faulting GPU command remain unknown. **Not fixed yet.**

The earlier emulator-tested viewer is preserved unchanged under `port3ds/`,
including its existing CIA/3DSX. Its historical validation remains emulator-only.
The new independent test target is `port3ds/hwdiag/` with title F2A73 / build HW01.

## Hardware diagnostic audit

- Audited VBO allocation/layout, shader, render target, projection, depth, cache
  flush, frame/queue submission against Citro3D and devkitPro examples.
- Found ignored results for frame begin/draw-on, buffer registration, uniform
  lookup and cache flush. These obscure failure; they do not establish root cause.
- Found blocking queue waits in the baseline's frame/cleanup path. A missing GPU
  completion is consistent with the reported freeze, but not proven to cause it.
- Added clear, triangle, cube, checkerboard, spheres and full-scene diagnostics.
- Added fixed/interleaved colour and depth switches, explicit state setup,
  128-byte-aligned linear VBO, checked setup/submission, heartbeat, free linear/VRAM
  observations and SD logging. No texture path exists in the baseline to audit.
- Added nonblocking queue polling and >2-second missing-completion reporting,
  independent console flushing, and exit handling that avoids waiting on a known
  busy queue. Real stuck-GPU recovery remains unverified.
- Preserved the original scene and original-source extracts. No additional game
  systems were integrated while the hardware gate is open.

## ISO resource prototype session

- Added `tools/build.py`: real bounded disc validation, `opening.bnr` extraction,
  RGB5A3 conversion, staged ARM11 compilation, and CIA/3DSX packaging. A matching
  GPVE01 disc-0/revision-0 DOL checksum is required. No real dump is available;
  that revision is an initial allowlist, not an experimentally verified disc.
- Added `port3ds/iso_prototype/`, a native RomFS resource probe using the existing
  original Section/math/debug-graphics extracts and the checked HW01 backend.
  Starts with clear colour; A displays the supplied image beside the spheres.
  Runtime resource checks, camera controls, input and visible errors are present.
- No new gameplay systems or original JKR/J3D loader were integrated. This uses
  native resource I/O for one tiny original file, not the full engine startup.
- Thirteen host tests pass. Two actual SDK builds/packages with different
  **synthetic** banners pass; changing the asset changes CIA content and 3DSX.
  Inputs remain unchanged. Production CLI rejects the test executable hash.
- The exact public synthetic CIA installs, renders its generated card, responds
  to camera controls and exits in Azahar. [Evidence](port3ds/iso_prototype/validation/README.md).
  Public test builds contain no real game assets. Physical hardware is untested.
- Updated the no-Docker SDK tutorial, output paths, revision limits and privacy
  rules. RVZ/CISO adapters and genuine game model conversion remain unimplemented.

## Evidence levels

| Component | What is established | Hardware status |
| --- | --- | --- |
| Old viewer | Compiles/links; original Section/debug graphics/math execute in Azahar | User reports no scene / possible freeze |
| HW01 diagnostic | ARM build and emulator tests recorded in hwdiag validation | Awaiting user's test |
| ISO resource pipeline | Synthetic extraction/conversion, two real ARM builds/packages, CIA emulator test | Real dump and New 3DS test pending |
| Full engine bootstrap | Five entire original translation units compile to ARM objects | Not linked as full engine, not executed |
| Original math | `src/Dolphin/mtx/mtx.c::C_MTXLookAt`, `src/sysCommonU/sysMath.cpp::{qdist2,roundAng,angDist}` execute through narrow adapters; host tests/provenance retained | Not proven correct on physical 3DS |
| Original Section | Extracted `src/sysGCU/section.cpp` lifecycle methods execute with diagnostic callbacks in emulator | Not original Boot/SingleGameSection |
| Audio | Original THP decoder port passes host vector tests; silent fallback in emulator | No audible-output validation |
| Assets/gameplay | Private supplied OBJ tested separately; no genuine disc resource tested | No original levels/AI/gameplay running |

Using the user's stage definitions: selected functions have reached stage 3 in
an emulator, not full-engine stage 4 and not hardware verification. Full-game
compilation probes are stage 1 only. No original engine asset-loading subsystem,
original model rendering or gameplay stages have been demonstrated. The native
banner resource probe does not change those engine-stage claims.

The tiny `.iso` files used for parser/build tests are generated fixtures, not
game dumps. No actual user ISO/RVZ/CISO is accessible. None is needed for Phase 0.
Do not publish game assets or interpret fixtures as a supported game revision.

## Next single milestone

User confirms HW01 triangle/checkerboard/spheres on real New 3DS XL, with log and
screenshots. Privately validate the new builder against the user's matching ISO
when available, then compare the rendered banner and runtime hash with extracted
bytes. After the hardware gate, the next asset milestone is a bounded original
archive/J3D model-loading path, not a replacement gameplay simulation.
