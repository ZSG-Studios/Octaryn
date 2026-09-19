#!/usr/bin/env python3
"""Compare steady tails of identical authoritative torch capture phases."""
import argparse
import json
from pathlib import Path
from lighting_tunnel_fixture import inspect_stationary


def measure(case):
    result = json.loads((case / 'result.json').read_text())
    sequence = json.loads((case / 'sequence.json').read_text())
    captures = {row['frame']: row for row in sequence['captures']}
    phases = {}
    for name, frames in result['measurements']['phase_frames'].items():
        # Exclude the transition rather than counting its intended response as flicker.
        tail = frames[-8:]
        phases[name] = inspect_stationary([Path(captures[f]['path']) for f in tail])
        phases[name]['frames'] = tail
        phases[name]['elapsed_seconds'] = (captures[tail[-1]]['observed_seconds']
                                          - captures[tail[0]]['observed_seconds'])
    return dict(case=str(case), phases=phases,
                probe_refresh=result['measurements'].get('chamber_probe_refresh'),
                scope='Steady phase tails, fixed receivers, final GPU images; includes presentation and daylight drift.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cases', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    measurements = [measure(case) for case in args.cases]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(measurements, indent=2), encoding='utf-8')
    for row in measurements:
        print(row['case'])
        for name, phase in row['phases'].items():
            print(name, {region: round(values['mean_pair_change'], 6)
                         for region, values in phase['receivers'].items()})


if __name__ == '__main__':
    main()
