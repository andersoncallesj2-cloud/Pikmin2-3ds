# Pikmin 2 build tools for Windows — no Docker required

[**Download the native Windows x64 ZIP**](https://github.com/andersoncallesj2-cloud/Pikmin2-3ds/raw/refs/heads/main/downloads/pikmin2-builder-windows-native.zip)

Extract the entire ZIP to a writable folder and run `pikmin2-builder.exe`.
Python and native Windows tools are bundled. **No Docker, WSL or separate Python installation required.** Replace the whole old Docker-based bundle with this version.

The bundle includes all 2,151 files from your supplied `pikmin2-main.zip` unchanged, plus builder source.

- **YES — GameCube source build:** runs the uploaded decompilation's configure.py and Ninja natively on Windows, producing a GameCube `.dol`. Requires a matching retail ROM and Internet access to download the source project's configured compilers/tools on first use. A complete real-ROM build remains unverified.
- **NO — 3DS banner preview:** packages your ROM's banner into a `.cia` using bundled native Windows makerom and a precompiled ARM viewer. No compiler setup or Docker is needed. This does not compile the decompilation's game engine for 3DS.

**This is not a playable Pikmin 2 3DS port.** The game engine still needs substantial porting.

The updated Windows EXE completed a native preview-CIA build under Wine, with no Docker invoked by the application. CIA content hashes and thirteen synthetic-data tests pass. Physical Windows/3DS execution remains untested. The supplied configure.py and Windows Ninja also start using the bundled runtime.

See `WINDOWS.md` inside the ZIP for details. No game ROM is included.
