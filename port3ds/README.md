# Pikmin 2: original-engine bring-up on 3DS

**Hardware update:** the user reports no 3D rendering and a possible hang on New 3DS XL. This baseline is preserved, not hardware-verified. Use [HW01 diagnostics](../docs/TESTING.md) before further engine integration.


This milestone **boots original engine lifecycle code on 3DS**, with original
debug graphics, camera/game math and a ported movie-audio decoder. It is an
engineering starting point, **not yet playable Pikmin 2**: the original boot,
title, level, Pikmin/Navi AI, animation and JAudio systems are not linked.
No replacement gameplay engine or Pikmin-AI simulation is used in this build.

![Installed CIA running in Azahar](validation/engine-cia-start.png)

## Run the prebuilt milestone

- Place `pikmin2-port.3dsx` under `/3ds/pikmin2-port/` on the SD card and launch
  it through Homebrew Launcher, or install `pikmin2-port.cia` on a 3DS configured
  for homebrew. The title is **Pikmin 2 Port Lab**, ID `000400000F2A7200`.
- No ROM or original assets are required for this diagnostic. The checkerboard
  is generated test geometry; the spheres use the original graphics methods.
- Circle Pad/D-pad moves the **test camera focus**, L/R orbits, Y resets,
  B toggles the outer debug spheres, A requests the decoder audio test,
  START exits. These are diagnostic controls, not original gameplay controls.
- Missing DSP firmware disables audio without stopping the engine. On hardware,
  NDSP homebrew normally uses your own dumped firmware at `/3ds/dspfirm.cdc`.
  No firmware is distributed here. The test sound is synthetic, not game audio.
- Downloading/running these files needs no Windows compiler or Docker setup.

## What is actually original

| Component | Implemented now | Still missing |
| --- | --- | --- |
| Engine section loop | 12 original retail `Section` methods, including `run`, loading/fades, draw-before-update main loop and frame/render wrappers | Original constructor/JKR lifetime, `BootSection`, `Title::Section`, `Game::SingleGameSection`, full `System` |
| Graphics | Original `Graphics::drawSphere` and `drawLine`; replacement direct GX line path rasterized by Citro3D/PICA200 | J3D, display lists, indexed attributes, TEV, textures, lighting, actual models |
| Camera and math | Original `C_MTXLookAt`, projection functions and game distance/angle routines; scalar PowerPC vector/matrix adapters | Full camera/game classes; byte-identical paired-single behavior |
| Audio | Actual `THPAudioDecode` control flow, safe big-endian input wrapper and NDSP PCM backend | JAudio/JAS, GameCube DSP program, music/sequencing, THP streaming/video |

`engine/` and `game_math/` retain verbatim method bodies and extraction scripts;
`graphics/` records source/method hashes; `audio/original/THPAudio.c` preserves
the entire original decoder file. The original source license is included.

The narrow section/system/type declarations are **source compatibility adapters**,
not binary-layout-compatible versions of the complete original engine classes.
Only the diagnostic content callbacks are registered. Timer instrumentation,
DVD/card/sequence services are not implemented or exercised. Zero-length fades
are intentional. Do not attach original gameplay sections to these declarations
and assume their memory/resource requirements are satisfied.

## Complete original-engine compilation target

Separate from the runnable extracted lifecycle, `bootstrap/` compiles five
**complete original translation units**, leaving the supplied files unchanged:

`sysBootup.cpp`, `appThread.cpp`, `gameflow.cpp`, `section.cpp`, `system.cpp`.

```sh
SOURCE_DIR="$PWD/../decomp/pikmin2-main" \
OUTPUT_DIR="$PWD/build/original-engine-objects" \
DEVKITARM=/opt/devkitpro/devkitARM sh bootstrap/probe.sh
```

Compatibility headers are generated in the build directory with before/after
hashes. These objects **do not link into a runnable full engine yet**. The
partial link exposes duplicate original OS globals and 130 unresolved symbols;
the reports are in `validation/full-engine-*`. No success-shaped link stubs
were added. The original entry path is:

`main → System::initialize → System → System::run → GameFlow::run → Section`.

Ordinary little-endian ARM compilation also changes the meaning of original
union/data layouts (already visible in `GameFlow::SectionInfo`). The next
stage must choose and test a BE8/native bridge or systematic layout adaptation
before claiming these compiled objects are behaviorally correct.

## Build and validate the runnable milestone

With devkitPro devkitARM, libctru and Citro3D activated:

```sh
make -j2
make cia MAKEROM=/path/to/makerom
```

`assets/menu.bnr` is generated homebrew artwork/silence, not an original game
asset. It can be recreated with bannertool from the adjacent PNG/WAV. The
PICA shader is adapted from devkitPro's official simple-triangle example.
The known compiler image used here is
`devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5`;
it is an optional developer tool, not a runtime requirement.

```sh
sh tests/run_host.sh
SANITIZE=1 sh tests/run_host.sh
python3 tests/check_provenance.py --source ../decomp/pikmin2-main
```

Validation passed:

- 35 math numerical checks plus matrix/camera/angle invariants.
- Original debug sphere: all 1,024 vertices satisfy radius/color/count checks.
- 169 independent audio-decoder vector and bounds checks.
- Six original lifecycle scenarios: ordering, fades, frame limit, loading limit,
  exit and reentrancy. Host tests also pass Address/UndefinedBehavior sanitizers.
  LeakSanitizer is disabled in this executor because of its ptrace sandbox.
- Source extraction/provenance checks and ARM build.
- Azahar 2126.2: 3DSX boot; CIA install/launch; original section phase 2,
  advancing frames, camera/orbit/toggle/reset controls, zero GX errors and clean
  START exit. Screenshots/report pin exact tested binary hashes.
- CIA content/ExeFS/section hashes pass. Homebrew ticket/TMD/access-descriptor
  signatures are not Nintendo-signed and fail retail signature checks.

**Unverified:** physical 3DS behavior/performance, audible playback, real-ROM
assets and original gameplay. Emulator NDSP initialization returned `D880A7FA`
with firmware absent; the A test remains nonfatal. No audible-output claim is made.

## Next original-engine gates

1. Establish the full-engine endian/ABI strategy and fix the real link inventory.
2. Implement working JKR heaps/thread/display lifetimes and DVD/ARAM file services.
3. Load the user's actual Yaz0/RARC/J3D resources and render one original scene.
4. Bring up original `BootSection`/title and original single-game initialization;
   expand GX state and JAudio as their dependencies demand.
5. Validate actual original player/Pikmin logic and memory/performance on hardware.

Architectural references inspected: [Melee 3DS](https://github.com/2gifts/melee-3ds)
retains an original engine with a BE8/platform bridge, while
[MKW-3DS](https://github.com/MiniBandit/MKW-3DS) uses a hybrid Kinoko approach.
No code or assets from those repositories were copied. This project follows the
original-engine direction requested for Pikmin 2.
