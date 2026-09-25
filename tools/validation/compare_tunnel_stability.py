"""Measure fixed tunnel receivers after the final authoritative replacement."""
import argparse
import json
from pathlib import Path
import statistics
from lighting_tunnel_fixture import receivers, difference


def measure(case):
    result = json.loads((case / 'result.json').read_text())
    paths = result['captures'][-16:]
    images = [receivers(Path(path)) for path in paths]
    values = {}
    for name in images[0]:
        light = [statistics.mean(image[name]) for image in images]
        changes = [difference(a[name], b[name])['mean_absolute'] for a, b in zip(images, images[1:])]
        values[name] = dict(mean_luminance=statistics.mean(light), luminance_span=max(light)-min(light),
                            mean_pair_change=statistics.mean(changes), maximum_pair_change=max(changes))
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    before, after = measure(args.before), measure(args.after)
    results = dict(before=before, after=after, reduction={name: 1-after[name]['mean_pair_change']/max(before[name]['mean_pair_change'], 1e-12)
                                                       for name in before},
                   scope='Last16 fixed-camera tone-mapped captures after replacement. Includes temporal convergence and all lighting; not an unbiased irradiance reference.')
    args.output.write_text(json.dumps(results, indent=2))
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
