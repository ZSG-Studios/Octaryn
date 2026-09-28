"""Summarize matched runs; image error is evidence, never automatic quality approval."""
import argparse
import json
from pathlib import Path
import statistics


def load(case):
    result = json.loads((case / 'result.json').read_text())
    if result['status'] not in ('captured', 'measured') or not result['timing_qualification']:
        raise ValueError(f'Not a qualified timing run: {case}')
    return result


def compare(root):
    groups = {True: [], False: []}
    identity = None
    records = []
    for case in sorted(root.iterdir()):
        if not (case / 'performance.json').exists():
            continue
        result = load(case)
        build = json.loads((case / 'client-build.json').read_text())
        key = [result.get(name) for name in ('backend', 'upscaler_mode', 'dimensions',
               'reflection_quality', 'shadow_quality', 'camera_motion', 'camera_origin',
               'fixed_lighting', 'rt_sparse', 'map', 'draw_mode')]
        key += [{k: v for k, v in build['sha256'].items() if not k.startswith('case/')}, build['host']]
        if identity is not None and key != identity:
            raise ValueError('Executable, shader, asset, driver or workload identity differs')
        identity = key
        stats = json.loads((case / 'performance.json').read_text())
        row = dict(case=str(case), reference=result['rt_reference'],
                   gpu=stats['measured']['gpu']['total_gpu_ms'],
                   frame=stats['measured']['cpu']['total_ms'], phases=stats['phases'],
                   memory=stats['memory'])
        records.append(row)
        groups[row['reference']].append(row)
    if any(len(group) < 3 for group in groups.values()):
        raise ValueError('At least three runs per variant required')
    summary = {}
    for reference, group in groups.items():
        summary['reference' if reference else 'adaptive'] = {
            metric: {stat: statistics.median([row[metric][stat] for row in group])
                     for stat in ('mean', 'median', 'p95', 'p99', 'worst')}
            for metric in ('gpu', 'frame')}
    summary['gpu_mean_reduction_percent'] = 100 * (1 - summary['adaptive']['gpu']['mean'] /
                                                        summary['reference']['gpu']['mean'])
    summary['quality_acceptance'] = 'requires separate image and motion review'
    summary['aggregation'] = 'median of run-level statistics; worst is median run worst, not global maximum'
    summary['runs'] = records
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    args = parser.parse_args()
    report = compare(args.root)
    (args.root / 'comparison.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k != 'runs'}, indent=2))
