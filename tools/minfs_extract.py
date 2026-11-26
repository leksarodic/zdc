#!/usr/bin/env python3
"""
MinFS extractor for Allwinner Melis/ePDK images (F1C200s 7010B, etc.)

Reads the MINFS filesystem found in a raw SPI-NOR dump, lists entries and
extracts uncompressed files, single-stream LZMA files and LZMA "modules"
(section-table + per-section LZMA) to flat bytes.

The format here is the one used by the 7010B image in this repo:

  header      "MINFS", version, root offset, root size, entry count,
              tree size, data length, image size
  dentry      offset, size, unpacked, entry_len, attr, name_len, extra_len,
              name, extra
  attr        DIR=1, MODULE=2, COMPRESS=4
  LZMA        5-byte props (lc/lp/pb + dict size) + raw LZMA1 stream

Usage:
  python3 minfs_extract.py IMAGE --list
  python3 minfs_extract.py IMAGE --extract OUT_DIR
  python3 minfs_extract.py IMAGE --extract OUT_DIR --manifest
"""

from __future__ import annotations

import argparse
import json
import lzma
import os
import struct
import sys

MINFS_MAGIC = b"MINFS"
ATTR_DIR = 1
ATTR_MODULE = 2
ATTR_COMPRESS = 4


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<L", b, off)[0]


def align4(n: int) -> int:
    return (n + 3) & ~3


def lz_decode(blob: bytes, out_len: int) -> bytes:
    """7-zip LzmaUncompress equivalent: [props5][raw LZMA1]."""
    if len(blob) < 5:
        raise ValueError("LZMA section shorter than 5 bytes")
    props = blob[0]
    pb, rem = divmod(props, 45)
    lp, lc = divmod(rem, 9)
    dict_size = struct.unpack("<L", blob[1:5])[0]
    dec = lzma.LZMADecompressor(
        format=lzma.FORMAT_RAW,
        filters=[
            {
                "id": lzma.FILTER_LZMA1,
                "dict_size": max(dict_size, 1 << 12),
                "lc": lc,
                "lp": lp,
                "pb": pb,
            }
        ],
    )
    out = dec.decompress(blob[5:], out_len)
    if len(out) != out_len:
        raise ValueError(f"short LZMA decode: {len(out)} != {out_len}")
    return out


def parse_minfs(image: bytes, base_override: int | None = None):
    if base_override is not None:
        base = base_override
        if image[base : base + 5] != MINFS_MAGIC:
            raise ValueError(f"no MINFS magic at {base:#x}")
    else:
        candidates = []
        start = 0
        while True:
            base = image.find(MINFS_MAGIC, start)
            if base < 0:
                break
            if base + 32 <= len(image):
                version = struct.unpack_from("<H", image, base + 6)[0]
                root_off = u32(image, base + 8)
                file_count = u32(image, base + 16)
                image_size = u32(image, base + 28)
                if (
                    version in (0x0100, 0x0101)
                    and 0x20 <= root_off < 0x100000
                    and 0 < file_count < 100000
                    and image_size <= len(image) - base
                ):
                    candidates.append((image_size, base))
            start = base + 1
        if not candidates:
            raise ValueError("not a MINFS image (no valid header)")
        # A dump can contain a nested MINFS (e.g. inside ramdisk.iso). The
        # system rootfs is the largest valid image.
        base = max(candidates)[1]
    version = struct.unpack_from("<H", image, base + 6)[0]
    root_off = u32(image, base + 8)
    root_size = u32(image, base + 12)
    file_count = u32(image, base + 16)
    tree_size = u32(image, base + 20)
    data_len = u32(image, base + 24)
    image_size = u32(image, base + 28)

    entries = []
    pos = base + root_off
    for _ in range(file_count):
        off, size, unpacked, entry_len, attr, name_len, extra_len = struct.unpack_from(
            "<LLLHHHH", image, pos
        )
        raw_name = image[pos + 20 : pos + 20 + name_len]
        name = raw_name.decode("utf-8", "replace")
        extra = image[pos + 20 + align4(name_len) : pos + 20 + align4(name_len) + extra_len]
        entries.append(
            dict(
                name=name,
                offset=off,
                size=size,
                unpacked=unpacked,
                entry_len=entry_len,
                attr=attr,
                extra=extra,
                pos=pos,
            )
        )
        pos += entry_len

    return dict(
        base=base,
        version=version,
        root_off=root_off,
        root_size=root_size,
        file_count=file_count,
        tree_size=tree_size,
        data_len=data_len,
        image_size=image_size,
        entries=entries,
    )


