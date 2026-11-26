#!/usr/bin/env python3
"""Convert a binary PPM (P6) screenshot to PNG using only the stdlib."""

import argparse
import struct
import zlib


def read_ppm(path: str):
    data = open(path, "rb").read()
    if not data.startswith(b"P6"):
        raise ValueError("not a binary PPM (P6)")
    # P6 <w> <h> <max>\n<data>
    parts = []
    i = 2
    while len(parts) < 3:
        while i < len(data) and data[i] in b" \t\r\n":
            i += 1
        if data[i : i + 1] == b"#":
            while i < len(data) and data[i] not in b"\r\n":
                i += 1
            continue
        j = i
        while j < len(data) and data[j] not in b" \t\r\n":
            j += 1
        parts.append(data[i:j])
        i = j
    if i < len(data) and data[i:i + 2] == b"\r\n":
        i += 2
    elif i < len(data) and data[i] in b" \t\r\n":
        i += 1
    w, h, maxv = (int(x) for x in parts)
    if maxv != 255:
        raise ValueError("only 8-bit PPM is supported")
    pixels = data[i : i + w * h * 3]
    if len(pixels) != w * h * 3:
        raise ValueError("truncated PPM")
    return w, h, pixels


def png_chunk(tag: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)


def write_png(path: str, w: int, h: int, rgb: bytes):
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)  # filter: none
        raw.extend(rgb[y * stride : (y + 1) * stride])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)  # 8-bit RGB
    blob = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr)
    blob += png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    blob += png_chunk(b"IEND", b"")
    open(path, "wb").write(blob)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args()
    w, h, rgb = read_ppm(args.input)
    write_png(args.output, w, h, rgb)
    print(f"{args.input} -> {args.output} ({w}x{h})")


if __name__ == "__main__":
    main()
