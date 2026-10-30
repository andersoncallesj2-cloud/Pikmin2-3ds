# Building the native resource prototype

The new builder implements a small, genuine data path: read a local supported
ISO → extract `opening.bnr` → decode its image → compile the native ARM11 runtime
with that image in RomFS → package CIA and/or 3DSX. It is not a full-game compiler.
No real Pikmin 2 dump is currently accessible here. Real-disc compatibility and
physical New 3DS rendering remain unverified.

## Tutorial: build with your own ISO

1. Clone this repository and open its root directory in a development terminal.
2. Install/activate [devkitPro](https://devkitpro.org/wiki/Getting_Started) with
   devkitARM and the `3ds-dev` libraries/tools (libctru, Citro3D, picasso,
   bannertool and 3dsxtool). Install Python **3.9 or newer** and GNU Make.
   `DEVKITPRO`, `DEVKITARM` and the SDK tools on `PATH` must point to that install.
3. Install [Project_CTR makerom](https://github.com/3DSGuy/Project_CTR) and put it
   on `PATH`, or supply its full path with `--makerom`. Tested here with makerom
   0.19 and devkitARM GCC 16.1.0. Match the packager to your host OS.
4. Keep your dumped ISO outside the checkout. Initially the validator permits
   **USA GPVE01, disc 0, revision 0**, with the `main.dol` SHA-1 from the supplied
   projectPiki `config/GPVE01/config.yml`:
   `90d328bf8f190c90472e8c19e7e53c6ad0fe0d1a`.
   This is a configured allowlist, not a claim of real-disc testing. Other IDs,
   revisions, modified executables, malformed filesystems and compressed images
   fail explicitly. Do not change checksums merely to force a build to pass.
5. Run:

   ```sh
   python tools/build.py --iso "/path/to/Pikmin2.iso" --target cia
   ```

   For both formats, a specific packager, and an easily located workspace:

   ```sh
   python tools/build.py --iso "/path/to/Pikmin2.iso" --target both --work-dir build/my-private-test --makerom /path/to/makerom
   ```

6. Read the absolute **OUTPUT** paths printed on success. In the second example:

   ```text
   build/my-private-test/
   ├── extracted/opening.bnr
   ├── converted/banner.p2b
   ├── runtime/                 # staged source, RomFS and compiler intermediates
   ├── logs/compile.log
   ├── logs/package.log
   └── output/
       ├── Pikmin2-3DS.cia
       ├── Pikmin2-3DS.3dsx     # only with --target both or 3dsx
       ├── build-report.txt
       └── build-report.json
   ```

   Without `--work-dir`, a fresh `build/iso-prototype-<suffix>/` directory is
   created. A chosen workspace must be empty or nonexistent; previous files are
   never overwritten. **Do not put spaces in the workspace/repository path**:
   the devkitPro Makefile template cannot handle them. ISO paths may have spaces.
7. Follow [the hardware test procedure](TESTING.md#iso-resource-prototype).
   A generated package alone is not evidence that it boots on real hardware.

On **Windows**, use the devkitPro MSYS2 development terminal with native Python,
Make and `makerom.exe`. No Docker is required. The new pipeline has been compiled
on Linux here; its complete Windows workflow has not yet been tested. The older
downloadable Windows builder EXE does **not** include `tools/build.py`.

## What the stages actually do

- **ISO validation:** open read-only, verify GameCube magic, game ID/disc/revision,
  bounded DOL extent/checksum, complete FST node/name/directory/file bounds, and
  presence/size of root `opening.bnr`. This is selective validation, not a full
  disc hash or validation of every game archive.
- **Extraction:** copy the actual banner bytes into this build's private folder;
  record size and SHA-256. No game data or tools are downloaded by the builder.
- **Conversion:** decode big-endian 4×4 RGB5A3 tiles into 96×32 row-major RGB8;
  composite alpha on black, preserve dimensions and add a versioned CRC header.
  This is an image resource conversion, not J3D/model conversion.
- **Compilation:** stage the existing selected original source and native
  backend, include only this build's resource, invoke Make, check ARM ELF and
  3DSX formats and embedded resource bytes. Record compiler/source/output hashes.
- **Packaging:** invoke makerom with a separate homebrew title ID
  `000400000F2A7500`, check NCCH bounds/ExeFS and RomFS header hashes and embedded
  asset bytes. This makes an unsigned homebrew CIA, not an official signed title.

Only completed stages receive PASS. A failure returns a nonzero exit code and
records the failing stage; compiler/packager errors include a log path and tail.
Never use a partial output from a failed build. Missing toolchains and unsupported
dumps fail instead of silently generating a generic viewer.

The app loads and checks this resource at runtime, shows its CRC/source hash,
and uses Citro3D to render a pixel card in 3D. [Runtime details and controls](../port3ds/iso_prototype/README.md).
No original character, level, JKR/J3D loader, gameplay objects or audio have been
added. The small resource needs 9 KiB of pixel storage; the full ISO is never
copied to RomFS or loaded into 3DS RAM.

## Privacy and future dump formats

`build/` is ignored, and each workspace receives its own ignore file. Real ISO
build outputs contain copyrighted game data, including the CIA and 3DSX: keep
them local/private. Never force-add them or publish them as project downloads.
The public smoke-build binaries contain only explicitly labeled synthetic data.

**RVZ and CISO support is planned, not implemented by this CLI.** Convert a private
RVZ to a separate ISO locally with a compatible installed DolphinTool before
building. Preserve the input; do not rename a compressed file to `.iso`. A future
adapter will inspect magic, create an owned temporary conversion workspace and
record converter versions/checksums. CISO variants must be identified explicitly;
GameCube/Wii sparse CISO and PSP CSO are not interchangeable. No variant has been
tested here yet.

Larger assets should be selectively extracted and streamed from SD after actual
memory/load-time measurements. RomFS suits this tiny first resource; it is not a
decision to embed the whole game. Open Nectar's prepared-image lifetime owner
and Melee's strict revision-checked local extraction remain architectural
references; no GPL Melee implementation was copied into this builder.

## Reproduce the non-game tests

Host tests require no SDK and use synthetic data:

```sh
PYTHONPATH=tools:tests python3 -m unittest discover -s tests -v
```

With the SDK active, run two actual compilations/packages using generated data:

```sh
python tests/run_iso_fixture.py --work-dir /tmp/pikmin-synthetic-test --makerom /path/to/makerom
```

The destination must not exist. This runner injects a synthetic revision table
through a test API; the production CLI rejects the fixture and has no skip flag.
It checks input preservation and that changing the source banner changes both
CIA content and 3DSX. [Recorded evidence](../port3ds/iso_prototype/validation/README.md)
keeps real compilation, emulator behavior and unverified hardware separate.

## Existing asset-free hardware diagnostic

With the same SDK activated:

```sh
cd port3ds/hwdiag
make -j2
make cia MAKEROM=/path/to/makerom
```

Outputs: `port3ds/hwdiag/pikmin2-hwdiag.3dsx` and `.cia`. The six selectable HW01
tests require no game data. Their original source/binaries and the older viewer
are preserved. [Test instructions](TESTING.md).
