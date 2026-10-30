# Original-engine route and reference audit

No major original-engine integration proceeds until the real-hardware rendering
gate in TESTING.md passes. This audit does not certify the upstream ports or claim
an Android build can run on ARM11.

## Foundations inspected

- projectPiki/pikmin2: supplied decomp snapshot, preserved in the existing Windows
  bundle. Relevant exact file/function hashes are in port3ds provenance files.
  Its CC0 LICENSE.MD is preserved. Original entry:
  `sysBootupU/sysBootup.cpp -> System::initialize/System::run -> GameFlow::run -> Section`.
- Open Nectar `SSunnKing/Open-Nectar---Pikmin-Native-PC-Mobile-Port`, inspected
  commit `14a922a91d1f9af39504aa56a707f964e1103861`.
- `2gifts/melee-3ds` inspected snapshot `1efbf37caeed0f4bc1bc7828f9e61a12a1376004`.
- `MiniBandit/MKW-3DS` inspected snapshot `1b3ff336d3911908aaf58a758036cc91c327be5d`.
- devkitPro/citro3d `9f21cf7b380ce6f9e01a0420f19f0763e5443ca7`,
  3ds-examples `be2001fee08cf8ec7d3366e095e83f6f75da8942`, and libctru console
  implementation. The installed compiler/SDK also supplied API signatures.

## Open Nectar: useful original-engine seams

`games/pikmin2/CMakeLists.txt` separates `pikmin2_legacy` (original game/JSystem)
from native `pc_port` code and force-includes
`pikmin2-decomp-adapter/pikmin2_pc_types.h`. `pc_port/pc_main.cpp` replaces only
process entry and ultimately drives original System/GameFlow. This is the desired
long-term separation, rather than replacing gameplay with a newly written engine.

Useful files inspected:

- `pc_port/dolphin_stubs/gx_stubs.cpp`, `pc_port/gl/pc_gfx.cpp` and
  `gl/pc_gx_vertex_decode.h`: GX API/state and direct/indexed attribute interpretation.
  The GL 3.3/GLES 3.0 shader/backend cannot run on PICA200; draw decoding and
  resource semantics are candidates for adaptation, not shader source reuse.
- `dolphin_stubs/dvd_stubs.cpp`: maps original paths to extracted `assets/` files,
  uses file-position reads and a path table for FastOpen. Its async read currently
  calls the callback synchronously, and it pads short reads; these are behaviors
  to audit against original expectations, not blindly import as complete DVD support.
- `dolphin_stubs/os_stubs.cpp`: uses std::thread/mutexes/maps and contains a
  **256 MiB static arena plus 16 MiB simulated ARAM**. These sizes exceed the
  current 3DS application's available budget. Replace platform allocations and
  measure actual heap/linear/VRAM usage before adopting components.
- `audio/pc_dsp_host.cpp` and `audio/pc_audio.cpp`: software audio/DSP host and PCM
  mixing rather than translating the THP decoder into a full music engine.
  SDL output needs a bounded NDSP backend and measured mixer costs on ARM11.
- `launcher/prepared_image.h`: owns a newly created temporary conversion directory,
  converts RVZ/WIA/GCZ through Dolphin and preserves the input. Does not list CISO.

Desktop and Android share the original library; Android adds SDL JNI/activity,
GLES and touch layers. `pc_main.cpp` has AArch64/ARM-specific crash-context branches,
std::thread, chrono and 64-bit atomic heartbeat values. ARM11 atomics/locking,
thread stack sizes and runtime dependencies need auditing, not just a new compiler.
Native types replace original 64-bit-host `long` assumptions with fixed widths.
On 32-bit ARM, verify every pointer cast, size_t, structure/union layout, alignment,
paired-single replacement, and original BE resource read. The Android CMake file
also contains GPI version IDs while the Pikmin 2 root config describes GPV IDs;
verify the actual build selection before treating copied configuration as working.
No Open Nectar build or gameplay run was performed here.

Open Nectar's README declares CC0 and credits separate third-party audio/network
components; its linked root LICENSE.MD is absent in this inspected tree, while
the decomp's CC0 file exists. Check per-file/vendor scope before importing code.
No Open Nectar code or assets were copied into this project in this session.

## Comparison with the user's other references

Melee retains its original engine with a custom BE8/native boundary, GX state/FIFO
translation and software audio plus platform services. Its easy builder takes
user-provided US v1.02 ISO/GCM, validates revision, extracts locally, packages a
3DS executable and external SD files; docs require RVZ conversion first. Its
integration license is GPLv3 with separate vendor scope. We studied architecture,
not copied the implementation.

MKW uses Kinoko/recreated physics with native rendering/asset cooking. That is a
useful platform/build reference but not permission to replace Pikmin 2 gameplay.
Its original import and additions have distinct licensing scopes.

## After the hardware gate

1. Revalidate the already isolated original C_MTXLookAt and game math on hardware
   using known input/output vectors and invocation counts. Small dependency graph,
   no assets, threads or dynamic allocation; it is the first suitable subsystem.
2. Choose/test endian/ABI handling before linking larger units. Existing complete
   original System/GameFlow/Section ARM probes are not behavioral proof. Their
   unresolved link inventory remains preserved under port3ds/validation.
3. Adapt real JKR lifetime/allocation and bounded DVD/ARAM reads; no success-shaped
   dummy services. Measure before importing the desktop arena/cache design.
4. Read user's original RARC/J3D resources, then original Boot/Title/SingleGame
   sections. Add GX state by the actual call/resource dependency graph, not by
   inventing a new universal mesh API or recreating Pikmin AI.
