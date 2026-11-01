# Pikmin 2 - 3DS port D04

Hardware feedback update. ARM compilation and packaging only; no emulator or gameplay test run.

- Original MAT3 TEV stages are baked for the Onions, ship, number pellets and Pellet Posies, replacing the incorrect first-texture-only rendering. Original colour registers, layer orders and texture SRT are used. Reflections use a fixed lookup; full dynamic GX parity remains incomplete.
- The original dirt atlas is cropped to its floor region, box-filtered and repeated at a lower frequency to address the severe ground aliasing.
- Posy pellets stand upright and face the camera; carried pellets retain their flat orientation.
- Dismissal adapts Navi::releasePikis colour means, sqrt(N)*6.25 cluster radii, four separation passes and captain clearance. Pikmin walk into separate colour groups through native collision routing.
- Rendering is capped at 30 FPS; simulation remains fixed at 60 Hz.
- Original PSSE_PL_THROW (0x806), PSSE_SY_CHANGE_ORIMA (0x188d) and PSSE_SY_CHANGE_LUI (0x188e) samples are resolved from se.bms, PSound.aaf IBNK/WSYS and wScene_0.aw. The effects use a separate NDSP stream from music/jingles. This is selected sample playback, not the full JAudio sequence/effect engine.

Install D04 over D03 (same title ID, remaster version 3). Controls and the other demo features remain as described in DEMO_D03.md. Audio requires the console's existing /3ds/dspfirm.cdc. These visual and audio corrections require the user's hardware check.

Build with tools/build_demo.py using your ISO, devkitPro, makerom and bannertool. Original assets remain private; GitHub contains source only.
