"""Describe measured tile work without treating completed requests as all requests."""
import argparse
from collections import defaultdict
import csv
import json
from pathlib import Path

from loading_report import parse_loading
from performance_summary import distribution, read_profile
from tile_metric_records import TileMetricRecords
from startup_readiness_report import initial_playable_report


def hq200_activation(records):
    resolutions = defaultdict(int)
    if not records:
        raise ValueError('HQ200 resolution evidence is missing')
    for row in records.values():
        if (row.get('display_width'), row.get('display_height')) != (2560, 1440):
            raise ValueError('HQ200 output is not 2560x1440')
        width, height = row.get('render_width', 0), row.get('render_height', 0)
        if not (1280 <= width <= 1920 and 720 <= height <= 1080) or abs(width/2560-height/1440) > 1/1440:
            raise ValueError('HQ200 internal resolution is outside the approved bounds')
        if row.get('upscaler_mode') != 6 or row.get('dynamic_resolution') != 1 or row.get('ray_tracing_active') != 1:
            raise ValueError('HQ200 reconstruction, dynamic resolution or RT is inactive')
        resolutions[f'{int(width)}x{int(height)}'] += 1
    return dict(resolution_histogram=dict(resolutions), checked_frames=len(records),
                render_budget_qualified=False)


def parse_tile_work(log, manifest_tile_count=None):
    groups = defaultdict(lambda: defaultdict(list))
    state = 'before_authority_ready'
    final_state = {}
    missed = defaultdict(int)
    observed_ready = False
    cumulative_upload_call_max = None
    first_complete = None
    request_span_lower_bound_ms = None
    measurements = TileMetricRecords()
    for line_number, line in enumerate(log.splitlines(), 1):
        if line.startswith('authoritative_player_ready'):
            observed_ready = True
            state = 'after_authority_ready'
        marker, fields = measurements.parse(line, line_number)
        if marker == 'tile_stream':
            final_state = fields
            if (first_complete is None and manifest_tile_count and
                    fields.get('resident') == manifest_tile_count and
                    fields.get('wanted', 0) > 0 and fields.get('preparing') == 0 and
                    fields.get('uploading') == 0):
                first_complete = dict(state=fields, request_span_lower_bound_ms=request_span_lower_bound_ms)
        elif marker == 'tile_readiness_deadline':
            missed[state] += 1
        elif marker == 'tile_published' and 'readiness_ms' in fields:
            groups[state]['completed_request_ready_ms'].append(fields['readiness_ms'])
            if first_complete is None:
                request_span_lower_bound_ms = max(request_span_lower_bound_ms or 0, fields['readiness_ms'])
        elif marker in ('tile_upload', 'tile_resource_setup', 'tile_ray_pump', 'tile_ray_schedule',
                         'map_resource_allocation', 'map_ray_resources', 'map_compact_resource'):
            if 'cpu_ms' in fields:
                groups[state][marker + '_cpu_ms'].append(fields['cpu_ms'])
            if marker == 'tile_ray_schedule':
                for name in ('polls', 'wait_fences', 'wait_allocations', 'operations', 'submissions',
                             'inflight', 'work_ms', 'published', 'cancelled'):
                    groups[state]['ray_schedule_' + name].append(fields[name])
                groups[state]['ray_schedule_soft_budget_overrun_ms'].append(
                    max(0, fields['cpu_ms'] - fields['soft_budget_ms']))
            if marker == 'tile_upload':
                if fields['bytes'] > fields['budget']:
                    raise ValueError('Tile upload exceeded recorded byte budget')
                groups[state]['upload_bytes'].append(fields['bytes'])
                if 'maximum_call_ms' in fields:
                    cumulative_upload_call_max = max(cumulative_upload_call_max or 0, fields['maximum_call_ms'])
    return dict(authoritative_ready_observed=observed_ready,
                initial_playable_startup=initial_playable_report(log),
                full_map_residency=dict(
                    full_manifest_ready_observed=first_complete is not None,
                    first_complete=first_complete,
                    process_launch_to_ready_ms=None,
                    request_span_lower_bound_ms=request_span_lower_bound_ms,
                    note='Full-manifest residency uses owner publication after collision and ray readiness. '
                        'Request durations are a lower bound, not a process-launch timestamp. This separate '
                        'full-map metric cannot establish initial required-visible/collision playable readiness.'),
                stages={stage: {name: distribution(values) for name, values in metrics.items()}
                        for stage, metrics in groups.items()},
                deadline_miss_records=dict(missed), final_tile_state=final_state,
                maximum_recorded_upload_call_ms=cumulative_upload_call_max,
                measurement_completeness=measurements.coverage(),
                completion_scope='Readiness samples cover successful publications only. Cancelled, queued and '
                    'unfinished requests are not latency samples; consult final state and deadline counts.',
                pump_scope='Recorded upload pumps only; binaries omitting zero-byte material pumps have '
                    'incomplete pump coverage. maximum_call_ms is a cumulative builder maximum, not a percentile.',
                stage_scope='Log order relative to authoritative ready; after-ready includes capture and stress '
                    'work. Background allocation totals can overlap frame work and must not be added to it. '
                    'Ray scheduling CPU includes its active polling/publication, not the separate retirement scan; '
                    'complete frame CPU includes both. GPU AS cost requires external command-buffer query spans.')


