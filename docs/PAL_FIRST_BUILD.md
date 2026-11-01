# ISO-PAL01: first actual European-disc build (2026-11-01)

## Implemented changes

- Add GPVP01 disc 0/revision 0 using the independent projectPiki executable
  checksum. Do not bypass validation or accept arbitrary disc headers.
- Fix FST name decoding for Japanese development filenames in the retail ISO.
  Validate path components and table/file bounds before reading resources.
- Support Windows native Python with the devkitPro MSYS installation: resolve
  `/opt/devkitpro`, keep mktemp files in the private workspace, and precreate the
  Make build directory without traversing restricted ancestors using mkdir -p.
- Extend the ISO resource app with seven separate graphics stages, an
  identity-camera triangle, accurate `not run` status, loop/CPU/GPU measurements,
  and SD logging. Old viewer, HW01, HW02 and public binaries are preserved.
- Add `--start-test 1..7` (default safe clear). RomFS control data uses explicit
  LF bytes on all hosts. Add a separate original BMD extraction/inventory tool.

## ISO integration

Actual input: GPVP01, disc 0/revision 0, 1,459,978,240 bytes; 3,349 FST entries.
The read-only executable check matches
`cbd6d7b93536c8e84f75ed76027df740b4e1178b` from
[projectPiki GPVP01 configuration](https://github.com/projectPiki/pikmin2/blob/999140f3521e22619547d5d61862a6577ea73882/config/GPVP01/config.yml).
Only the small selected files are read into memory; the whole ISO is not loaded.

| Resource | Result |
| --- | --- |
| `opening.bnr` | 8,096-byte BNR2; extracted and converted to 96x32 RGB8/P2BN; embedded in CIA and 3DSX |
| `user/Ebisawa/testdata/title_red_piki.bmd` | 7,296-byte J3D2bmd3; extracted and structurally inspected, **not used by the runtime** |

Raw banner SHA-256:
`e8f4673b3f5fb234f5050cd8f1abab9f2af7bebddea58fc24c063aff71be8f23`.
Converted file SHA-256:
`505444d35a24d0373ec00f2eb0ab61423d9e34b87d52bae0ad0ed0547236339a`.
Runtime pixel CRC32: `5b089bc1`. CIA checks verify the exact converted bytes and
NCCH section-header hashes. No private asset, real-disc binary or screenshot was
added to this repository. Extraction is local; the ISO is never opened for writes.

The BMD has INF1/VTX1/EVP1/DRW1/JNT1/SHP1/MAT3/TEX1 blocks, 3 shapes, 3 joints,
3 materials, 5 texture entries and no envelope records. Geometry display lists,
joint transforms, material/texture data and offsets need further validation and
conversion. A title resource is not the gameplay Pikmin character or Olimar.

## Original game code

No new gameplay functions were imported. Existing extracted original
`Section::run/loading/fadeIn/main/fadeOut`, `C_MTXLookAt`, and diagnostic
`Graphics::drawSphere/drawLine` remain behind their documented narrow adapters.
The identity-view triangle isolates rendering from the original camera math.
The new parser, builder changes and diagnostic controls are new port code.
Displaying the banner does not mean the full original engine is running.

## Native engine status

Native libctru startup, RomFS reads, CRC validation, bottom-screen diagnostics,
Citro3D initialization, clear-colour transfer and the actual banner render in
Azahar. The user confirmed the mapped A key (J) selects the banner; a subsequent
window capture showed the Pikmin 2 image, test 7, 18,432 vertices, CRC `5b089bc1`,
successful draw checks and no queue-busy warning. Other stages, camera/toggles
and clean START exit still require confirmation for this build.
Rendering uses constant-colour triangles, not a GPU texture upload; the banner
stage submits 18,432 vertices. This is a small resource probe, not a scalable
texture/model renderer or a performance benchmark.

## Build status

Actual GPVP01 extraction, conversion, ARM11 compile/link, 3DSX generation and CIA
packaging passed on Windows using devkitARM GCC 16.1.0, installed libctru/Citro3D
and the makerom already distributed in the project's Windows kit. Reports record
UTC build time, source hashes, compiler identity, chosen startup test and output
hashes. The private binaries are deliberately excluded from GitHub.

17 disc/build/RVZ/BMD tests and 4 OBJ converter tests passed. Synthetic PAL BNR2
coverage includes Japanese filenames, selective resource extraction, and
rejection by the production executable checksum. Existing corrupt/unsupported
disc and changed-asset tests pass. The separate existing host C mesh-loader test
could not execute because this Windows environment has no host `cc`; this is not
a 22/22 passing full-suite claim. Actual ARM compilation succeeded without warnings.

## Emulator testing

Azahar 2126.2 on this PC booted the actual-disc 3DSX in clear-colour mode. It
loaded the real banner, displayed the expected CRC and raw SHA prefix, advanced
the heartbeat and reported successful initialization. Controller keystrokes
injected by the desktop automation did not reliably reach the emulated HID path.
An explicit keyboard profile was saved on this PC, preserving the old profile.
The user pressed J and confirmed the real banner appeared. A following capture
independently showed the genuine banner and matching source/CRC overlay.
See [keyboard mappings](AZAHAR_KEYBOARD.md). No claim is made that the other
graphics stages or START exit passed.
The CIA was structurally checked but not installed/launched in this session.
Historical synthetic CIA emulator evidence remains separate.

## Real hardware testing

**Pending. The reported blank-scene/freeze bug is not proven fixed.**
Launch the default-start 3DSX or install the private CIA. Verify magenta clear and
an advancing bottom heartbeat. Step with D-pad Right, waiting five seconds per
stage: triangle, cube, checkerboard, spheres, full debug scene, ISO banner.
X toggles fixed/interleaved colour, Y depth; L/R orbit, B reset; A banner/clear;
START exits. The banner should show the disc's Pikmin 2 image with CRC `5b089bc1`.
API PASS is not proof of visible pixels. Stop at a freeze; report the first failing
stage, launch format, both screens, and
`/3ds/pikmin2-iso-prototype/hardware.log`.

## Known problems and next development goal

The hardware gate remains open. J3D conversion, GPU texture/material handling,
characters, gameplay, collision, animation and full audio are not implemented.
First obtain the first failing physical graphics stage. Once basic geometry is
confirmed, implement bounded VTX1/SHP1 display-list decoding and JNT1/DRW1
transforms for the extracted small title model, with explicit material limitations.

## Upstream reuse check

Rechecked Open Nectar at `a0985ba968d9e6078ccb50dd79e8865d289d1c8c`:
`games/pikmin2/pc_port/launcher/gamecube_image.cpp` has FST extraction, safe
Windows filename handling, and full-disc integrity verification. Its current
known-disc table uses **GPIE01/GPIP01 (Pikmin)**, not this **GPVP01** Pikmin 2
executable. Its full install workflow extracts broadly rather than selecting
one small BNR/BMD, so it was not imported or treated as GPVP01 validation.
`prepared_image.h` retains the private conversion-directory model already used
by this project's RVZ code. No new upstream source was copied; existing source
licenses and provenance remain intact. Original J3D structures were inspected
in projectPiki `include/JSystem/J3D/J3DFileBlock.h` and its loader interfaces.
The previous MKW/Melee/native-SDK architecture audit is retained in
[ENGINE_INTEGRATION.md](ENGINE_INTEGRATION.md); no major engine redesign occurred.
