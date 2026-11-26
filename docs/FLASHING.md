# Flashing the 7010B safely

The 7010B stores its firmware on an **8 MiB SPI NOR flash** (commonly `25L6406E`, `25QH64`, `QH64`). The dump in `firmware/7010b-original.bin` is the complete flash, not just the app.

> **The patched image in this repo is the stock firmware with only the Bluetooth name changed.** It keeps the original Melis UI. The new CAR_482 UI in `app/` is a Linux/SDL build and is **not** a replacement for the stock Melis app. Do not flash the UI binary into this image.

## 1. Before you touch the radio

1. Keep at least **two verified copies** of the original 8 MiB dump.
2. Confirm the original SHA-256:

```
90fda1101bf72c70f1d0a5edb60a75c33275de34ecb2190e0385c5e153d68075
```

3. Use a stable 12 V supply or bench power supply. Do **not** flash while the radio is connected to a car with the engine running.
4. Do not flash the MCU/BT module. Its update file is `f:\hsn_6905.bfu`; a wrong image there is much harder to recover.

## 2. Read the original flash

Recommended: a **CH341A** programmer (3.3 V) with a **SOP8 clip**, or an XGecu T48/TL866.

```bash
flashrom -p ch341a_spi -r backup1.bin
flashrom -p ch341a_spi -r backup2.bin
sha256sum backup1.bin backup2.bin
```

* Pin 1 is marked by a dot on the chip and the clip.
* Use **3.3 V**, never 5 V.
* If autodetect fails, specify the chip, for example:

```bash
flashrom -p ch341a_spi -c MX25L6406E -r backup1.bin
```

Do not continue until both reads are identical and the hash matches the known dump. If your radio is a different variant (different panel/MCU/flash), stop; the offsets and patch assumptions may not apply.

## 3. Build the patched image

```bash
python3 tools/patch_bt_name.py backup1.bin -o 7010b-CAR_482.img --name CAR_482
python3 tools/patch_bt_name.py 7010b-CAR_482.img --verify-against backup1.bin
```

The tool only changes the 7-byte `CAR-KIT` slots to `CAR_482`. It does not move data, change sizes, or touch MINFS entries. `--verify-against` must report **0 bytes outside BT-name slots**.

Expected default output: `artifacts/7010b-CAR_482.img` from `make image`.

## 4. Write the patched image

```bash
flashrom -p ch341a_spi -w 7010b-CAR_482.img
flashrom -p ch341a_spi -v 7010b-CAR_482.img
```

If the write is interrupted, do not power the radio; re-read and retry. A failed SPI write is recoverable with the programmer as long as you have the original dump.

## 5. Optional: F1C200s FEL recovery

The F1C200s boot ROM can expose **FEL over USB OTG**. The 7010B may not route USB OTG to an accessible connector; you may need test points.

```bash
sunxi-fel -l
sunxi-fel spiflash-info
sunxi-fel spiflash-read 0 0x800000 backup.bin
sunxi-fel spiflash-write 0 0x800000 7010b-CAR_482.img
```

Verify the exact `sunxi-fel` support for your SoC revision before relying on this. The SPI programmer is the safer primary method.

## 6. SD/USB update

The factory updater expects a vendor container named `ePDKv100.img` under `f:\C500_7\` (USB) or `g:\C500_7\` (SD). **A raw 8 MiB dump is not guaranteed to be accepted by that updater.** Use the SPI programmer for the raw image. If you must use the vendor updater, keep the vendor container format and only change the config through a tool that rebuilds the container.

## 7. After flashing

* The radio should boot to the original UI.
* On a phone, the Bluetooth name should be `CAR_482` (PIN `0000`).
* If the name still says `CAR-KIT`, the BT module kept its own stored name. The stock app normally sends the configured name at boot; if it does not, use the module's own update/vendor tool. Do not flash `hsn_6905.bfu` unless you have the exact file for your board.

## 8. Recovery

If the radio does not boot:

1. Disconnect power.
2. Reconnect the SOP8 clip and read the chip again.
3. Write the original `backup1.bin` back.
4. Verify the hash.
5. If the chip is not detected, check clip orientation, 3.3 V, and the programmer's driver.

Keep `firmware/7010b-original.bin` and your own `backup1.bin` outside the repo as well. Git history is not a recovery plan.
