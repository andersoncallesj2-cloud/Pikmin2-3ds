# First GX graphics path

`original_graphics.cpp` compiles verbatim method bodies for the supplied game's
`Graphics::drawLine` and `Graphics::drawSphere(Vector3f&, f32)` from
`src/sysGCU/graphics.cpp`. `provenance.json` pins the source and extracted bodies.

The narrow `Vector3f`/`Matrixf` declarations supply only what those methods use.
Unit-scale single-axis SRT matrices and the PowerPC matrix-concatenation operation
are implemented with scalar ARM arithmetic. This does not instantiate the full
original `Graphics` object or preserve its binary class layout.

Original direct-position/color GX line commands go to `backend.c`. It transforms
vertices by the original camera/model matrix and emits thin triangle ribbons to
Citro3D/PICA200. The PICA shader, depth mapping and rotated display projection are
native 3DS code. Right-handed projection is required by original C_MTXLookAt.
An early left-handed mismatch was caught and corrected in emulator testing.

Only this declared direct-vertex line format is supported. Unsupported primitive
or matrix-bank requests increment an error counter. There are no dummy GX
functions claiming to support textures, J3D materials, TEV, display lists or
lighting. Floor triangles in the diagnostic are newly generated test geometry.
No game models/textures are shipped or loaded.

The host test checks all 1,024 transformed vertices emitted by the original sphere
for radius, matrix count, primitive count and color invariants. Runtime emulator
tests exercise three spheres and camera motion with zero adapter errors.
