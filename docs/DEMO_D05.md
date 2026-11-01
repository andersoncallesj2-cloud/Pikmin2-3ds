# Pikmin 2 - 3DS port D05

Developer shown in HOME Menu: **eabwnhm**. Native ARM11 prototype, built from
the user's European GPVP01 disc, revision 0. This remains a partial port.

## Changes in this build

- Original summer title environment, five original title Pikmin models and
  the English/Nintendo logo coordinates. L/R switches the formations. Original
  Press Start pictures and title-menu pictures use sampled original BCK
  transforms. This replaces the small disc-banner placeholder.
- Begin loads the Valley of Repose exploration prototype. Test maps is an
  additional menu choice below Bonus. Options opens native settings. The other
  original game modes explicitly report that they are not yet ported.
- Actual Valley geometry, collision triangles and water boxes. The original
  course start and angle come from `user/Abe/stages.txt`; the three Onions and
  ship use the active base-camp generators in `tutorial/defaultgen.txt`,
  including their rotations. Captain offsets follow BaseGameSection's demo
  matrix. The remaining test entities are a native overlay around base camp.
- Onion withdrawal is queued and limited by field capacity. Pikmin emerge
  through a leg, grow and climb down; returning Pikmin walk to a foot and climb
  up while shrinking. The tube endpoints come from `onyonColl.txt` transformed
  through the original model joints. The HNOBORU motion comes from the disc.
- Purple/white storage uses the ship's original `in1`/`out` model joints.
  Entry adapts the original 1.3-second wait, 110-unit suction trajectory and
  square-root shrinking. Exit velocity follows Onyon::exitPiki. Original ship
  delivery destination is 135 units along its facing direction; the ground
  glow uses the `start1` joint.
- Purple carrying strength, movement/attack tuning and impact adapter; white
  speed/poison immunity and poison damage adapter. Original model scales and
  attack parameters are extracted for all five types.
- The complete original `ActFree::exec` body is compiled through native world
  adapters. Other AI actions and the full Brain remain unfinished.
- Dwarf Bulborb central descending hits can kill in one hit. Enemies have
  dormant/sleep behavior. Delivery raises/shrinks objects and plays carrier
  celebration motions. Captain death has a bounded transition to the survivor
  or a game-over screen, replacing the blocked controller state.
- Fixed-angle upright Posy pellets; held Pikmin height follows the captain;
  original leaf-joint attachment. Ship material baking now respects texture
  wrapping and UV domains. These corrections still need visual hardware review.
- HUD moved to the bottom screen. Debug is off by default. Settings can enable
  five monitoring pages and collision rings. Pages cover frame time/FPS, heap
  and linear memory, AI states, entities, coordinates/animations, rendering
  counters, audio events and a frame-time graph.
- Native pause/settings/3DS submenu, experimental stereoscopic rendering,
  conservative draw culling and the existing 30 FPS cap. Simulation continues
  for offscreen entities; there is no gameplay culling.
- Brighter native water/hazard/glow adapters. Fixed a bottom-screen RGB565/RGB8
  mismatch exposed by the brief emulator check and removed repeated effect UVs.
- 51 selected original SE samples, captain-specific whistles/dismissal,
  throw buildup/yell, camera, footstep, enemy, hit, death, seed and storage cues.
  Two original 14-note march phrases use original pitches/rhythm with native
  sample resampling. Full dynamic JAudio sequencing is not implemented.
- Original greeting audio is embedded in the HOME banner. CSND is attempted
  if NDSP/DSP firmware is unavailable; fallback operation on Old 3DS is unverified.

## Controls

