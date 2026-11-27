# Real-device status

This is important: **the stock 7010B does not run Linux.** It runs Allwinner Melis/ePDK on an ARM926-class SoC with an external MCU and a 6905-family Bluetooth module. The binary in this repository is not a direct replacement for the stock firmware.

## What is real now

- `strada-ui-fb` is a real **Linux ARMv5TE framebuffer/evdev** program.
- It uses real `/dev/fb0` output, real evdev touch and real evdev physical buttons.
- Files come from real mounts (`/media`, `/mnt`, `/run/media` or `--files-root`).
- BMP/PNG/JPG decoding is real LVGL decoding.
- UART framing for the recovered `55 AA LEN CMD DATA CHK` protocol is implemented and unit-tested.

## What is simulated or missing

- Bluetooth audio/calls are simulation (`bt_sim`). The real 7010B talks to the MCU/BT module over UART; the full command map is not implemented.
- WAV playback uses SDL audio in the emulator. The framebuffer build has no ALSA/OSS backend yet, so it shows the transport in simulated mode.
- The stock Melis/ePDK app cannot load this binary.

## To run on real hardware

You need one of these:

1. A Linux port for the 7010B/F1C200s class board with framebuffer, evdev and audio drivers, or
2. A different Linux-capable ARM board / head unit with framebuffer and touch, or
3. A Melis port of this UI (LVGL + the same screens), which is a separate firmware project.

### Example Linux startup

```bash
./strada-ui-fb \
  --fb /dev/fb0 \
  --touch /dev/input/event0 \
  --buttons /dev/input/event1 \
  --files-root /media
```

`--buttons` is optional; without it the UI still runs with touch/keys.

A minimal systemd unit:

```ini
[Unit]
Description=CAR_482 7010B UI
After=systemd-udev-settle.service

[Service]
ExecStart=/usr/bin/strada-ui-fb --fb /dev/fb0 --touch /dev/input/event0 --files-root /media
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

## Hardware checklist

- ARM926EJ-S / ARMv5TE compatible Linux userland (`arm-linux-gnueabi`).
- 800×480 framebuffer, 16 bpp preferred.
- Touch or button input through evdev.
- At least 32 MiB free RAM (64 MiB device recommended).
- Read-only rootfs is fine; presets/settings are written to the working directory.
- Audio output path for the final product (ALSA/OSS), not yet wired.

## Safety

The only flashable artifact in this repo is the **patched full SPI dump** (`7010b-CAR_482.img`). The stock updater expects `C500_7/ePDKv100.img` instead. Do not flash the Linux UI binary into the stock image.

Read [../DISCLAIMER.md](../DISCLAIMER.md) before writing anything to the device.
