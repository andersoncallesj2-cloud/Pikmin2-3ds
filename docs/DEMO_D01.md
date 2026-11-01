# D01 playable increment — 1 November 2026

Onion update: the delivery point now displays the genuine textured `goal.bmd`
from the ISO's `user/Kando/onyon/arc.szs` (1,246 triangles, two textures), at
0.65 model scale. It is a static visual landmark; spawning/storage and Onion
animations are not implemented. The updated CIA/3DSX compile and contain the
converted Onion; this addition has not yet been checked in an emulator or on hardware.

Build with Python 3.10+, installed devkitPro devkitARM/libctru/Citro3D and makerom:

```powershell
python tools/build_demo.py --iso "Y:\Downloads\Pikmin 2 (Europe) (En,Fr,De,Es,It).iso" --makerom "C:\path\to\makerom.exe"
```

The command prints its private output directory. It validates the known disc
executable, extracts Yaz0/RARC resources in memory, converts rigid BMD models and
textures, reads disc gameplay parameters, compiles ARM code and packages both
`Pikmin2-Demo-D01.cia` and `Pikmin2-Demo-D01.3dsx`. Build paths must have no spaces.
The output contains private disc assets: do not upload the binaries or RomFS to
the public repository. Only tools, runtime source and synthetic tests belong there.

## Playing

Install the CIA with the same homebrew installation method used for HW03, or
launch the 3DSX through Homebrew Launcher. This has a separate title ID from HW03
and the banner diagnostic. No extra asset folder needs copying.

| Function | 3DS | Saved Azahar keyboard profile |
|---|---|---|
| Move captain | Circle Pad | WASD |
| Rotate camera | L / R | Q / E |
| Move aim direction/range | D-pad or C-stick | Arrows, or F/H and T/G |
| Throw one following Pikmin | A | J |
| Whistle around cursor | Hold B | Hold K |
| Dismiss following squad | X | U |
| Reset arena | Y | I |
| Toggle automated showcase and reset | SELECT | Backspace |
| Exit | START | Enter |

At the starting position, press A three times, about a second apart. The Pikmin
land near the gold crate. Three carriers move it to the blue delivery ring.
Move, adjust aim and hold B to recall idle/carrier Pikmin within the expanding
cursor circle. Whistling removes carriers and can stop the crate. Reset with Y.
The grey block is solid. Edges clamp movement; there is no falling/death.

## ISO assets

European GPVP01 disc 0, revision 0, known DOL SHA1
`cbd6d7b93536c8e84f75ed76027df740b4e1178b` passed actual extraction.
`user/Kando/piki/pikis.szs` supplies `orima_model/orima1.bmd`,
`piki_model/piki_p2_red.bmd` and `happa_model/leaf.bmd`.
Olimar has 2,210 triangles, red Pikmin 282, leaf 4; shared immutable model VBOs
are drawn with separate object transforms. Texture encodings used here include
IA8, RGB565, I4 and CMPR. Native files carry bounds, count and CRC checks.

## Original code and adaptations

Reference source: projectPiki/pikmin2 commit
`999140f3521e22619547d5d61862a6577ea73882` (CC0).

| Classification | In this demo |
|---|---|
| Original C bodies compiled | `C_MTXLookAt`, `roundAng`, `angDist` in existing `game_math/original_math.c` |
| Adapted equations | `Navi::throwPiki` red throw vertical speed/flight estimate and captain momentum; flat-floor `FakePiki::moveVelocity` response and `moveRotation` turn response; `Piki::getSpeed` leaf interpolation |
| Disc data | Captain speed 160, leaf run 120, walk 10, gravity 560, red height 72.5, landing-time parameter 1, whistle radius 5..90/growth time .45 |
| New native implementation | Renderer, archive/model converter, fixed-step controller, simple formation, four Pikmin states, box/edge collision, crate transport/delivery, controls/HUD |

Gameplay references are `src/plugProjectKandoU/{navi,fakePiki,piki}.cpp`.
Quantized movement is reduced; original stick correction/cursor-only walking
branch is not fully ported. Whistle uses original parameters with a simplified
linear expansion/eligibility check. Following is not original CPlate/ActFormation.
Carrying is a new demonstrator, not the original treasure/carry AI. The existing
source-extract license and provenance files remain applicable to compiled bodies.

## Native engine and build status

libctru input, RomFS, SD diagnostic log, APT exit and fixed 60 Hz simulation work.
Citro3D draws static float4 position/colour/UV buffers with model-view-projection
uniforms. Texture data uses native Morton tiling. Queue polling and exit drain
are bounded, following HW03; the failed old per-frame vertex rewrite is not used.
Real ARM ELF, 3DSX embedded model and CIA NCCH section hash checks pass.

## Validation

Host C checks pass: throw apex/landing, whistle recall, dismiss, world/block
collision, carry threshold/delivery and the 30-second input tour. Compiled with
Zig 0.13 `cc`, C11, warnings treated as errors. The 29 Python checks pass, including
the existing native mesh loader test (using Zig as the missing host `cc`), bounded
Yaz0 failures/overlap, truncated archive/model rejection, texture decoding and tiling.
Actual disc BMD conversion is checked in the build, not replaced by a fixture.

Azahar launches the real-disc 3DSX, displays the textured arena and disc character
geometry and keeps rendering without a queue timeout. Input automation is too
brief for reliable game key holds; the deterministic host controller tests cover
the action transitions. No claim that this provides a full CIA hardware test.

Hardware: HW03 is confirmed working by the user's latest report. D01 remains
unverified on New 3DS XL. Check models/colours, movement, three throws, delivery,
whistle, camera and START exit. Report the first failing action and photograph
the HUD. Its SD log is `/3ds/pikmin2-demo/demo.log`.

## Known limitations and next goal

Characters have static bind poses. Leaf placement and lighting are approximate;
materials use base colour/first texture rather than complete GX TEV, so the
helmet/reflections can look wrong. No skeletal animation, full original AI,
enemies, attacks, sound, saves, original maps or complete game loop.

Next concrete step: load the original BCK walk animation and drive the existing
joint hierarchy while preserving the hardware-tested static render path.
