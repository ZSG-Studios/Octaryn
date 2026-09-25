"""Encode the native generator's exact RGB diagnostic pixels as PNG files."""

import pathlib
import struct
import sys
import zlib


def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
        ">I", zlib.crc32(kind + payload)
    )


def convert(path):
    with path.open("rb") as source:
        if source.readline() != b"P6\n":
            raise ValueError(f"{path}: expected native RGB PPM")
        width, height = map(int, source.readline().split())
        if source.readline() != b"255\n":
            raise ValueError(f"{path}: expected 8-bit color")
        pixels = source.read()
    stride = width * 3
    if len(pixels) != stride * height:
        raise ValueError(f"{path}: incomplete pixels")
    rows = b"".join(b"\0" + pixels[start : start + stride] for start in range(0, len(pixels), stride))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows))
    png += chunk(b"IEND", b"")
    path.with_suffix(".png").write_bytes(png)


if __name__ == "__main__":
    output = pathlib.Path(sys.argv[1])
    for name in ("height", "surface-materials", "biomes", "cave-section"):
        convert(output / f"{name}.ppm")
