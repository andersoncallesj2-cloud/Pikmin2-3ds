# RVZ preparation follow-up — 2026-10-30

No Nintendo disc data was used. The user's OneDrive folder could not be reached:
the managed HTTP destination allowlist rejected it with CONNECT 403. This does
not establish any problem with the sharing link or its authentication.

Installed the official Debian `dolphin-emu` package, version
`2503+dfsg-1+deb13u1`, and used `/usr/games/dolphin-tool`. No Dolphin code was
copied or linked into the project. Header layout was checked against Dolphin
2503 `Source/Core/DiscIO/WIABlob.h`; the CLI performs actual conversion locally.

## Verified

- 18 host tests pass, including malformed RVZ checksum/recorded size/expansion
  bounds, missing converter and honest builder stage failure.
- Actual Dolphin ISO → RVZ → ISO conversion recovers all 12,288 generated source
  bytes, without scrubbing. [Hashes and package version](roundtrip-result.json).
- `tools/disc_image.py` with the actual converter recovers the same source bytes,
  preserves the input RVZ hash, and passes the resulting ISO to the real bounded
  banner extractor. [Conversion evidence](conversion-result.json).
- The integrated builder passes RVZ Preparation, ISO Validation, Asset Extraction
  and Asset Conversion with that fixture. The converter container has no active
  ARM SDK, so it explicitly stops at Runtime Compilation. That failure was
  expected and asserted; it is not an end-to-end RVZ CIA success claim.

The earlier raw-ISO ARM compilation/CIA/3DSX/emulator evidence remains unchanged.
No runtime or downloadable diagnostic binary was changed for this follow-up.

## Reproduce the converter check

Generate a synthetic ISO using `tests/iso_fixture.py::image`, then run:

```sh
dolphin-tool convert --input synthetic.iso --output synthetic.rvz --format rvz --compression zstd --compression_level 5 --block_size 131072
dolphin-tool convert --input synthetic.rvz --output roundtrip.iso --format iso
cmp synthetic.iso roundtrip.iso
```

Use fresh output paths. The synthetic executable is intentionally rejected by
the production builder, whose game checksum table was not relaxed. The converted
file can only be used by the explicit test API revision override.

## Still required

Download the user's dump privately after the environment network change applies;
validate its actual ID, revision, executable and filesystem; run the combined
RVZ-to-CIA build with the active SDK. Then follow the physical New 3DS rendering
gate. Neither real-disc compatibility nor hardware success has been established.
