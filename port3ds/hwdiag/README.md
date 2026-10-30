# HW01 — isolated hardware-rendering diagnostic

Build/run instructions and expected screens: [TESTING.md](../../../docs/TESTING.md).

This is an independent target. It does not change the original viewer's source
or binaries, run its Section lifecycle, initialize NDSP, load ROM resources or
add gameplay systems. The original camera/debug methods are reused for tests
3–6 through source includes, without changing their bodies. Tests 1–2 do not
invoke those methods, so basic GPU setup is isolated from original-engine code.

Architecture: original debug sphere code emits position/RGBA line vertices;
scalar camera/model transforms produce eye-space vertices; this small backend
turns lines into triangles and submits a linear-memory VBO to PICA200/Citro3D.
The shader, framebuffer format, 240x400 target, transfer configuration, right-
handed tilted perspective and depth conventions were compared with official
simple_tri, multiple_buf and textured_cube examples. The baseline has no textures.

Diagnostic differences from the baseline:

- Boot at bright clear colour; independently selectable geometry stages.
- Bind the shader before uniform setup; check parse/DVLE, program binding result,
  uniform lookup, target/VBO allocation, attribute registration, BufInfo_Add,
  cache-flush Result, FrameBegin and FrameDrawOn.
- 128-byte-aligned linear allocation with size/stride/physical address displayed;
  compile-time layout assertions (28-byte stride, colour at byte 12).
- Fixed-colour path matching the official minimal example, switchable to the
  viewer's two-loader interleaved colour path. Independently toggle depth.
- Explicit cull/alpha/blend/stencil/early-depth/depth-map state.
- Nonblocking queue polling; do not write VBO data unless the previous queue has
  completed. Report a measured >2s missing completion, not an invented GPU code.
- Console cache flush independent of GPU progress, visible CPU heartbeat and
  best-effort SD logging. CPU sleeps pace polling rather than blocking in GSP.
- Avoid blocking cleanup on an observed busy GPU; process teardown may still be
  unable to recover a hardware/driver fault. Not tested on a hung physical console.

These changes are diagnostic/hardening measures, **not a proven hardware fix**.
The exact first hardware failure and offending command are not yet known.

`validation/runtime-report.json` pins the tested files. The six stages and exit
work in Azahar; on-device acceptance is pending. `hwdiag-package.txt` records
successful content/ExeFS/section hashes and expected homebrew signature failures.
