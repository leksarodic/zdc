# Performance notes

The target class is an ARM926EJ-S / ARMv5TE device with very little RAM and no GPU. The Linux framebuffer build is tuned for that class; the SDL emulator keeps 32-bit color for clean screenshots.

## ARMv5 build (`strada-ui-fb`)

- **16-bit RGB565** color (`STRADA_LOW_SPEC`), so the 800×480 framebuffer is 768 KB instead of 1.5 MB.
- **2 MiB LVGL heap** instead of the 4 MiB desktop default.
- **Partial refresh**: `lv_linux_fbdev_set_force_refresh(disp, false)`. Only changed areas are flushed.
- **40 ms refresh period** (25 FPS target) instead of 33 ms.
- **`-Os` plus `-ffunction-sections -fdata-sections` and `--gc-sections`**, so unused code is dropped from the binary.
- **Widget subset**: only ARC, BAR, BUTTON, IMAGE, LABEL and SLIDER are compiled. Chart, table, calendar, textarea, keyboard, list, menu, tabview, canvas and similar widgets are disabled.
- **One 1 Hz timer** drives the clock, Bluetooth simulation and UI refresh; the idle timer is the only second periodic timer.
- LVGL logging and performance/memory overlays are disabled.
- No full-screen redraw loop. The UI redraws on state changes and partial refreshes.

## Memory estimate

| Item | Approx. |
|---|---|
| 800×480 RGB565 framebuffer | 768 KB |
| LVGL heap | 2 MiB |
| Code/data | depends on the ARM build; printed by CI (`arm-linux-gnueabi-size`) |
| UI objects + fonts | within the LVGL heap |

This fits a 64 MiB device with room for the Linux kernel and rootfs, but it does **not** make the binary runnable on the stock Melis firmware.

## SDL emulator

The desktop build keeps `LV_COLOR_DEPTH 32` so screenshots and the browser mock stay sharp. It does not use `STRADA_LOW_SPEC`.

## Future optimizations

- Replace SDL/WAV playback on the device with a small ALSA or OSS output.
- Cache unchanged label/style values to reduce `ui_update()` work further.
- Use a direct 16-bit framebuffer path with dirty rectangles if the kernel driver forces full-screen updates.
- Move image decoding off the UI thread or cache thumbnails.
