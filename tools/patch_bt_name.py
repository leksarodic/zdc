#!/usr/bin/env python3
"""
Patch the Bluetooth name in a 7010B / C500_7 SPI-NOR dump.

The stock image stores the name "CAR-KIT" (7 bytes) in several fixed
locations (app_config.fex, app_config.bin and in-memory defaults).  This
tool replaces every occurrence with a new name padded to exactly 7 bytes,
so no file sizes, offsets, MINFS entries, or checksums change.

Only names of 1..7 printable ASCII characters are accepted.  The default
is STRADA (6 characters + NUL = the exact width of "CAR-KIT").

Usage:
  python3 tools/patch_bt_name.py firmware/7010b-original.bin -o out.img --name STRADA
  python3 tools/patch_bt_name.py out.img --verify-against firmware/7010b-original.bin
"""

from __future__ import annotations

import argparse
import hashlib
import sys

OLD = b"CAR-KIT"
KNOWN_SHA256 = "90fda1101bf72c70f1d0a5edb60a75c33275de34ecb2190e0385c5e153d68075"


def sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def patch(src: bytes, name: str) -> tuple[bytes, list[int]]:
    raw = name.encode("ascii")
    if not 1 <= len(raw) <= 7:
        raise ValueError("name must be 1..7 printable ASCII characters (CAR-KIT is 7 bytes)")
    if any(c < 0x20 or c > 0x7E for c in raw):
        raise ValueError("name must be printable ASCII")
    replacement = raw + b"\x00" * (7 - len(raw))

    out = bytearray(src)
    offsets: list[int] = []
    start = 0
    while True:
        at = src.find(OLD, start)
        if at < 0:
            break
        out[at : at + 7] = replacement
        offsets.append(at)
        start = at + 1

    if not offsets:
        raise ValueError("no CAR-KIT string found; is this the right dump?")
    return bytes(out), offsets


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="original or patched 8 MiB SPI-NOR dump")
    ap.add_argument("-o", "--output", help="write patched image here")
    ap.add_argument("--name", default="STRADA", help="new Bluetooth name (1..7 ASCII, default STRADA)")
    ap.add_argument("--verify-against", metavar="ORIGINAL",
                    help="verify that input differs from ORIGINAL only at the BT-name offsets")
    ap.add_argument("--force", action="store_true", help="proceed if the input SHA-256 is not the known 7010B dump")
    args = ap.parse_args(argv)

    data = open(args.input, "rb").read()
    digest = hashlib.sha256(data).hexdigest()

    if args.verify_against:
        orig = open(args.verify_against, "rb").read()
        if len(orig) != len(data):
            print("verify: size mismatch", file=sys.stderr)
            return 1
        allowed = set()
        start = 0
        while True:
            at = orig.find(OLD, start)
            if at < 0:
                break
            allowed.update(range(at, at + 7))
            start = at + 1
        diffs = [i for i in range(len(data)) if data[i] != orig[i]]
        unexpected = [i for i in diffs if i not in allowed]
        print(f"verify: {len(diffs)} bytes differ, {len(unexpected)} outside BT-name slots")
        if unexpected:
            for i in unexpected[:16]:
                print(f"  unexpected diff at {i:#x}: {orig[i]:02x} -> {data[i]:02x}")
            return 1
        if not diffs:
            print("verify: images are identical")
        return 0

    if not args.output:
        print("error: --output is required unless --verify-against is used", file=sys.stderr)
        return 1
    if digest != KNOWN_SHA256 and not args.force:
        print(f"error: input SHA-256 is {digest}", file=sys.stderr)
        print(f"       known 7010B dump is {KNOWN_SHA256}", file=sys.stderr)
        print("       use --force only if you have a verified different dump", file=sys.stderr)
        return 1

    try:
        out, offsets = patch(data, args.name)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    open(args.output, "wb").write(out)
    out_digest = hashlib.sha256(out).hexdigest()
    print(f"input:  {args.input}")
    print(f"output: {args.output}")
    print(f"name:   {args.name!r}")
    print(f"patches: {len(offsets)} at " + ", ".join(f"{o:#x}" for o in offsets))
    print(f"input sha256:  {digest}")
    print(f"output sha256: {out_digest}")
    print(f"size: {len(out)} bytes (unchanged)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
