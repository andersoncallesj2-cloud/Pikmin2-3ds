# Build and private game-data workflow

## Current hardware diagnostic (no game data)

With devkitPro devkitARM, libctru and Citro3D activated:

```sh
cd port3ds/hwdiag
make -j2
make cia MAKEROM=/path/to/makerom
```

Outputs are `port3ds/hwdiag/pikmin2-hwdiag.3dsx` and `.cia`. These are distinct
homebrew formats, with distinct launch/install steps in TESTING.md. The CIA is
an unsigned homebrew title, not an official Nintendo-signed game. No Windows
packager setup or game data is needed to run the downloadable diagnostic.

The original viewer source/binaries are retained. Do not overwrite them when
preparing a hardware test. Developer builds may use the existing pinned SDK image
from port3ds/README.md, but Docker is not required to run the resulting application.

## Planned actual-game workflow — not yet implemented

The final public distribution will contain code/tools, never a ROM, extracted
archives, models, textures, game sounds or DSP firmware. A user supplies a local
private dump. Current scripts are not a full-game 3DS compiler.

Design the stages now, implement them only after the hardware gate:

1. Open the input read-only; inspect magic/header instead of trusting its extension.
2. ISO/GCM: bounded GameCube filesystem reads. RVZ: invoke an installed/pinned
   DolphinTool conversion locally in a fresh temporary directory. CISO: distinguish
   GameCube/Wii sparse CISO from unrelated compressed formats (including PSP CSO);
   use a converter proven compatible with that variant, or fail with a precise
   unsupported-variant message. Do not assume a .ciso suffix proves compatibility.
3. Check game ID, region/revision, filesystem bounds, required file set and known
   executable checksum before extraction. Initially support only an actually
   tested revision; no revision has yet been tested with real data here.
4. Extract locally to a private staging directory with safe paths, size limits,
   no input mutation and no blind overwrite. Record input identity/tool versions
   and resulting checksums. Convert only formats the 3DS backend cannot read.
5. Keep small application resources in RomFS; prefer measured, bounded streaming
   of larger original resources from an external SD directory. Neither the whole
   ISO nor all resources belong in RAM. Validate manifest/version at runtime.
6. Compile original source + platform adaptation and package CIA and/or 3DSX.
   Proposed output layout: `output/<build-id>/app/`, `sd/3ds/pikmin2/data/`,
   `manifest.json`, and installation instructions. These local outputs contain
   private data and are not committed or uploaded.
7. Fail explicitly when conversion, checksum validation or resource preparation
   fails. Clean only owned temporary files; preserve source dumps and user data.

Open Nectar's prepared-image owner and Melee's revision-checked local extraction
are references for this approach. Their workflows do not establish our RVZ/CISO
support. **Current extractors accept only uncompressed retail ISO/GCM for their
limited banner/DOL tasks; RVZ/CISO input support remains planned, not verified.**

No real game dump is currently accessible. The synthetic .iso parser fixtures
are not Pikmin 2 development assets. Until the user can provide a dump privately,
continue hardware diagnostics and source auditing without claiming asset tests.
