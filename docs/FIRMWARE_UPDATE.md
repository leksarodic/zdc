# Firmware update mechanism (stock 7010B)

This note documents what the supplied 8 MiB dump (`firmware/7010b-original.bin`) shows about the stock update path. It separates **observed bytes/strings** from **inferences**.

## Evidence source

The image is an Allwinner Melis/ePDK firmware with a **MINFS** filesystem at `0x24400`. `tools/minfs_extract.py` extracts its files and modules. Key entries:

- `app_config.fex` — plain-text product configuration
- `update.mod` — update handler
- `usbd_msc.drv`, `usb_host0.drv`, `sdmmc.drv` — USB mass-storage, USB host and SD card drivers
- `spinor.drv` and the bootloader — SPI NOR access

REA's firmware-region provider was not available on this macOS host because it requires Linux plus Binwalk. The findings below come from the repo's MINFS extractor and direct `strings`/hex inspection of the same dump.

## Observed configuration

`app_config.fex` contains:

```ini
[fw_para]
;usb_fw_name=U盘固件文件路径, sd_fw_name=TF卡固件文件路径
usb_fw_name =string:f:\C500_7\ePDKv100.img
sd_fw_name =string:g:\C500_7\ePDKv100.img
```

It also contains the physical-key calibration ranges used by the MCU/IR front end (`vadd_min/vadd_max` for VOL+, `vdec_min/vdec_max` for VOL−, plus `power`, `mode`, `home`, `menu`, `next`, `prev`).

## Observed update strings

`update.mod` contains:

- `b:\STORAGE\SPINOR`
- `FULLIMG_00000000`
- `modupdate`
- `update_nor.c`
- `IMAGEWTY`
- `[IMAGE_CFG]`
- `ePDK.exf`
- `ePDK.mod`
- `restart system now`
- `send spinor page program cmd fail`
- `send spinor write enable cmd fail`
- `init spinor failed`
- `read from spinor failed`

The full dump also contains localized upgrade prompts:

- `Quer fazer o upgrade?`
- `Upgrade failed, please restart!`
- `Upgrade success, please restart!`
- `Updating Firmware` / `Upgrade MCU`

In Portuguese, “Quer fazer o upgrade?” means “Do you want to upgrade?”. This is the stock UI asking for confirmation.

## Inference: how the stock update path works

The evidence supports this flow:

1. Put the vendor update image at **`C500_7/ePDKv100.img`** on a USB drive (mounted as `f:`) or SD/TF card (mounted as `g:`).
2. Boot or enter the stock update flow. `update.mod` reads the configured `usb_fw_name` / `sd_fw_name`.
3. The image is parsed as an ePDK image container (`IMAGEWTY`, `[IMAGE_CFG]`, `ePDK.exf`, `FULLIMG_00000000`).
4. `update_nor.c` erases/programs the SPI NOR through `spinor.drv`.
5. A full image uses the full-image item; `modupdate` handles module-level updates.
6. The stock UI shows success/failure and restarts (`restart system now`).

The exact prompt timing, whether it scans at every boot or only from a service menu, and the image container checksum/header layout are **not established** by strings alone.

## What this means for this project

- The file to put on a USB/SD card for the **stock** updater is `C500_7/ePDKv100.img`, not the repo's `7010b-CAR_482.img`.
- `7010b-CAR_482.img` is a **full 8 MiB SPI dump** intended for an SPI programmer (or a full-chip FEL write), not the vendor's `ePDKv100.img` package.
- We do not currently have a verified `ePDKv100.img` builder or the full image header specification, so this repo **does not generate** a stock-over-the-air update package.
- The safe artifact in this repo remains the original dump with only the 7-byte Bluetooth-name slots patched.
- **Make two verified full backups before writing anything.** A wrong image can leave the unit unbootable without a programmer.

## Safer update checklist

1. Read the stock `app_config.fex` and confirm `product_para` / panel / key map match your exact unit.
2. Verify the SHA-256 of the original dump against the known value.
3. Keep two independent 8 MiB backups, stored off-device.
4. Prefer an SPI programmer with a verified read-back over any partial update.
5. After writing, verify the read-back hash and only then boot the unit.
6. Use the disclaimer in [../DISCLAIMER.md](../DISCLAIMER.md).

## Open questions

- Exact `ePDKv100.img` header and item checksum rules.
- Whether a full image can be generated from the MINFS layout alone.
- Whether the MCU has a separate firmware update path (`Upgrade MCU`) with its own image.
- Whether the updater checks a signature or model ID before erasing SPI NOR.
