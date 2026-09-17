"""Matched-region luminance from production 32-bit BMP captures plus PNG inspection copies."""
import statistics
import struct
from pathlib import Path


def read_bmp(path):
    data = Path(path).read_bytes()
    if data[:2] != b'BM':
        raise RuntimeError(f'Invalid GPU BMP capture: {path}')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, signed_height, planes, bits = struct.unpack_from('<iiHH', data, 18)
    height = abs(signed_height)
    if width <= 0 or height == 0 or planes != 1 or bits != 32:
        raise RuntimeError(f'Expected the production 32-bit BMP capture: {path}')
    if len(data) < offset + width * height * 4:
        raise RuntimeError(f'Truncated BMP capture: {path}')
    return data, offset, width, height, signed_height


def region_luminance(path, box):
    """box is (x0, y0, x1, y1) in top-left-origin pixel coordinates, end exclusive."""
    data, offset, width, height, signed = read_bmp(path)
    x0, y0, x1, y1 = box
    if not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
        raise RuntimeError(f'Region {tuple(box)} outside the {width}x{height} capture: {path}')
    samples = []
    for y in range(y0, y1):
        row = height - y - 1 if signed > 0 else y
        base = offset + row * width * 4
        for x in range(x0, x1):
            b, g, r = data[base + x * 4: base + x * 4 + 3]
            samples.append((.2126 * r + .7152 * g + .0722 * b) / 255)
    return dict(box=[x0, y0, x1, y1], pixels=len(samples), mean=statistics.mean(samples),
                stdev=statistics.stdev(samples) if len(samples) > 1 else 0.0,
                minimum=min(samples), maximum=max(samples))


def save_png(bmp_path, png_path):
    """PNG copies of actual GPU BMP captures for visual inspection; never synthetic."""
    from PIL import Image
    Image.open(bmp_path).save(png_path)
    return Path(png_path)
