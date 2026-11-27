# QEMU testing notes

The 7010B is an **Allwinner F1C200s-class ARM926EJ-S (ARMv5TE)** device running **Melis/ePDK**, not Linux. There are three different QEMU questions.

## 1. Running the ARMv5 Linux binary (`strada-ui-fb`)

`make arm` produces a Linux framebuffer/evdev binary. QEMU user mode can execute the instruction set:

```bash
qemu-arm ./build-arm/strada-ui-fb
```

It will not show the UI, because the binary expects `/dev/fb0` and an evdev touch device. QEMU user mode does not provide those. Options:

- run it on a real ARM Linux board / VM with fbdev, or
- use the SDL build (`strada-ui`) on a desktop for UI work, or
- boot a minimal ARM Linux under `qemu-system-arm` with a framebuffer driver and run `strada-ui-fb` there.

The last option is possible in principle (`versatilepb` or `vexpress-a9` plus a kernel/rootfs), but it is a Linux environment, not the stock Melis firmware.

## 2. Testing the ARMv5 core under QEMU user mode

This is implemented and runs in CI:

```bash
make qemu-test
```

It cross-compiles `tests/test_bt.c` with `bt_sim.c` and `bt_uart.c` as a **static ARMv5TE** binary and runs it with `qemu-arm`. This validates the Bluetooth state machine, call/music interruption logic and UART framing on the target instruction set.

Requirements:

- `arm-linux-gnueabi-gcc`
- `qemu-arm` (`qemu-user` on Debian/Ubuntu)

## 3. Booting the stock firmware

QEMU system mode does **not** currently have a machine model for the 7010B / F1C200s Melis platform. The CPU core is supported (`arm926`), but the board model would need:

- F1C200s DRAM, SPI NOR, clock/reset and interrupt controllers,
- the LCD/touch panel path,
- the external MCU + 6905 Bluetooth module over UART,
- CedarX/media blocks if video/audio is exercised.

Without that model, QEMU cannot boot `7010b-original.bin` or the patched image as a real device. A custom QEMU machine or real hardware is required.

## Practical test matrix

| Test | Tool | What it proves |
|---|---|---|
| UI layout and interaction | SDL emulator / browser | UI behavior, not target hardware |
| ARMv5 instruction set + logic | `qemu-arm` + `make qemu-test` | Core logic runs on ARMv5TE |
| Framebuffer/evdev startup | ARM Linux board or ARM VM | Linux port only |
| Stock firmware boot | Real 7010B / custom QEMU board | Actual Melis behavior |
| Flash safety | SPI programmer + verified backups | Recovery path |

See [FIRMWARE_UPDATE.md](FIRMWARE_UPDATE.md) for the stock update path and [../DISCLAIMER.md](../DISCLAIMER.md) before flashing.
