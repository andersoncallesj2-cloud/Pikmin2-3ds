# Optional user-supplied model diagnostic (HW02)

This separate target preserves all six HW01 graphics tests and adds test 7:
an optional static model on the checkerboard alongside the three spheres.
It does not port J3D, original character animation or gameplay. The model is
external SD data and is not baked into the CIA or public source.

No Olimar model has been obtained, received or rendered yet. The workspace only
contains the earlier source ZIP. The model-loader code compiles for ARM and its
converter/loader host tests pass; device/model rendering remains unverified.
Do not describe this as a working Olimar build until the actual asset is tested.

## Prepare your own OBJ

```sh
python tools/convert_obj.py /private/path/olimar.obj --output /private/path/olimar.p2mesh
```

The converter reads OBJ positions/faces and MTL diffuse (`Kd`) colors. Export
convex/triangulated static geometry; at most 6,000 triangles are accepted. Negative
indices are supported. UVs, normals, textures, skinning, transparency and animation
are not implemented by this initial untextured renderer. Missing MTLs fall back
to gray with a warning. `--up z` handles a Z-up export; otherwise Y is up.
Conversion centers the model on X/Z, places its base at Y=0 and scales its height
to 2.4 test-scene units. The input is never modified and outputs are not overwritten.

Copy the result to `/3ds/pikmin2-modeldiag/olimar.p2mesh`. Launch HW02 and advance
with R/Right to test 7. X enables interleaved colors if needed; A toggles depth.
Y reloads the file. The status must explicitly show `static mesh loaded`; an absent
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
