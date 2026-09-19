#!/usr/bin/env python3
"""Quantify overshoot and reversal during measured DDGI lighting transitions."""
import argparse
import csv
import json
import statistics
from pathlib import Path


def analyze(path):
    rows = list(csv.DictReader(path.open(newline='')))
    phases = {}
    for row in rows:
        phases.setdefault(row['phase'], []).append(row)
    result = {}
    for name, samples in phases.items():
        energies = [(float(s['red']) + float(s['green']) + float(s['blue'])) / 3 for s in samples]
        times = [float(s['elapsed_seconds']) for s in samples]
        if len(energies) < 5:
            continue
        start = energies[0]
        final = statistics.mean(energies[-3:])
        span = final - start
        if abs(span) < 0.004:
            continue
        # Largest excursion beyond the settled level, in units of the total change.
        if span > 0:
            overshoot = max(0.0, max(energies) - final) / span
        else:
            overshoot = max(0.0, final - min(energies)) / (-span)
        # Direction reversals on the way to the target, ignoring sub-2% jitter.
        direction = 0.0
        reversals = 0
        previous = energies[0]
        for value in energies[1:]:
            step = value - previous
            previous = value
            if abs(step) < max(abs(span) * .02, 1e-5):
                continue
            sign = 1.0 if step > 0 else -1.0
            if direction and sign != direction:
                reversals += 1
            direction = sign
        # Monotonic progress after the peak: how long until within 10% of final.
        settle = None
        for time, value in zip(times, energies):
            if abs(value - final) <= max(abs(span) * .1, 1e-4):
                settle = time
                break
        result[name] = dict(start=start, final=final, span=span,
                            overshoot_fraction=overshoot, reversals=reversals,
                            settle_within_10_percent_seconds=settle,
                            minimum=min(energies), maximum=max(energies))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cases', nargs='+', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = {}
    for case in args.cases:
        for backend in ('d3d12', 'vulkan'):
            path = case / f'{backend}-samples.csv'
            if path.exists():
                report[f'{case.name}/{backend}'] = analyze(path)
    text = json.dumps(report, indent=2)
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text)


if __name__ == '__main__':
    main()