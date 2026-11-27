# CAR_482 7010B — release

Experimental, unofficial interface and firmware research for the Chinese 7010B car MP5 radio.

## Assets

- `strada-ui-fb` — ARMv5TE Linux framebuffer/evdev UI. Requires a Linux port on the target; it is not a drop-in replacement for the stock Melis app.
- `7010b-CAR_482.img` — full 8 MiB SPI dump with only the Bluetooth name changed to `CAR_482`. Use an SPI programmer or full-chip FEL write.
- Screenshot PNGs from CI.

The stock updater looks for `C500_7/ePDKv100.img` on USB or SD, not this full SPI dump. See `docs/FIRMWARE_UPDATE.md`.

## Before flashing

- Keep two verified full 8 MiB backups.
- Verify the original dump SHA-256:
  `90fda1101bf72c70f1d0a5edb60a75c33275de34ecb2190e0385c5e153d68075`
- Prefer an SPI programmer and verify the read-back.
- Do not flash an image for a different panel, key or product variant.

## Disclaimer

This project is provided “as is”, without warranty. Flashing or modifying firmware can brick the device, erase data, or break radio/Bluetooth functionality. You use it entirely at your own risk. The authors and contributors are not responsible or liable for any damage, data loss, injury, legal issue, or other consequence. See `DISCLAIMER.md`.

Do not operate the UI while driving.