| Action | Button |
|---|---|
| Move / gentle cursor aim | Circle Pad |
| Hold / throw | Hold A / release A |
| Change held type | A + ZR; Old 3DS fallback A + R |
| Whistle | B |
| Dismiss by colour | X |
| Switch captain | Y |
| Centre / orbit camera | L / hold L + Circle Pad |
| Zoom / close view | R / hold R |
| Camera elevation | ZL |
| Swarm | C-Stick or D-pad |
| Pluck / open storage nearby | A; hold to continue plucking |
| Storage amount | Up/Down, A confirm, B cancel |
| Purple/white ship storage | Left/Right in ship menu |
| Pause | START |
| Debug page / entity | SELECT / SELECT + Left/Right |

Title: START/A opens the original menu layout; L/R selects logo formation.
Choose Test maps below Bonus to select the test arena or Valley. SELECT exits
from the title menu. Begin currently loads exploration, not the story campaign.

## Source and ISO accounting

CC0 reference: projectPiki/pikmin2 commit
`999140f3521e22619547d5d61862a6577ea73882`.
Ten unchanged original function bodies are linked: six CarryInfo/LifeGauge
functions, C_MTXLookAt, roundAng, angDist and ActFree::exec. The compatibility
types, action substitutes, world access and platform drawing/audio are native.
Movement, camera, dismissal, following, storage, seed emission and combat also
contain adaptations. This is not the full original engine or complete Pikmin AI.

New disc inputs include `user/Ebisawa/title/{title,bg_summer}.szs`,
`new_screen/eng/title.szs`, `message/font_foreign.szs`, Valley's `arc.szs` and
`texts.szs`, `user/Abe/stages.txt`, `tutorial/defaultgen.txt` and
`user/Kando/onyon/onyonColl.txt`. Gameplay models, BCA motions, UI art and
IBNK/WSYS/AFC sound resources continue to come from the user's disc. The private
build report records conversions, source hashes, placements and audio results.
Converted assets and actual-disc executables must remain outside public Git.

## Important remaining work

The title is **not identical** yet: only summer is integrated; the environment
uses its original waiting pose, and native arrival steering replaces the full
boids, wind, title enemies, seasonal transitions and title-camera sequence.
BCK menu transforms are sampled, but full E2D callbacks/BPK colours, original
menu transitions and the other game modes are incomplete.

**Valley music is not implemented.** The ISO contains `n_tutorial.bms`,
`n_tutorial_1stday.bms` and `new_00.bms` in `AudioRes/Seqs/Seq.arc`, plus conductor,
bank and wave data. These require the original sequence/layer/conductor system;
they are not ready-made streamed songs. Current demo music is still the title
stream. No substitute track is presented as Valley music. Full music integration
is the next concrete audio task.

Other incomplete systems include original Brain/actions, terrain wall collision
and routing, scenario generators, story/caves/saving, full GX materials,
J2D Onion/ship/pause/treasure screens, particles, animation events and all sound
variants. Most gameplay FSMs, particle cards and frontend navigation are native
adapters. Exact GameCube behavior, 1:1 appearance and sustained hardware FPS are
not established. The remaining two unresolved selected SE IDs are reported.

## Verification and hardware handoff

ARM compilation and CIA/3DSX packaging succeeded. Host checks passed for
captain death, held-type cycling, queued storage, carrying strength and field
capacity. An earlier D05 title build briefly rendered in Azahar at about 30 FPS;
the user reported it works. That does not validate this final revision.
**No emulator runs were performed after the user's latest stop instruction.**

Install D05 over the earlier demo: same title ID, remaster version 4. Confirm the
Valley camp placement, Onion climbing, ship entry/exit, audio and pause/exit on
hardware. Try withdrawing enough Pikmin to reach 100, then return and withdraw
each colour. Check that treasures reach the ship's glow and corpses/pellets
reach the correct Onion. Experimental 3D is disabled initially.

Send a photo of both screens, the first incorrect action, console model and
`/3ds/pikmin2-demo/demo.log`. Settings are stored beside that log. The new
revision's runtime, appearance, CSND fallback and stereoscopic behavior await
hardware feedback.

Build with `python tools/build_demo.py --iso <your-ISO> --makerom <makerom>
--bannertool <bannertool>`. See BUILDING.md for SDK setup.
