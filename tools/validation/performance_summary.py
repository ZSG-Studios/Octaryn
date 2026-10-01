"""Join versioned renderer evidence without mixing startup and capture stalls."""
import csv
import json
import math
import statistics
from pathlib import Path


def read_profile(path):
    with Path(path).open(newline='', encoding='utf-8') as source:
        rows = list(csv.DictReader(source))
    if not rows:
        raise ValueError(f'Empty profile: {path}')
    records = {}
    for row in rows:
        if None in row or any(value is None for value in row.values()):
            raise ValueError(f'Profile column mismatch: {path}')
        if row.get('schema_version') not in ('2', '3', '4'):
            raise ValueError(f'Unsupported profile schema: {path}')
        record = {key: float(value) for key, value in row.items()}
        if not all(math.isfinite(value) for value in record.values()):
            raise ValueError(f'Nonfinite profile value: {path}')
        frame = int(record['frame'])
        if frame in records:
            raise ValueError(f'Duplicate frame {frame}: {path}')
        records[frame] = record
    return records


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        raise ValueError('No measured samples')
    def percentile(p):
        position = (len(ordered) - 1) * p
        low, high = math.floor(position), math.ceil(position)
        return ordered[low] + (ordered[high] - ordered[low]) * (position - low)
    return dict(samples=len(ordered), mean=statistics.mean(ordered),
                median=statistics.median(ordered), p95=percentile(.95),
                p99=percentile(.99), worst=max(ordered))


def summarize_case(case, warmup=120):
    case = Path(case)
    gpu = read_profile(case / 'gpu.csv')
    cpu = read_profile(case / 'frame-timing.csv')
    lighting = read_profile(case / 'lighting.csv')
    common = sorted(gpu.keys() & cpu.keys() & lighting.keys())
    if len(common) <= warmup:
        raise ValueError('Insufficient matched frames after warmup')
    captures = set()
    for path in case.glob('*.observation.json'):
        frame = int(json.loads(path.read_text())['frame'])
        captures.update(range(frame - 1, frame + 3))
    usable = set(common[warmup:]) - captures
    phases = {}
    motion = case / 'camera-motion.csv'
    if motion.exists():
        with motion.open(newline='') as source:
            phases = {int(row['frame']): row['phase'] for row in csv.DictReader(source)}
    def sample(frames):
        result = {}
        for group, records in [('cpu', cpu), ('gpu', gpu), ('lighting', lighting)]:
            fields = [key for key in records[frames[0]] if key.endswith('_ms')]
            result[group] = {key: distribution([records[f][key] for f in frames]) for key in fields}
        return result
    selected = sorted(usable)
    if not selected:
        raise ValueError('No uncontaminated completed frames')
    result = dict(schema_version=1, frame_ids='zero_based_render_submission',
                  warmup_frames=warmup, matched_frames=len(common), measured_frames=len(selected),
                  excluded_capture_frames=sorted(set(common) & captures),
                  frame_range=[selected[0], selected[-1]], measured=sample(selected), phases={})
    for phase in sorted(set(phases.values())):
        frames = [frame for frame in selected if phases.get(frame) == phase]
        if frames:
            result['phases'][phase] = sample(frames)
    result['cpu_time_note'] = 'Thread CPU sampling resolution is OS-dependent; use interval totals and a CPU trace for fine attribution.'
    result['memory'] = {key: distribution([cpu[f][key] for f in selected])
                        for key in cpu[selected[0]] if key.endswith('_bytes')}
    (case / 'performance.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', type=Path)
    parser.add_argument('--warmup', type=int, default=120)
    args = parser.parse_args()
    print(json.dumps(summarize_case(args.case, args.warmup), indent=2))
