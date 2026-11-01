# Pikmin 2 - 3DS port: D03 hardware handoff

D03 is a native gameplay prototype compiled from the user's validated European
GPVP01 disc. Its HOME Menu name is **Pikmin 2 - 3DS port**, with the original
`opening.bnr` artwork and a red Pikmin HUD icon converted privately from the disc.
It updates the D02 demo title (UniqueId `0xF2A77`, remaster version 2).

**Validation:** ARM compilation and CIA/3DSX packaging only. No emulator,
gameplay simulation or hardware run was performed for D03. The user's D02 photo
shows working geometry and the UI/material issues this update addresses.
Compilation does not establish that these fixes look correct on the console.

## Implemented in this increment

- A 1,240 by 1,300 unit authored map with original dirt texture, six obstacles,
  a pond, four fire hazards and three electrical hazards.
- Thirty starting Pikmin: ten red, ten yellow and ten blue. Each Onion starts
  with twenty stored Pikmin. The field limit is 100, including sprouts;
  additional births enter matching-colour storage when no field slot is free.
- All three original Onion models, colour register keys and spotlight models.
  Onion bodies explicitly use opaque alpha. The original spotlight is additive;
  its full multi-stage TEV/BTK animation remains incomplete.
- A matching-colour Onion menu for withdrawal and return. Food deliveries
  produce original seed models with an airborne sprouting phase and pluckable
  sprouts. Hold A near sprouts to continue plucking.
- The Hocotate ship and three original treasures: rubber duck, bolt and apple.
  Treasure goes to the ship and awards its extracted value. Defeated enemies
  and number pellets go to the majority carrier colour's Onion.
- Five enemies across red Bulborb, orange Bulborb and dwarf Bulborb appearances.
  Local sleep, chase, bite, damage, death and corpse-carry states are implemented.
  The orange enemy shares the original Chappy mesh with its own disc textures.
- Sixteen Pellet Posies, four each carrying 1, 5, 10 and 20 pellets, with cycling
  colours. Original pellet meshes, colour registers, carrier capacities and
  matching/other-colour yields come from the disc configurations. Harvested
  plants do not regrow in this demo.
- Red immunity to fire, yellow immunity to electricity, and blue immunity to
  drowning. Electricity kills other colours; fire can be whistled out. Called
  drowning Pikmin paddle toward the captain, and nearby blue Pikmin can rescue
  them. Hazard placement, collision and drowning timers remain demo adapters.
- Whistling the inactive captain recruits them and combines the following squad.
  Dismiss releases the captain; switching preserves separate squads when apart.
  Normal follow velocity adapts the original `NaviFollowState::exec` branches.
- Full 400 by 240 top-screen projection. Texture upload no longer applies the
  second vertical flip responsible for inverted glyphs and padded portraits.
- Original ground HUD textures and static BLO picture layouts for captain
  portraits, round health gauges, selected type, squad/field/total counts,
  day badge and moving sun meter. Positions are adapted for the wider display.
- World carry fractions use the original `item_0_9.bti` font and stacked
  numerator/bar/denominator layout, with original appearance/fade routines.
- Original BCA frame playback on captains, Pikmin and enemies. The converter
  retains source frames, joint hierarchy, weighted envelopes and scale
  compensation. Native PICA matrix uniforms animate immutable vertex buffers.
  Clips cover idle, locomotion, attacks, carry, ready/throw, whistle, plucking,
  drowning and death where applicable. Motion selection/timing is demo logic;
  original blending, animation events and leaf-joint attachment are incomplete.
- Native NDSP streaming of the original title music and treasure/captain-down/
  Pikmin-greeting streams. Title music is demo music, not the original area BGM.
  Sound availability is shown on the bottom screen; missing DSP firmware leaves
  the game usable without audio.

## Controls

