# Original retail Section lifecycle on the 3DS platform backend

This module compiles the actual uploaded `src/sysGCU/section.cpp` bodies for:

- `Section::run`, `loading`, `fadeIn`, `main`, `fadeOut`;
- `init`, `beginFrame`, `endFrame`, `beginRender`, `endRender`, `update`, `draw`.

The original lifecycle sequences loading, fade-in, its draw-before-update main
loop, and fade-out. `original_section.cpp` is generated from verbatim original
method bodies and wrapped in namespace `P2Engine` to avoid collisions with the
separately ported original `Graphics` class. `provenance.json` records the source
file and each extracted method's SHA-256 hash.

The narrow C++ declarations in `engine_compat.h` and the `PlatformSection`
implementation are explicit portability code. They replace only the physical
system/display/service endpoints and concrete content callbacks. They do not
preserve the original object-memory ABI or pretend to implement the complete
GameCube SDK. Original JKR heap construction/destruction and `Section::exit`
are not compiled: platform resources are owned by the 3DS application.

The platform callbacks provide graphics/audio work; the uploaded original
lifecycle decides when to call them. The original `mTimeStep` defaults can be
set to zero for bring-up without fabricated fade visuals. Fades still call the
original initialization stages and optional fader callbacks. Source-specific
US demo paths remain excluded; this is the retail lifecycle.

## Integration

Compile `engine/original_section.cpp` and `engine/port_engine.cpp` as C++11,
`-Iinclude -Iengine`, with the normal `-fno-exceptions -fno-rtti` if desired.
No libctru dependency is required in these two files. The public interface is
the C/C++ header `include/port_engine.h`.

Zero-initialize `PortEngineCallbacks`, set `context`, and provide `begin_frame`,
`begin_render`, `draw`, `update`, `end_render`, `end_frame` as appropriate.
Call `port_engine_run(&callbacks, 0.0f, 0, &stats)` for an unfaded loop without
an artificial frame budget. `update` returning false ends the original main
loop normally; `begin_frame` returning false requests platform exit and
suppresses subsequent draw/update callbacks. Original unwind stages still run;
end-render/end-frame callbacks remain paired with their opened stages.

For automated tests a positive `max_frames` bounds stalled loading/main/finish
callbacks. Reentrant runs and invalid fade durations are rejected.

## Service limits and what this does not run

No GameCube DVD, memory card, wipe object, heap, or audio-sequence manager is
mounted. Its initial backend reports no queued DVD I/O, does not implement
profiling timers, and delegates optional audio stop/fade events through callbacks.
These absent service endpoints are explicitly scaffolding. They must be replaced
before an original asset-driven content section can use them.

This executes original **engine lifecycle scheduling**. It does not instantiate
original `BootSection`, `RootMenuSection`, `Title::Section`, or
`Game::SingleGameSection`, and it does not supply game assets or entity/AI logic.
Original `GameFlow::run` is not used because it is an unconditional infinite
heap-backed factory for those currently unported sections.

## Verification

```sh
g++ -std=c++11 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
  -Iinclude -Iengine engine/original_section.cpp engine/port_engine.cpp \
  tests/engine_test.cpp -o /tmp/engine_test
/tmp/engine_test
python3 engine/extract_upstream.py --check
```

Six host scenarios verify original callback ordering, fade phases, bounded main
and loading loops, platform exit and reentrancy rejection. Host execution does
not establish hardware behavior or successful original gameplay startup.
