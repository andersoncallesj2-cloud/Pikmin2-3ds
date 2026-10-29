# Pikmin 2 build tools for Windows

[**Download the Windows x64 ZIP**](https://github.com/andersoncallesj2-cloud/Pikmin2-3ds/raw/refs/heads/main/downloads/pikmin2-builder-windows-x64.zip)

Extract the entire ZIP, then run `pikmin2-builder.exe`. Python is bundled. Builds require Docker Desktop running with Linux containers.

The bundle includes all 2,151 files from the supplied `pikmin2-main.zip` unchanged, plus source for the builder.

- **GameCube source build:** compiles the supplied decompilation to a GameCube `.dol`. Requires a matching retail ROM. Full real-ROM compilation has not been verified.
- **3DS preview build:** builds a banner-viewer `.cia`, without gameplay.

**This is not a playable Pikmin 2 3DS port.** The preview does not compile the game engine for 3DS. That engine still requires substantial porting.

The Windows launcher was smoke-tested under Wine. Thirteen synthetic-data tests and the container preview build pass. Physical Windows, Docker Desktop integration, and 3DS hardware execution remain untested.

See `WINDOWS.md` inside the ZIP for usage and limitations. No game ROM is included.
