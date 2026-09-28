"""Require exact RGBA parity between cropped and full-frame UI filter captures."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat


def compare(cropped, reference):
    def cases(root):
        rows = json.loads((root / 'results.json').read_text())
        return {(row['backend'], row['surface'], tuple(row['size'])): row for row in rows}
    left, right = cases(cropped), cases(reference)
    if not left or left.keys() != right.keys():
        raise ValueError('UI backend, surface or dimensions differ')
    results = []
    for key, row in left.items():
        other = right[key]
        if row['full_size_surfaces'] or not other['full_size_surfaces']:
            raise ValueError('Expected cropped candidate and full-size reference')
        images = [Image.open(item['capture']).convert('RGBA') for item in (row, other)]
        if images[0].size != images[1].size:
            raise ValueError('Capture dimensions differ')
        difference = ImageChops.difference(*images)
        maximum = max(high for _, high in difference.getextrema())
        results.append(dict(backend=key[0], surface=key[1], dimensions=key[2],
            exact_rgba=maximum == 0, maximum_channel_error=maximum,
            mean_channel_error=ImageStat.Stat(difference).mean,
            cropped_allocations=row.get('filter_allocations', []),
            reference_allocations=other.get('filter_allocations', [])))
    return dict(passed=all(row['exact_rgba'] for row in results), cases=results,
                scope='Captured static menu/card effects; not general animated UI qualification')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cropped', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = compare(args.cropped, args.reference)
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['passed'] else 1)
