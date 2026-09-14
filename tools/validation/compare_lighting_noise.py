"""Compare temporal display-luminance variation in two fixed-camera GPU runs."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics

import numpy as np
from PIL import Image


def measure(case, region):
    result = json.loads((case / 'result.json').read_text())
    if result['mode'] != 'stationary':
        raise RuntimeError('Noise comparison requires fixed-camera runs')
    samples = []
    # Discard the first half, where probe and FSR startup convergence dominates.
    for path in result['captures'][len(result['captures']) // 2:]:
        image = np.asarray(Image.open(path).convert('RGB'), dtype=np.float64) / 255
        h, w = image.shape[:2]
        x0, y0, x1, y1 = region
        area = image[int(y0*h):int(y1*h), int(x0*w):int(x1*w)]
        samples.append(area @ np.array([.2126, .7152, .0722]))
    stack = np.stack(samples)
    deviation = stack.std(axis=0)
    delta = np.abs(np.diff(stack, axis=0))
    with (case / 'lighting.csv').open() as source:
        rows = list(csv.DictReader(source))
    timings = {}
    for name in rows[0] if rows else []:
        if not name.endswith('_ms'):
            continue
        values = [float(row[name]) for row in rows]
        if not all(math.isfinite(value) and value >= 0 for value in values):
            raise RuntimeError(f'Invalid GPU timings: {name}')
        timings[name] = statistics.median(values)
    return dict(samples=len(samples), region=region, mean_luminance=float(stack.mean()),
                temporal_std_mean=float(deviation.mean()), temporal_std_p99=float(np.quantile(deviation, .99)),
                pair_delta_mean=float(delta.mean()), pair_delta_p99=float(np.quantile(delta, .99)),
                median_gpu_ms=timings)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    results = {}
    for name, region in [('floor', (.25, .63, .75, .81)), ('torch_receiver', (.35, .48, .45, .56))]:
        candidate, reference = measure(args.candidate, region), measure(args.reference, region)
        results[name] = dict(candidate=candidate, reference=reference,
                            temporal_std_reduction=1-candidate['temporal_std_mean']/max(reference['temporal_std_mean'], 1e-12))
    results['scope'] = 'Normalized display luminance, fixed camera, last half of each capture sequence. Includes day/night and probe convergence; not an unbiased GI ground truth.'
    args.output.write_text(json.dumps(results, indent=2))
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
