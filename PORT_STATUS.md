# Port status — HW01 hardware gate

Target: genuine original-engine native New Nintendo 3DS port. No remake or
emulation path. Current priority: **Phase 0, real-hardware renderer diagnosis**.

## Observed problem

The user reports the previous viewer launches on a New 3DS XL but shows no 3D
scene. Bottom text appears; the app may freeze and require a hard restart.
Exact launch format and faulting GPU command remain unknown. **Not fixed yet.**

The earlier emulator-tested viewer is preserved unchanged under `port3ds/`,
including its existing CIA/3DSX. Its historical validation remains emulator-only.
The new independent test target is `port3ds/hwdiag/` with title F2A73 / build HW01.

## This session

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

## Evidence levels

| Component | What is established | Hardware status |
| --- | --- | --- |
| Old viewer | Compiles/links; original Section/debug graphics/math execute in Azahar | User reports no scene / possible freeze |
| HW01 diagnostic | ARM build and emulator tests recorded in hwdiag validation | Awaiting user's test |
| Full engine bootstrap | Five entire original translation units compile to ARM objects | Not linked as full engine, not executed |
| Original math | `src/Dolphin/mtx/mtx.c::C_MTXLookAt`, `src/sysCommonU/sysMath.cpp::{qdist2,roundAng,angDist}` execute through narrow adapters; host tests/provenance retained | Not proven correct on physical 3DS |
| Original Section | Extracted `src/sysGCU/section.cpp` lifecycle methods execute with diagnostic callbacks in emulator | Not original Boot/SingleGameSection |
| Audio | Original THP decoder port passes host vector tests; silent fallback in emulator | No audible-output validation |
| Assets/gameplay | No real game dump available or verified | No original levels/models/AI/gameplay running |

Using the user's stage definitions: selected functions have reached stage 3 in
an emulator, not full-engine stage 4 and not hardware verification. Full-game
compilation probes are stage 1 only. Asset stages 5–8 have not been reached.

Two tiny `.iso` fixtures under `/workspace/tools/windows input` are synthetic
parser tests, not game dumps. No actual user ISO/RVZ/CISO is accessible. None is
needed for Phase 0. Do not publish game assets or interpret those fixtures as a
supported game revision.

## Next single milestone

User confirms HW01 triangle/checkerboard/spheres on real New 3DS XL, with log and
screenshots. Then verify original C_MTXLookAt against fixed expected vectors on
hardware, with a call/result counter, before widening original-engine integration.