| Action | 3DS mapping |
|---|---|
| Main stick cursor and movement | Circle Pad: gentle tilt aims, full tilt moves |
| Ready / throw | Hold A; release A to throw |
| Change held Pikmin colour | Hold A and press B |
| Whistle Pikmin / other captain | Hold B |
| Dismiss / switch captain | X / Y |
| Pluck / Onion menu | A near a sprout / Onion; hold A for continued plucking |
| Adjust Onion amount | D-pad up withdraws, down returns; A confirms, B cancels |
| Centre / rotate camera | Tap L / hold L with horizontal Circle Pad |
| Zoom / close view | Tap R / hold R for one second |
| Camera elevation | ZL |
| Swarm | New 3DS C-stick or D-pad outside the Onion menu |
| Pause / reset / exit | START; while paused Y resets, SELECT exits |

The camera/control adapter uses original ground-camera parameters and analog
thresholds, with digital shoulder-button substitutes. It does not reproduce
analog trigger travel or every original camera/gameplay mode.

## What is original code and what is new

CC0 reference: `projectPiki/pikmin2` commit
`999140f3521e22619547d5d61862a6577ea73882`.

Nine original function bodies are linked: the existing `C_MTXLookAt`, `roundAng`
and `angDist`, plus `CarryInfo::update`, `drawNumber`, `drawNumberPrim`, and
`LifeGauge::init`, `update`, `draw`. The six added UI bodies match the reference
exactly; source paths and body hashes are in
`port3ds/demo/original_ui_provenance.json`. Their platform shims are new.

Controller, whistle/cursor movement, throw calculations, camera transitions,
formation settling and captain-follow velocity adapt original source branches.
BLO, BMD, BRK, BCA and parameter readers are new native conversion code guided
by the original formats and algorithms. Original assets are extracted privately.

The map, work selection, navigation graph, storage/seed lifecycle, collision,
hazard and combat/transport state machines are new demo glue. **The complete
original Pikmin Brain, ActTransport, EnemyFSM, J2D and JAudio engines are not
linked.** This is not all original game code and is not yet an exact recreation
of every UI, animation or gameplay system. Copying the PowerPC executable into
RomFS would not make it native ARM code.

Remaining audio work includes the original BMS sequence player, area BGM and
individual Pikmin, whistle, throwing, footsteps, captain switching and enemy
sound routing. Only the four named streams have playback in this increment.
Other missing systems include saves, sprays, caves, original levels, day-end
sequences, full particle/TEV effects and the original menu engines.

## Build and hardware feedback

Install devkitPro's 3DS SDK, makerom and bannertool. Run from a path without
spaces, with a personal supported ISO:

```powershell
python tools/build_demo.py --iso "Y:\Downloads\Pikmin 2 (Europe) (En,Fr,De,Es,It).iso" --makerom C:\tools\makerom.exe --bannertool C:\tools\bannertool.exe
```

The command prints its private build folder. Its `output` directory contains
`Pikmin2-3DS-port-D03.cia`, `.3dsx` and `build-report.json`. Install the CIA using
your existing homebrew installer, or launch the 3DSX through Homebrew Launcher.
All game data is embedded. NDSP playback needs the console's usual dumped
`/3ds/dspfirm.cdc` firmware; the build does not distribute firmware.

For feedback, report launch format, HUD orientation/portraits, Onion opacity,
animation, audible music, storage/withdrawal, seeds/plucking, routing and the
largest usable squad. Include a photo of both screens and
`/3ds/pikmin2-demo/demo.log` if anything fails. Every D03 runtime feature still
needs this hardware check.

The public branch contains source and documentation. ISO-derived CIA/3DSX,
models, UI, music, icon and banner remain local private deliverables.

AST/AFC format cross-checks: [vgmstream AST reader](https://github.com/vgmstream/vgmstream/blob/master/src/meta/ast.c)
and [AFC decoder](https://github.com/vgmstream/vgmstream/blob/master/src/coding/afc_decoder.c).
