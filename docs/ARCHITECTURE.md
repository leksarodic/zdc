# Architecture and the stock-firmware gap

## 1. The stock 7010B

```
SPI NOR 8 MiB
  0x000000  boot0 (eGON.BT0)
  0x006000  boot1 (eGON.BT1)
  0x024400  MINFS root filesystem
              epos.img        Melis / uC/OS-II kernel
              init.axf        first app / drivers / UI framework
              app_root.axf    main UI + media + BT layer
              desktop.mod     home screen
              theme.bin       UI resources
              lang.bin        strings
              ramdisk.iso     nested MINFS: startup.esh, uart_com.drv, display.drv, ...
```

* **CPU:** Allwinner F1C200s class, ARM926EJ-S / ARMv5TE, soft-float.
* **Display:** 800×480 RGB TFT.
* **Touch:** 4-wire resistive (`touchpanel_res.drv`).
* **MCU:** external, power/keys/IR/FM/audio switching/BT module control.
* **BT:** external 6905-family audio module (likely JieLi AC6905C). The SoC image has no HCI/BlueZ/L2CAP/RFCOMM/A2DP/AVRCP/HFP/PBAP stack.

The stock UI is an **ePDK/Melis app** loaded by `init.axf`. Its module format is a custom section table with LZMA sections (see `ANALYSIS.md` and `tools/minfs_extract.py`). Building a replacement app requires the **Allwinner ePDK SDK for F1C200s** and the Melis kernel ABI, which are not in the dump and are not part of this repo.

## 2. The UART/BT link

Recovered from `ramdisk.iso/uart_com.drv` V5.5:

```
TX: 55 AA LEN CMD DATA[LEN] CHK
CHK = (0x100 - ((LEN + CMD + sum(DATA)) & 0xFF)) & 0xFF
```

* Sync: `0x55 0xAA`
* `LEN`: data length
* `CMD`: command byte
* `CHK`: two's complement checksum over `LEN + CMD + DATA`
* Total = `LEN + 5` bytes

Command classes observed in the driver:

| CMD | Data |
|---:|---|
| `0x80` | 8 bytes |
| `0x81` | 1 byte |
| `0x8D` | 0..30 bytes |
| `0x95` | 0..20 bytes |
| `0x9B` | 2 bytes |
| `0x9D` | 3 bytes |
| other | 1 byte (generic) |

The driver exposes `b:\USER\UART_COM`, uses one of `b:\BUS\UART0/1/2`, and defaults to **115200 8N1** (`0x1C200` literal). The module firmware update path is `f:\hsn_6905.bfu`.

The UI-side state reads in `desktop.mod` are 8/9/10/11/12/13-byte records. The receive parser uses two big-endian 32-bit words at offsets 4 and 8 within a 12-byte record, with status constants such as `0x220`, `0x240`, `0xD200`..`0xD280`, `0x617C0`. The semantic map (which value means play/pause/answer/etc.) still needs a hardware capture.

## 3. This repo's three artifacts

### `strada-ui` (SDL2 emulator)

The full UI at 800×480, a mock Bluetooth state machine, and screenshot output. This is the design/test target. It uses LVGL 9.3 with the SDL driver.

### `strada-ui-fb` (ARMv5 Linux framebuffer)

The same UI for a **Linux port** of the 7010B:

* LVGL Linux framebuffer (`/dev/fb0`)
* evdev touch (`/dev/input/eventN`)
* ARMv5TE / ARM926EJ-S soft-float via `cmake/arm-linux-gnueabi.cmake`

This is not a Melis app. It will not run on the stock firmware. It is useful if you replace the stock OS with a small Linux system (Buildroot/mainline F1C200s support) or if you port the UI to another ARM Linux board with the same panel.

### `7010b-CAR_482.img` (stock SPI image, BT name patched)

The original 8 MiB dump with `CAR-KIT` replaced by `CAR_482` in the two uncompressed config locations:

* `app_config.bin` string blob
* `app_config.fex` text config

The replacement is 7 bytes exactly (`CAR_482`), so no file size, offset, MINFS entry, or checksum changes. The stock app reads the name from this config at boot and sends it to the BT module.

This is the only artifact in the repo intended to be flashed to a stock 7010B. Use an SPI programmer; see [FLASHING.md](FLASHING.md).

## 4. Why the new UI is not a stock-firmware replacement

To replace the stock UI on the 7010B you would need:

1. **Allwinner Melis/ePDK SDK** for F1C200s (toolchain, kernel headers, display/touch/audio drivers, module loader).
2. **The ePDK ABI** used by `app_root.axf` (`0xE1800000` API table, module entry/exit, scene/layer framework).
3. **The UART COM client** in the new app, with the captured command map.
4. **The MCU/BT module** left intact; do not reflash it without `hsn_6905.bfu`.
5. **A recovery path** (SPI programmer or FEL) because a bad `app_root.axf` will not boot.

A simpler path is a **Linux port**: mainline F1C200s support + a minimal Buildroot image + `strada-ui-fb` + a small BT service. That is a separate OS project; this repo gives you the UI and the protocol codec, not the kernel/rootfs.

## 5. Roadmap to a real stock-UI replacement

### Path A — Melis/ePDK app

* Get the matching ePDK SDK for F1C200s / C500_7.
* Build the SDK's sample app and confirm it boots on the radio.
* Port the UI drawing to the ePDK framebuffer API.
* Port touch/key input from `touchpanel_res.drv` / `key_ic.drv`.
* Implement the UART COM client using `app/bt_uart.c`.
* Repack `app_root.axf` into MINFS; keep boot0/boot1.
* Test with an SPI programmer and a full backup.

### Path B — Linux port

* Build a minimal Linux for F1C200s (mainline `suniv`/F1C100s support).
* Add the 800×480 panel timings and resistive touch driver.
* Add I2S/analog audio and the amplifier mute GPIO.
* Add a BT service (BlueZ + UART HCI only if the module exposes HCI; otherwise use the existing MCU protocol).
* Cross-compile `strada-ui-fb` and autostart it on `/dev/fb0`.
* Flash the Linux image separately from the stock backup.

### Path C — keep the stock UI, change only the BT name

This is what `7010b-CAR_482.img` does. It is low risk and reversible. It does not remove weather/camera/navigation/apps, because those are compiled into the stock Melis app.

## 6. Testing

* **Unit tests:** `tests/test_bt.c` covers the frame encoder/decoder and state transitions.
* **Emulator:** `make screenshot` renders four states and CI uploads them.
* **ARM:** CI checks that `strada-ui-fb` is an ARM ELF.
* **Hardware:** only a real 7010B can validate the LCD/touch/audio/MCU/BT behavior. Do not claim a hardware pass from the emulator.

## 7. Safety

* Keep a full 8 MiB backup, verified twice.
* Flash only the patched stock image to the stock radio.
* Do not flash the UI binary into the stock image.
* Do not reflash the MCU/BT module without the exact `hsn_6905.bfu`.
* Use 3.3 V for the SPI NOR and a stable 12 V supply for the radio.
