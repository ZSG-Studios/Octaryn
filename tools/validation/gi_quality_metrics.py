"""Matched display-image measurements; these are not a radiometric GI oracle."""
import math
import re
import statistics
from capture_regions import read_bmp


def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def capture_poses(log):
    pattern = r'world_capture frame=(\d+).*?eye=([\d.-]+),([\d.-]+),([\d.-]+) yaw=([\d.-]+) pitch=([\d.-]+) fov=([\d.-]+)'
    return [dict(frame=int(row[0]), pose=list(map(float, row[1:])))
            for row in re.findall(pattern, log)]


def same_pose(first, second):
    return len(first) == len(second) and all(abs(a - b) <= .001 for a, b in zip(first, second))


def samples(path, region):
    data, offset, width, height, signed = read_bmp(path)
    x0, y0, x1, y1 = region
    if not (0 <= x0 < x1 <= 1 and 0 <= y0 < y1 <= 1):
        raise ValueError('Regions must use normalized top-left coordinates')
    values = []
    for y in range(int(height * y0), int(height * y1)):
        row = height - 1 - y if signed > 0 else y
        for x in range(int(width * x0), int(width * x1)):
            b, g, r = data[offset + (row * width + x) * 4:offset + (row * width + x) * 4 + 3]
            values.append((r / 255, g / 255, b / 255))
    if not values:
        raise ValueError('Empty image region')
    return values


def luma(rgb):
    return sum(a * b for a, b in zip(rgb, (.2126, .7152, .0722)))


def difference(first, second):
    if len(first) != len(second):
        raise ValueError('Matched images have different region dimensions')
    values = sorted(abs(luma(a) - luma(b)) for a, b in zip(first, second))
    rgb_mse = statistics.mean(sum((a[c] - b[c]) ** 2 for c in range(3)) / 3
                              for a, b in zip(first, second))
    return dict(mean_absolute_luminance=statistics.mean(values),
                p99_absolute_luminance=values[int(.99 * (len(values) - 1))],
                mean_signed_luminance=statistics.mean(luma(b) - luma(a) for a, b in zip(first, second)),
                rgb_rmse=math.sqrt(rgb_mse))


def measure(paths, regions):
    result = {}
    for name, box in regions.items():
        images = [samples(path, box) for path in paths]
        frames = []
        for values in images:
            lum = sorted(map(luma, values))
            frames.append(dict(mean_rgb=[statistics.mean(v[c] for v in values) for c in range(3)],
                mean_luminance=statistics.mean(lum), contrast_stdev=statistics.pstdev(lum),
                p05=lum[int(.05 * (len(lum) - 1))], p95=lum[int(.95 * (len(lum) - 1))],
                near_black_fraction=sum(v <= 2 / 255 for v in lum) / len(lum),
                clipped_channel_fraction=sum(max(v) >= 254 / 255 for v in values) / len(values)))
        result[name] = dict(box=box, pixels=len(images[0]), frames=frames,
            temporal_pairs=[difference(a, b) for a, b in zip(images, images[1:])])
    return result


def compare(reference, candidate, regions):
    if read_bmp(reference)[2:4] != read_bmp(candidate)[2:4]:
        raise ValueError('Image dimensions differ')
    return {name: difference(samples(reference, box), samples(candidate, box))
            for name, box in regions.items()}
