"""Measure fixed-view GPU capture changes on the fixture's stone receiver."""
import json
import statistics
import struct


def receiver(path):
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise RuntimeError(f'Invalid capture: {path}')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits = struct.unpack_from('<iiHH', data, 18)
    if width <= 0 or height == 0 or planes != 1 or bits != 32:
        raise RuntimeError('Expected the production 32-bit BMP capture')
    h = abs(height)
    pixels = []
    # The authored floor, below the wall and above the hands, excludes animated
    # sky, UI, alpha edges and the central reticle. Sample every other pixel.
    for y in range(int(h * .63), int(h * .81), 2):
        row = h - y - 1 if height > 0 else y
        for x in range(int(width * .25), int(width * .75), 2):
            address = offset + (row * width + x) * 4
            b, g, r = data[address:address + 3]
            pixels.append((.2126 * r + .7152 * g + .0722 * b) / 255)
    return pixels


def inspect_sequence(base, count):
    paths = [base] + [base.with_name(base.name + f'.sample-{i}.bmp') for i in range(1, count)]
    images = [receiver(path) for path in paths]
    if any(len(image) != len(images[0]) for image in images):
        raise RuntimeError('Capture dimensions changed during stationary qualification')
    changes = []
    for first, second in zip(images, images[1:]):
        delta = sorted(abs(a - b) for a, b in zip(first, second))
        changes.append(dict(mean=statistics.mean(delta), p99=delta[int(.99 * (len(delta) - 1))]))
    counters = [json.loads(path.with_name(path.name + '.lighting.json').read_text()) for path in paths]
    return dict(samples=count, receiver_pixels=len(images[0]), pairs=changes,
                maximum_mean=max(row['mean'] for row in changes),
                maximum_p99=max(row['p99'] for row in changes),
                valid_probes=[row['valid_probes'] for row in counters],
                scope='Fixed receiver over sampled frames; includes convergence and day/night changes, not moving-camera proof.')
