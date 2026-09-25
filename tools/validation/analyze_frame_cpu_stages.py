"""Join opt-in renderer stage intervals to same-thread RHI timing records."""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import re


PATTERN = re.compile(r'\b(world_frame_cpu_stage|rhi_d3d12_timing) ')
PAIRS = re.compile(r'([a-z_]+)=([^\s]+)')


def records(text):
    stages, native = [], []
    for line, content in enumerate(text.splitlines(), 1):
        matches = list(PATTERN.finditer(content))
        for index, match in enumerate(matches):
            end = matches[index + 1].start() if index + 1 < len(matches) else len(content)
            row = dict(PAIRS.findall(content[match.end():end]))
            for key in ('thread_id', 'start_ns', 'end_ns'):
                row[key] = int(row[key])
            row['wall_ms'] = float(row['wall_ms'])
            elapsed = (row['end_ns'] - row['start_ns']) / 1e6
            if (row['start_ns'] < 0 or row['thread_id'] <= 0 or elapsed < 0 or
                    not math.isfinite(row['wall_ms']) or row['wall_ms'] < 0 or
                    abs(elapsed - row['wall_ms']) > .002):
                raise ValueError(f'Invalid timing interval at log line {line}')
            row['log_line'] = line
            if match[1] == 'world_frame_cpu_stage':
                row['renderer_frame'] = int(row['renderer_frame'])
                row['app_frame'] = int(row['app_frame'])
                if row['app_frame'] != row['renderer_frame'] + 1:
                    raise ValueError(f'Invalid renderer/app frame mapping at line {line}')
                stages.append(row)
            else:
                native.append(row)
    return stages, native


def union_ns(intervals):
    """Measure covered time once even when backend timers are nested."""
    total, begin, end = 0, None, None
    for left, right in sorted(intervals):
        if right <= left:
            continue
        if begin is None:
            begin, end = left, right
        elif left <= end:
            end = max(end, right)
        else:
            total += end - begin
            begin, end = left, right
    return total + (end - begin if begin is not None else 0)


def join_stage(stage, native):
    calls, intervals = [], []
    for call in native:
        if call['thread_id'] != stage['thread_id']:
            continue
        left = max(call['start_ns'], stage['start_ns'])
        right = min(call['end_ns'], stage['end_ns'])
        if left >= right:
            continue
        intervals.append((left, right))
        calls.append(dict(call, contained=call['start_ns'] >= stage['start_ns'] and
                          call['end_ns'] <= stage['end_ns'], overlap_ms=(right - left) / 1e6))
    covered = union_ns(intervals) / 1e6
    return dict(stage, native_calls=sorted(calls, key=lambda row: row['overlap_ms'], reverse=True),
                native_interval_union_ms=covered,
                outside_logged_native_intervals_ms=max(0, stage['wall_ms'] - covered))


def frame_rows(path):
    """Native writers end every completed row with LF, including CRLF on Windows."""
    if not path.exists():
        return {}, False
    text = path.read_bytes().decode('utf-8-sig')
    incomplete_tail = bool(text) and not text.endswith('\n')
    if incomplete_tail:
        text = text[:text.rfind('\n') + 1]
    rows = csv.DictReader(io.StringIO(text, newline=''), strict=True)
    if not rows.fieldnames or 'frame' not in rows.fieldnames or len(set(rows.fieldnames)) != len(rows.fieldnames):
        raise ValueError(f'Missing or ambiguous frame CSV header in {path}')
    output = {}
    for row in rows:
        if None in row or any(value is None or value == '' for value in row.values()):
            raise ValueError(f'Malformed completed CSV row in {path}')
        frame = int(row['frame'])
        if frame < 0 or frame in output:
            raise ValueError(f'Negative or duplicate frame identity {frame} in {path}')
        output[frame] = row
    return output, incomplete_tail


def analyze(case):
    log = case / 'client.log'
    stages, native = records(log.read_text(encoding='utf-8', errors='replace'))
    if not stages:
        raise ValueError('No slow CPU stage records; tracing may be disabled or all stages were below 10 ms')
    app, app_tail = frame_rows(case / 'frame-timing.csv')
    gpu, gpu_tail = frame_rows(case / 'gpu-profile.csv')
    report = []
    for stage in stages:
        row = join_stage(stage, native)
        row['application'] = app.get(stage['app_frame'])
        row['gpu'] = gpu.get(stage['renderer_frame'])
        report.append(row)
    report.sort(key=lambda row: row['wall_ms'], reverse=True)
    counts = {}
    for row in report:
        group = counts.setdefault(row['stage'], {'count': 0, 'total_logged_ms': 0, 'max_logged_ms': 0})
        group['count'] += 1
        group['total_logged_ms'] += row['wall_ms']
        group['max_logged_ms'] = max(group['max_logged_ms'], row['wall_ms'])
    return dict(status='analyzed', case=str(case), log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),
                logged_stages=len(stages), logged_native_calls=len(native), by_stage=counts,
                partial_csv_tail_omitted={'application': app_tail, 'gpu': gpu_tail}, stages=report,
                limits=[
                    'Only stages at least 10 ms and backend calls above their logging threshold are present.',
                    'Same-thread clock overlap proves interval location, not a driver, paging or scheduler root cause.',
                    'Backend timers can nest. The interval union counts covered time once; do not sum nested calls.',
                    'Uncovered time may contain short unlogged calls, scheduling, application work or logging overhead.',
                    'GPU timestamps describe execution intervals, not frame-head scheduling or physical memory residency.',
                    'CSV joins use logged frame identities, not absolute CPU clocks; renderer N maps to app N+1 only on completion.',
                    'Resize retries can reuse renderer N; threshold-only stage logs cannot fully separate every aborted/retried attempt.',
                    'Unterminated CSV tails are omitted even when their partial last value parses as a number.',
                    'Normal local destruction after the final trace boundary is outside these intervals.',
                ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', type=Path)
    args = parser.parse_args()
    case = args.case.resolve()
    report = analyze(case)
    path = case / 'frame-cpu-stage-analysis.json'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(report=str(path), logged_stages=report['logged_stages'],
                          worst_stage=report['stages'][0]['stage'],
                          worst_stage_ms=report['stages'][0]['wall_ms'])))


if __name__ == '__main__':
    main()
