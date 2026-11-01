# D02 hardware handoff — 1 November 2026

D02 was compiled and packaged from the European ISO. **It has not been run in
an emulator, on hardware, or through the gameplay test suite**, at the user's
request. All new runtime behaviour below is implemented but awaiting feedback.
D01's previous results do not validate this update.

## Controls

| GameCube action | New 3DS XL mapping |
|---|---|
| Main analog stick: move cursor / walk | Circle Pad; gentle tilt aims while standing, stronger tilt walks |
| A: hold Pikmin / throw | Hold A to ready a nearby follower, release A to throw; without a follower, A punches nearby Bulborb |
| B: whistle | Hold B |
| X: dismiss | X |
| Y: change captain | Y switches between Olimar and Louie |
| L: centre camera | Tap L; centres behind the cursor direction |
| Analog L + main stick: camera rotation | Hold L and move Circle Pad horizontally |
| R: camera zoom | Tap R cycles near/mid/far; holding for one second enters close view |
| Z: camera elevation | ZL toggles low/overhead |
| C-stick: direct squad | New 3DS C-stick; D-pad also provides digital swarm directions |
| START: pause | START pauses/resumes |
| Demo reset / exit | While paused: Y resets, SELECT exits |

The 3DS shoulder buttons have no analog trigger travel. Digital hold-L is an
adapter for the original partial-L action. This is not a claim of identical
hardware feel or a complete original-game control implementation. Sprays,
Pikmin-type selection, plucking, original pause menus and the full CPlate AI
are not implemented. Only red leaf Pikmin are present. Original GameCube camera
aspect (4:3) is centred on the wider top screen.

## What to try on hardware

There are twenty red Pikmin, two captains, a red Onion, the Hocotate ship, a
rubber-duck treasure and one Bulborb. Models and textures come from the ISO.

- Gently tilt Circle Pad to move the cursor toward the duck; hold/release A to
  throw Pikmin there, or swarm the squad toward it. Idle Pikmin near an item
  join its available carrier slots. **Eight** are required to lift the duck;
  up to fifteen can carry it. Its live count/weight appears above it.
- Treasure travels to the **ship**, disappears on delivery, and adds its original
  **90 Pokos** to the demo counter. It never goes to the Onion.
- Approach the Bulborb and throw Pikmin onto it. Attached attackers damage it;
  whistle to disengage. It can chase, bite, eat Pikmin and hurt the captain.
  No combat animation is implemented: its visible model uses a bind pose.
- After defeat, the corpse can be carried by **ten to twenty** Pikmin to the
  **red Onion**. It never goes to the ship or awards treasure Pokos. Delivery
  records the original carcass yield of **twelve** Pikmin in Onion storage.
  Sprout/spit-out animations and a storage menu are not implemented.
- If too many Pikmin are lost, pause with START and reset with Y.

The bottom screen shows control help, carrier counts, losses, delivery and GPU
status. The SD log remains `/3ds/pikmin2-demo/demo.log`.

## Original code reuse

Reference: CC0 `projectPiki/pikmin2` commit
`999140f3521e22619547d5d61862a6577ea73882`.

- Existing original C bodies compiled: `C_MTXLookAt`, `roundAng`, `angDist`.
- Native C adaptations of `Navi::reviseController` and `makeVelocity`: original
  ten-degree angle bins, octagonal stick correction, .1 dead zone, .85 clamp,
  .8 cursor-only threshold and cursor-facing branch, using disc parameters.
- `NaviWhistle::update`: normalized cursor motion and tangent projection at the
  cursor radius, rather than the previous separate D-pad aiming controls.
- `Navi::throwPiki`, `FakePiki` movement response/rotation and `Piki::setSpeed`
  travel limiting remain adapted for native vectors and the flat arena.
- `PlayCamera` mode selection, preset selection, weighted formation look-at,
  cursor-directed centring, angular smoothing, damped rotation, 15-sample
  elevation correction and zoom transitions are adapted into `camera.c`.
  Both captains' ground-camera parameter blocks are extracted from the ISO.
  The demo runs at 60 simulation ticks/second; frame-based interpolation factors
  are adjusted to preserve the reference 30 Hz rates.
- `Navi::makeCStick` angle/strength response and `ActFormation::exec`'s seven/
  fifteen-unit settling thresholds and six-reference-frame grace are adapted.

Formation slots, collision separation, the tiny obstacle routing graph, local
work discovery, captain ownership, combat timing and transport are **new demo
code**. The original action brain, navigation, enemy FSM and Pellet FSM are not
fully compiled into this increment. This uses original mechanics where ported;
it is not the complete original engine.

## Real ISO assets and native engine

Validated GPVP01 disc 0/revision 0; DOL SHA1
`cbd6d7b93536c8e84f75ed76027df740b4e1178b`.

| Asset | ISO source |
|---|---|
| Captains, Pikmin, leaf | `user/Kando/piki/pikis.szs` |
| Red Onion | `user/Kando/onyon/arc.szs/goal.bmd`, final `kidou_red.brk` colour key (`body` = 255,0,0,255) |
| Hocotate ship | `user/Kando/ufo/arc.szs/ufo.bmd` |
| Rubber duck | `user/Abe/Pellet/pal/ahiru.szs/ahiru.bmd` |
| Bulborb | `enemy/data/Chappy/model.szs/enemy.bmd`, with the original external `moyou_565.1.bti` and `swallow_565.1.bti` |
| HUD art | Sixteen BTIs from `new_screen/eng/res_ground.szs` |
| Camera presets | `user/Nishimura/Camera/groundCameraParms.txt` |
| Item weights/value/yield | PAL `otakara_config.txt` and `carcass_config.txt` |
| Enemy tuning | `enemy/parm/enemyParms.szs/chappy/enemyparm.txt` |

The converter now resolves weighted bind-pose envelopes for the Bulborb and
external BTIs. Non-power-of-two textures are padded for the 3DS. All models use
immutable native vertex buffers and per-object uniforms; no unsupported raw
GameCube display list is submitted to the 3DS GPU.

The HUD has original digit, slash, red-Pikmin, captain portrait/frame and Poko
art with live squad/field/total, health, money and item carrier fractions.
**This is a native demo HUD using original art, not the complete J2D/BLO UI.**
Original layouts, screen animations, day clock, sprays, radar, menus and original
UI interaction code are not fully ported. Health bars and Bulborb health display
are native demo elements.

## Build and validation status

```powershell
python tools/build_demo.py --iso "Y:\Downloads\Pikmin 2 (Europe) (En,Fr,De,Es,It).iso" --makerom "C:\path\to\makerom.exe"
```

ARM compilation, model/texture conversion and CIA/3DSX packaging succeeded.
The packaging pipeline checks native ARM ELF/3DSX identity, converted model
embedding, and CIA section hashes. The output contains private game data and
must remain outside public Git history. Public source contains no extracted
proprietary models, textures or UI images. D02 keeps D01's title ID and updates
its installed application; HW03 remains a separate known-good control.

Emulator: **not run**. Hardware: **awaiting the user's New 3DS XL test**.
Updated host gameplay assertions are provided but were **not executed**.

## Known limitations and next step

Static bind poses; no BCK character/enemy animations, sound, save system,
original maps or full original game loop. TEV shading/reflections are approximate;
the Onion's original red register is applied as native material colour. Camera
collision queries the authored floor/box rather than original map triangles.
Pikmin attack/transport, Bulborb combat and Onion storage are simplified demo
behaviour. Hardware stability and correctness are not established by compilation.

Next concrete task: incorporate the user's hardware results, then load original
BCK walk/attack animations through the converted joint hierarchy.
