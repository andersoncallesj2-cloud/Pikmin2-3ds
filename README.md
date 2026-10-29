# Pikmin 2 original-engine port: first boot milestone

**[Download the working CIA](https://github.com/andersoncallesj2-cloud/Pikmin2-3ds/raw/refs/heads/main/downloads/pikmin2-port.cia)** · **[Download the Homebrew Launcher 3DSX](https://github.com/andersoncallesj2-cloud/Pikmin2-3ds/raw/refs/heads/main/downloads/pikmin2-port.3dsx)**

This first milestone boots the original Section lifecycle, original debug graphics and camera/game math on 3DS, with a ported THP movie-audio decoder. **It is not playable Pikmin 2 yet.** Original BootSection, title, levels, Pikmin AI and JAudio are not linked. No replacement gameplay/AI engine is used by this build.

## Tutorial: launch the tested build

1. Download `pikmin2-port.3dsx` above.
2. On your homebrew-capable 3DS SD card, create `/3ds/pikmin2-port/`.
3. Copy the 3DSX into that folder, reinsert the card, and open Homebrew Launcher.
4. Launch **Pikmin 2 Port Lab**. Alternatively, download/install the CIA using your existing homebrew CIA installer.
5. Use Circle Pad/D-pad to move the test camera, L/R to orbit, Y to reset, B to toggle the outer debug spheres, A to request the audio-decoder test, and START to exit.

No ROM, Docker, Windows compiler or original game assets are needed for this diagnostic. The checkerboard is generated; spheres execute the original graphics methods. Missing DSP firmware disables sound without preventing launch. Physical 3DS behavior and audible playback remain unverified.

## What works and what is next

Azahar 2126.2 tests pass: 3DSX boot, CIA install/launch, original main loop, camera/toggle/reset, zero GX adapter errors and clean exit. Original-source checks, math/graphics/decoder tests and six lifecycle scenarios pass, including address/undefined-behavior sanitizers. See [the source and detailed status](port3ds/README.md).

Five complete original engine bootstrap translation units also compile to ARM objects, but the full engine does not link yet. Endian/ABI bridging, real JKR/DVD/ARAM services, J3D resources and original gameplay sections are the next gates.

![Installed original-section CIA in emulator](port3ds/validation/engine-cia-start.png)

## Windows tools

[Native Windows bundle](https://github.com/andersoncallesj2-cloud/Pikmin2-3ds/raw/refs/heads/main/downloads/pikmin2-builder-windows-native.zip). No Docker required. An updated tutorial GUI with an obvious `output` folder is being added.

The bundle preserves all 2,151 uploaded original source files. [Original source license](port3ds/LICENSE.source.md). No game ROM or Nintendo assets are included in the diagnostic.
