# Releases and updates

## Versioning

- `VERSION` is the current UI version.
- Tags use `vMAJOR.MINOR.PATCH` (for example `v0.5.0`).
- Every push to `main` builds and tests the emulator, the ARMv5 framebuffer binary and the patched SPI image.
- A tag matching `v*` also publishes a GitHub Release with the build artifacts.

## What is in a release

- `strada-ui-fb` — ARMv5TE Linux framebuffer/evdev build. This is **not** a drop-in replacement for the stock Melis firmware; it needs a Linux port on the target.
- `7010b-CAR_482.img` — the original 8 MiB SPI dump with only the Bluetooth name changed. This is the file for an SPI programmer or full-chip FEL write.
- Screenshot PNGs from the CI render job.

The stock updater expects `C500_7/ePDKv100.img` on a USB drive or SD card, not the full SPI dump. See [FIRMWARE_UPDATE.md](FIRMWARE_UPDATE.md). This repo does not yet generate that vendor image container.

## Publishing a release

```bash
# 1. Make sure main is green
git push origin main

# 2. Bump VERSION and note the changes
$EDITOR VERSION README.md

# 3. Commit, tag and push
git add VERSION README.md
git commit -m "release: v0.5.0"
git tag v0.5.0
git push origin main --tags
```

The `image`, `arm` and `emulator` jobs run, then the release job creates the GitHub Release and uploads the artifacts. `docs/RELEASE_NOTES.md` becomes the release body.

## Device update policy

1. Keep two verified full 8 MiB backups.
2. Verify the SHA-256 of the original dump.
3. Prefer an SPI programmer and verify the read-back.
4. Never flash an image from an unknown source or for a different panel/key variant.
5. The ARM UI binary is for a Linux framebuffer target, not the stock ePDK app.

Read [../DISCLAIMER.md](../DISCLAIMER.md) before flashing anything.