def sections(entry):
    extra = entry["extra"]
    for i in range(len(extra) // 32):
        f = struct.unpack("<8L", extra[i * 32 : i * 32 + 32])
        yield dict(
            index=i,
            stored_off=f[0],
            stored_size=f[1],
            unpacked_size=f[2],
            size=f[3],
            vaddr=f[4],
            type=f[5],
            flags=f[6],
            attr=f[7],
        )


def extract_entry(image: bytes, base: int, entry):
    if entry["attr"] & ATTR_DIR:
        return None, "dir", {}
    start = base + entry["offset"]
    stored = image[start : start + entry["size"]]
    meta = {}

    if entry["attr"] & ATTR_COMPRESS and entry["attr"] & ATTR_MODULE:
        flat = bytearray()
        meta["sections"] = []
        for sec in sections(entry):
            seg = stored[sec["stored_off"] : sec["stored_off"] + sec["stored_size"]]
            if sec["attr"] & 2 and sec["stored_size"]:
                payload = lz_decode(seg, sec["unpacked_size"])
            elif sec["type"] == 8:  # NOBITS / bss
                payload = b"\0" * sec["size"]
            else:
                payload = seg
            meta["sections"].append(
                {
                    "vaddr": sec["vaddr"],
                    "size": sec["size"],
                    "type": sec["type"],
                    "attr": sec["attr"],
                    "stored_size": sec["stored_size"],
                    "payload_size": len(payload),
                }
            )
            flat += payload
        return bytes(flat), "module", meta

    if entry["attr"] & ATTR_COMPRESS:
        return lz_decode(stored, entry["unpacked"]), "lzma", meta

    return stored, "raw", meta


def safe_relpath(name: str) -> str:
    # MINFS names in this image are flat, but keep extraction inside the target.
    name = name.replace("\\", "/")
    parts = [p for p in name.split("/") if p not in ("", ".", "..")]
    return os.path.join(*parts) if parts else "unnamed"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", help="raw SPI-NOR dump containing MINFS")
    ap.add_argument("--list", action="store_true", help="list entries")
    ap.add_argument("--extract", metavar="DIR", help="extract entries to DIR")
    ap.add_argument("--manifest", action="store_true", help="write manifest.json (with --extract)")
    ap.add_argument("--base", type=lambda x: int(x, 0), help="force MINFS base offset")
    args = ap.parse_args(argv)

    image = open(args.image, "rb").read()
    fs = parse_minfs(image, args.base)
    base = fs["base"]

    if args.list or not args.extract:
        print(
            f"MINFS @ {base:#x}  version={fs['version']:#x}  entries={fs['file_count']}  "
            f"tree={fs['tree_size']:#x}  data={fs['data_len']:#x}  image={fs['image_size']:#x}"
        )
        for e in fs["entries"]:
            kind = "dir" if e["attr"] & ATTR_DIR else (
                "module" if e["attr"] & ATTR_MODULE else ("lzma" if e["attr"] & ATTR_COMPRESS else "file")
            )
            print(
                f"{kind:6s} {e['name']:<34s} off={e['offset']:#08x} "
                f"size={e['size']:#08x} unpack={e['unpacked']:#08x} attr={e['attr']:#x}"
            )

    if not args.extract:
        return 0

    os.makedirs(args.extract, exist_ok=True)
    manifest = []
    for e in fs["entries"]:
        out, kind, meta = extract_entry(image, base, e)
        record = {
            "name": e["name"],
            "kind": kind,
            "offset": e["offset"],
            "size": e["size"],
            "unpacked": e["unpacked"],
            "attr": e["attr"],
            **meta,
        }
        if out is None:
            manifest.append(record)
            continue
        path = os.path.join(args.extract, safe_relpath(e["name"]))
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        with open(path, "wb") as f:
            f.write(out)
        record["out_len"] = len(out)
        manifest.append(record)
        print(f"{kind:6s} {e['name']:<34s} -> {len(out):>9d} bytes")

    if args.manifest:
        with open(os.path.join(args.extract, "manifest.json"), "w") as f:
            json.dump(manifest, f, indent=1)
        print(f"manifest: {os.path.join(args.extract, 'manifest.json')}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