def frame_costs(case):
    paths = {name: case / path for name, path in
             [('cpu', 'frame-timing.csv'), ('gpu', 'gpu.csv'), ('lighting', 'lighting.csv')]}
    present = {name: read_profile(path) for name, path in paths.items() if path.is_file()}
    if not present:
        return dict(available=False)
    frames = sorted(set.intersection(*(set(records) for records in present.values())))
    union = set.union(*(set(records) for records in present.values()))
    route = case / 'tile-route.csv'
    phases = {}
    if route.exists():
        with route.open(newline='') as source:
            phases = {int(row['frame']): row['phase'] for row in csv.DictReader(source)}
    capture_frames = set()
    for path in case.glob('*.observation.json'):
        frame = int(json.loads(path.read_text())['frame'])
        capture_frames.update(range(frame - 1, frame + 3))
    def summarize(selected):
        if not selected:
            return {}
        return {group: {key: distribution([records[frame][key] for frame in selected])
                        for key in records[selected[0]]
                        if key.endswith(('_ms', '_bytes')) or key in ('render_width', 'render_height')}
                for group, records in present.items()}
    clean = [frame for frame in frames if frame not in capture_frames]
    return dict(available=True, profile_groups=list(present), matched_frames=len(frames),
                missing_frames_by_group={group: sorted(union - set(records)) for group, records in present.items()},
                all_recorded_frames_by_group={group: {
                    key: distribution([row[key] for row in records.values()])
                    for key in next(iter(records.values())) if key.endswith(('_ms', '_bytes'))}
                    for group, records in present.items()},
                all_frames_including_capture=summarize(frames),
                excluding_capture=summarize(clean),
                excluded_capture_frames=sorted(set(frames) & capture_frames),
                phases={phase: summarize([frame for frame in clean if phases.get(frame) == phase])
                        for phase in sorted(set(phases.values()))},
                scope='No warmup or expensive streaming frames removed. Phase warmup includes startup hold; '
                    'frame-driven 384m camera route is extreme streaming stress, not normal gameplay or an FPS gate.')


def report(case):
    case = Path(case)
    log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
    inputs = case / 'tile-inputs.json'
    tile_count = json.loads(inputs.read_text()).get('tile_count') if inputs.is_file() else None
    result = dict(schema_version=4, loading=parse_loading(log), tile_work=parse_tile_work(log, tile_count),
                  frames=frame_costs(case), process_launch='fresh process', os_file_cache='unknown',
                  identity_files=['client-build.json', 'tile-inputs.json', 'settings.json', 'result.json'])
    (case / 'tile-performance.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', type=Path)
    args = parser.parse_args()
    print(json.dumps(report(args.case), indent=2))
