# Original engine bootstrap compilation milestone

This target compiles **five complete, original Pikmin 2 C++ translation units**
for ARM, without substituting game logic or compiling a rewritten simulation:

- `src/sysBootupU/sysBootup.cpp`
- `src/sysGCU/appThread.cpp`
- `src/sysGCU/gameflow.cpp`
- `src/sysGCU/section.cpp`
- `src/sysGCU/system.cpp`

The `.cpp` files and original source headers remain unchanged. A generated,
separate include overlay makes legacy Metrowerks-era C++ parse with GCC.
`prepare_overlay.py` records the original header hashes, every adapted header,
its before/after hash and the reason for its change. `run_probe.py` additionally
checks that the five source files stayed unchanged and records all commands.

## Run

Use Python 3.9+ and devkitARM. The scripts do not download dependencies or require
Docker. From the repository root, for example:

```sh
SOURCE_DIR="$PWD/decomp/pikmin2-main" \
OUTPUT_DIR="$PWD/port3ds/build/original-engine-objects" \
DEVKITARM=/opt/devkitpro/devkitARM \
  sh port3ds/bootstrap/probe.sh
```

`COMPILER=/absolute/path/to/arm-none-eabi-g++` overrides DEVKITARM. With neither
set, the compiler is looked up on PATH. `PYTHON_CMD` can select another Python.
For Windows users, the Python entry point also runs directly (the generated
include casing aliases currently require symlink support):

```powershell
python port3ds/bootstrap/run_probe.py --source decomp/pikmin2-main `
  --output port3ds/build/original-engine-objects `
  --compiler C:\devkitPro\devkitARM\bin\arm-none-eabi-g++.exe
```

The tested compiler was devkitARM GCC 16.1.0. The tested flags select the 3DS's
ARMv6K CPU, hard-float ABI, 32-bit enums, standard C++17 parsing, function/data
sections, no exceptions or RTTI, and `-fpermissive` for remaining legacy warnings.
Every warning is preserved in the per-unit logs; warnings are not presented as
proven runtime compatibility.

Outputs include:

- `report.json`: all five compile outcomes and original source checksums.
- `overlay-manifest.json`: generated header adaptations and original hashes.
- `<unit>.o` and `<unit>.log`: current-run objects and complete compiler output.
- `unresolved-symbols.txt` / `symbols-demangled.txt`: real dependency inventory.
- `relocatable-link.log`: actual partial-link diagnostics, without link stubs.

A successful script exit means **five objects compiled**. It does not mean the
engine linked, launched, loaded assets or became playable. The partial-link
probe is deliberately reported separately and may fail. No undefined symbols
are replaced with no-op implementations.

## Original entry and required next work

The original entry calls `OSReport`, `System::initialize`, the original `System`
constructor, and `System::run`. `System::run` enters `GameFlow::run`, which creates
section heaps and executes the original `Section::init/run/exit` lifecycle.
BootSection/RootMenuSection initialization then pulls in actual resources,
JKernel/JFW/JUT services, GX rendering, JAudio, DVD access and thread scheduling.
These need real implementations, not merely declarations or successful syntax.

The first partial link reveals duplicate OS globals emitted by original headers;
those require one canonical definition and extern declarations in the port
header overlay. The unresolved inventory includes real original engine and
platform dependencies. The generated register-address declaration `Pad3Button`
and intrinsic signatures likewise require actual ARM platform adapters.

**Endianness is unresolved for the whole engine.** These test objects use the
ordinary little-endian 3DS ABI. For example, `GameFlow` initializes SectionInfo's
union as a big-endian-style integer but reads `mSectionId[0]`: that interpretation
changes on little-endian ARM even before disc-resource parsing. A real engine
port must systematically adapt such layouts, or implement a documented BE8
engine/native bridge. This milestone does not claim to solve that issue.

Melee's port illustrates the latter strategy: original engine C compiled with
Clang `--target=armeb-none-eabi`, custom BE8 ELF/relocation handling, memory/libc
bridges and native little-endian 3DS services. It cannot be reproduced by simply
adding `-mbig-endian` to an ordinary 3DS build. Its implementation was studied,
not copied. Pikmin 2's C++ ABI and object layouts introduce additional checks.
