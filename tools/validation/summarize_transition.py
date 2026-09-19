#!/usr/bin/env python3
"""Print the worst transition overshoot/reversal cases from an analysis report."""
import argparse
import json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report')
    args = parser.parse_args()
    data = json.loads(open(args.report, encoding='utf-8').read())
    worst = []
    for case, phases in data.items():
        for name, row in phases.items():
            print(f"{case:22s} {name:28s} overshoot={row['overshoot_fraction']*100:6.2f}% "
                  f"reversals={row['reversals']:2d} settle10%={row['settle_within_10_percent_seconds']}")
            worst.append((row['overshoot_fraction'], row['reversals'], name, case))
    if worst:
        worst.sort(reverse=True)
        print('\nworst overshoot:', worst[0])
        print('max reversals:', max(w[1] for w in worst))


if __name__ == '__main__':
    main()