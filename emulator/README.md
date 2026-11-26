# Emulator / test environment

This is a **UI and Bluetooth-protocol emulator**, not an F1C200s SoC emulator. It runs the same LVGL UI source that targets the 800×480 framebuffer build, with an SDL2 window and a simulated Bluetooth stack.

## Run locally

```bash
# macOS/Linux with SDL2 and CMake
make emulator
```

Or build manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/strada-ui
```

Click the buttons to drive the UI:

| Control | Action |
|---|---|
| Center primary button | Power on / Pair / Play / Pause / Answer / Hang up / Retry |
| Center secondary button | Scan / Disconnect / Reject / Mute |
| Phone card **Call** | Simulate an incoming call |
| Media buttons | Play/pause, previous, next |
| Center Bluetooth circle | Same as the primary button |

Test keys in the native emulator:

| Key | Action |
|---|---|
| `1` … `8` | Off, Ready, Pairing, Connected, Playing, Incoming, Active, Error |
| `Space` | Primary action |
| `N` / `P` | Next / previous track |
| `M` | Secondary action (mute in a call) |
| `R` / `F` / `G` | Open Radio / Files / Settings |
| `H` | Back to the home screen |

### Functional panels

* **Radio** — tune, seek, presets, **save preset to 1–6**, RDS name/genre line, power, mute
* **Files** — real directory list for the native build (root `media/`), up/refresh, text/binary preview, **No USB or SD card connected** message
* **Settings** — accent color, 12/24 h clock, **date and time**, volume, **dark/light theme**, **night dim**, **screensaver**, About, reset
* **Now Playing** — maximises when playback starts, with large art and controls
* **Call** — 560×280 recent-contacts/dial-pad popup from the phone card Call button
* **Volume** — top-bar speaker button shows a live volume overlay

### Browser test UI

For a zero-install interactive test, open `prototype/index.html`. It has a **TEST** bar under the 800×480 screen with state buttons, simulate call, next/prev track, reset and hide. URL parameters `?state=incoming` and `?clean=1` work, and `window.strada` is exposed for automation.

## Headless screenshots (CI)

```bash
xvfb-run -a ./build/strada-ui --screenshot artifacts/connected.ppm --frames 30 --state connected
python3 tools/ppm_to_png.py artifacts/connected.ppm artifacts/connected.png
```

Supported states: `off`, `ready`, `pairing`, `connected`, `playing`, `incoming`, `active`, `error`.

## Docker

```bash
docker build -t strada7010b-emulator -f emulator/Dockerfile .
docker run --rm -it strada7010b-emulator
```

The image installs `build-essential`, CMake, SDL2, Xvfb and ImageMagick, builds the UI, and runs the unit tests.

## What this does and does not test

Tested:

* 800×480 layout and touch/mouse hit targets
* Bluetooth state machine and button labels
* Track/time rendering and mock AVRCP metadata
* The `55 AA LEN CMD DATA CHK` frame encoder/decoder (`tests/test_bt.c`)
* Screenshot regression in CI

Not tested:

* F1C200s DRAM/LCD/touch/audio hardware
* The stock Melis/ePDK ABI
* Real HFP/A2DP/AVRCP/PBAP against a phone
* The external 6905 MCU/BT module

For the real UART protocol, use a USB-UART or logic analyzer on the SoC↔MCU line and the frame decoder in `app/bt_uart.c`.
