# Optional user-supplied model diagnostic (HW02)

This separate target preserves all six HW01 graphics tests and adds test 7:
an optional static model on the checkerboard alongside the three spheres.
It does not port J3D, original character animation or gameplay. The model is
external SD data and is not baked into the CIA or public source.

The user's OBJ/MTL has been received and tested privately: 1,195 source vertices,
2,210 triangles. The 3DSX and installed CIA render that static mesh with all three
spheres in Azahar. The model data is not distributed here. This is an untextured
preview: the MTL references `orima_565.png` and `helkan_8ia.png`, but those images
have not been received as filesystem-accessible PNG files. Helmet transparency,
texture details and animation are therefore not represented correctly.

Physical New 3DS rendering remains unverified. The existing hardware rendering
gate is still open; this optional model test does not establish a fix.

## Prepare your own OBJ

```sh
python tools/prepare_model.py /private/path/olimar.obj
```

The converter reads OBJ positions/faces and MTL diffuse (`Kd`) colors. Export
convex/triangulated static geometry; at most 6,000 triangles are accepted. Negative
indices are supported. UVs, normals, textures, skinning, transparency and animation
are not implemented by this initial untextured renderer. Missing MTLs fall back
to gray with a warning. `--up z` handles a Z-up export; otherwise Y is up.
Conversion centers the model on X/Z and places its base at Y=0. The preparation
helper scales it to 3.6 test units and bakes diagnostic face shading; direct
`convert_obj.py` defaults to 2.4 with shading disabled. This baked shading does not
implement original GX lighting. The input is never modified and outputs are not
overwritten. The provided DAE was inspected to confirm its material/image bindings;
this converter uses the OBJ and does not import DAE skinning or transforms.

Copy the result to `/3ds/pikmin2-modeldiag/olimar.p2mesh`. Launch HW02 and advance
with R/Right to test 7. X enables interleaved colors if needed; A toggles depth.
Y reloads the file and B turns it 90 degrees. The status must explicitly show `static mesh loaded`; an absent
or corrupt file shows an error rather than a substitute character. It is placed
in front of the three spheres. The first six stages remain usable without a model.

Build with the standard devkitPro toolchain:

```sh
cd port3ds/modeldiag
make -j2
make cia MAKEROM=/path/to/makerom
```

The converted mesh is bounded to 288,000 payload bytes with a CRC32, finite-position
and range checks. Together with the spheres/checkerboard, the maximum draw uses
28,080 vertices within the existing 32,768-vertex GPU allocation. These bounds do
not establish real-hardware compatibility or a frame-rate target.

Public distribution must omit original/ripped Nintendo model data and any private
converted meshes. OBJ/MTL/P2MESH and private-models paths are ignored by Git. Verify
the source and terms of any online fan model before redistribution.

Host checks: `python -m unittest discover -s tests -v` from the repository root.

## Windows kit tutorial (no setup)

1. Extract the entire `pikmin2-model-test-windows.zip` into a writable folder.
2. Keep your OBJ and MTL together. Drag the OBJ onto `Prepare Olimar.cmd`.
3. Wait for `MODEL READY: 2210 triangles`. The full generated path is printed.
4. Copy the **contents** of the kit's `sd-card` folder to the SD-card root.
5. Open **Pikmin 2 Model Diagnostic** in Homebrew Launcher, or install the CIA
   from `install/`. Either format still needs the external SD model file.
6. Advance with R six times to stage 7. B turns; Y reloads; START exits.

The kit includes official CPython 3.12.10 with its license. It includes no OBJ,
MTL, textures or converted mesh. No Docker/compiler/Python installation is needed.
Delete or move only the previous generated mesh before preparing it again.

Five host converter/loader tests pass, including CRC rejection, material colors,
negative indices, bounds, optional face shading and turn coordinates. The bundled
Windows Python/preparation script was run under Wine; its converted output exactly
matched the host output for the private OBJ/MTL. This is not physical Windows or
3DS validation. Runtime evidence is under `validation/`.
