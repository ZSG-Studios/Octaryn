"""Compare explicit matrix variants on identical observed camera/frame workloads."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics

from performance_summary import distribution, read_profile
from asset_evidence import compare_recorded_asset_identities

CAMERA = ('phase', 'eye_x', 'eye_y', 'eye_z', 'yaw', 'pitch')
STATS = ('mean', 'median', 'p95', 'p99')


def camera_records(path):
    with path.open(newline='') as source:
        rows = list(csv.DictReader(source))
    result = {}
    for row in rows:
        ready, frame = int(row['ready_frame']), int(row['frame'])
        if ready in result or any(not math.isfinite(float(row[key])) for key in CAMERA[1:]):
            raise ValueError(f'Invalid camera samples: {path}')
        result[ready] = (frame, tuple(row[key] if key == 'phase' else float(row[key]) for key in CAMERA))
    if not result or sorted(result) != list(range(len(result))):
        raise ValueError(f'Camera ready frames are missing/noncontiguous: {path}')
    return result


def require_same_camera(records):
    reference = {ready: row[1] for ready, row in records[0].items()}
    for record in records[1:]:
        if {ready: row[1] for ready, row in record.items()} != reference:
            raise ValueError('Actual ready-frame camera coordinates/phases differ')


def capture_lighting(case, camera):
    ready_by_frame = {row[0]: ready for ready, row in camera.items()}
    result = {}
    for path in sorted(case.glob('*.lighting.json')):
        data = json.loads(path.read_text())
        frame = data['render_frame']
        if frame not in ready_by_frame:
            raise ValueError(f'Capture has no observed camera sample: {path}')
        key = ready_by_frame[frame]
        value = (data['sky_light_direction'], data['sky_time'])
        if not all(math.isfinite(float(v)) for part in value for v in part):
            raise ValueError(f'Nonfinite captured sunlight: {path}')
        result[key] = value
    return result


def read_case(entry):
    case = Path(entry['case'])
    result = json.loads((case / 'result.json').read_text())
    if result.get('status') not in ('captured', 'measured') or not result.get('timing_qualification'):
        raise ValueError(f'Not a qualified timing run: {case}')
    if result.get('ray_diagnostics') or result.get('rhi_validation') or result.get('debug') or (case / 'ray-diagnostics.csv').exists():
        raise ValueError(f'Instrumented diagnostics cannot establish rendering timing: {case}')
    log_path = case / 'client.log'
    if log_path.is_file() and 'world_validation core=required' in log_path.read_text(errors='replace'):
        raise ValueError(f'Runtime enabled graphics validation during timing: {case}')
    if not result.get('uncapped_fps') or not result.get('fixed_lighting'):
        raise ValueError(f'Uncapped fixed-lighting workload required: {case}')
    variant = entry['variant']
    if variant not in ('reference', 'adaptive', 'history', 'sparse', 'indirect', 'lod', 'meshlet'):
        raise ValueError(f'Unknown matrix variant: {variant}')
    actual = (result['rt_reference'], result['rt_sparse'], result.get('rt_history_search', False),
              result['draw_mode'], result['lod_pixels'])
    expected = (variant == 'reference', variant == 'sparse', variant == 'history',
                'meshlet' if variant == 'meshlet' else 'indirect' if variant in ('indirect', 'lod') else 'direct',
                .5 if variant == 'lod' else 0.)
    if actual != expected:
        raise ValueError(f'Matrix variant does not match executed settings: {case}: {actual} != {expected}')
    expected_size = {'1440p': [2560, 1440], '4k': [3840, 2160]}[entry['size']]
    if result['dimensions'] != expected_size or result['upscaler_mode'] != entry['mode'] or \
            result['camera_motion'] != (entry['workload'] == 'motion'):
        raise ValueError(f'Matrix dimensions/mode/workload mismatch: {case}')
    camera = camera_records(case / 'camera-motion.csv')
    profiles = {'cpu': read_profile(case / 'frame-timing.csv'),
                'gpu': read_profile(case / 'gpu.csv'), 'lighting': read_profile(case / 'lighting.csv')}
    for row in profiles['gpu'].values():
        if [row['width'], row['height']] != expected_size:
            raise ValueError(f'Actual GPU output dimensions differ: {case}')
    fovs = {}
    for ready,(frame,observed) in camera.items():
        row = profiles['cpu'].get(frame)
        if row is None: continue
        if tuple(row[key] for key in CAMERA[1:]) != observed[1:]:
            raise ValueError(f'Camera trace and actual CPU render profile disagree: {case}')
        fovs[ready] = row['fov']
    build = json.loads((case / 'client-build.json').read_text())
    identity = [result.get(key) for key in ('backend', 'dimensions', 'render_dimensions', 'upscaler_mode',
        'ray_tracing', 'reflection_distance', 'temporal_reflections', 'reflection_quality',
        'shadow_quality', 'camera_origin', 'fixed_lighting', 'process_priority')]
    # Older matrix cases recorded parsed manifest and GLB hash; newer capture
    # tooling also records manifest-byte hash and source path. Compare the
    # common content identities, not incidental provenance field additions.
    identity += [{key:result['map'][key] for key in ('sha256','manifest')}]
    identity += [build['host'], {key: value for key, value in build['sha256'].items() if not key.startswith('case/')}, fovs]
    return dict(entry=entry, result=result, camera=camera, identity=identity,
                sunlight=capture_lighting(case, camera), profiles=profiles)


def matched_frames(cases):
    common = None
    excluded = set()
    warmup = max(case['result']['warmup_frames'] for case in cases)
    for case in cases:
        camera = case['camera']
        ready_by_frame = {row[0]: ready for ready, row in camera.items()}
        complete = set.intersection(*(set(profile) for profile in case['profiles'].values()))
        ready = {ready_by_frame[frame] for frame in complete if frame in ready_by_frame and ready_by_frame[frame] >= warmup}
        common = ready if common is None else common & ready
        for path in Path(case['entry']['case']).glob('*.observation.json'):
            frame = int(json.loads(path.read_text())['frame'])
            if frame not in ready_by_frame:
                raise ValueError(f'Capture frame absent from camera trace: {path}')
            center = ready_by_frame[frame]
            excluded.update(range(center - 1, center + 3))
    selected = sorted(common - excluded)
    if len(selected) < 120:
        raise ValueError('Insufficient identical uncontaminated ready frames')
    return selected, sorted(excluded), warmup


def run_statistics(case, ready_frames):
    frames = [case['camera'][ready][0] for ready in ready_frames]
    result = {}
    for group, records in case['profiles'].items():
        result[group] = {}
        for key in records[frames[0]]:
            if key.endswith('_ms'):
                values = [records[frame][key] for frame in frames if records[frame][key] >= 0]
                if values: result[group][key] = distribution(values)
    result['memory'] = {key: distribution([case['profiles']['cpu'][frame][key] for frame in frames])
                        for key in case['profiles']['cpu'][frames[0]] if key.endswith('_bytes')}
    return result


def aggregate(runs):
    groups = {}
    for group in ('cpu', 'gpu', 'lighting', 'memory'):
        fields = set.intersection(*(set(run[group]) for run in runs))
        groups[group] = {}
        for field in sorted(fields):
            values = [run[group][field] for run in runs]
            summary = {stat: statistics.median(row[stat] for row in values) for stat in STATS}
            summary['worst'] = max(row['worst'] for row in values)
            summary['run_mean_range'] = max(row['mean'] for row in values) - min(row['mean'] for row in values)
            summary['run_means'] = [row['mean'] for row in values]
            groups[group][field] = summary
    return groups


def summarize_group(cases):
    if any(case['identity'] != cases[0]['identity'] for case in cases[1:]):
        raise ValueError('Executable/shader/asset/driver or fixed workload identities differ')
    require_same_camera([case['camera'] for case in cases])
    asset_coverage = compare_recorded_asset_identities([case['result']['map'] for case in cases])
    variants = {}
    for case in cases: variants.setdefault(case['entry']['variant'], []).append(case)
    if 'reference' not in variants:
        raise ValueError('Matrix group requires a reference variant')
    sunlight = None
    for variant, members in variants.items():
        if sorted(case['entry']['repeat'] for case in members) != [0, 1, 2]:
            raise ValueError(f'Exactly three distinct repetitions required: {variant}')
        captures = [case['sunlight'] for case in members if case['sunlight']]
        if not captures: raise ValueError(f'No actual sunlight capture evidence: {variant}')
        for capture in captures:
            if sunlight is None: sunlight = capture
            elif capture != sunlight: raise ValueError('Captured sun direction/time or ready frames differ')
    selected, excluded, warmup = matched_frames(cases)
    summaries = {}
    for variant, members in variants.items():
        members.sort(key=lambda case: case['entry']['repeat'])
        runs = [run_statistics(case, selected) for case in members]
        phases = {phase: aggregate([run_statistics(case, [ready for ready in selected
                  if case['camera'][ready][1][0] == phase]) for case in members])
                  for phase in sorted({cases[0]['camera'][ready][1][0] for ready in selected})}
        summaries[variant] = dict(metrics=aggregate(runs), phases=phases,
            runs=[dict(case=case['entry']['case'], repeat=case['entry']['repeat'], metrics=run)
                  for case,run in zip(members,runs)])
    reference = summaries['reference']['metrics']['gpu']['total_gpu_ms']
    comparisons = {}
    for variant, summary in summaries.items():
        if variant == 'reference': continue
        baseline = 'adaptive' if variant != 'adaptive' and 'adaptive' in summaries else 'reference'
        base = summaries[baseline]['metrics']['gpu']['total_gpu_ms']
        metric = summary['metrics']['gpu']['total_gpu_ms']
        reduction = base['mean'] - metric['mean']
        paired = [a-b for a,b in zip(base['run_means'], metric['run_means'])]
        noise = max(base['run_mean_range'], metric['run_mean_range'])
        comparisons[variant] = dict(baseline=baseline, gpu_mean_reduction_ms=reduction,
            gpu_mean_reduction_percent=100 * reduction/base['mean'], paired_reduction_ms=paired,
            reference_gpu_mean_reduction_percent=100*(reference['mean']-metric['mean'])/reference['mean'],
            conservative_run_spread_ms=noise, exceeds_observed_noise=reduction > noise and min(paired) > 0,
            quality_acceptance='requires separate image and motion review')
    return dict(backend=cases[0]['result']['backend'], dimensions=cases[0]['result']['dimensions'],
        render_dimensions=cases[0]['result']['render_dimensions'],
        reconstruction='native' if cases[0]['entry']['mode'] == 0 else
                       'native_temporal_AA' if cases[0]['entry']['mode'] == 1 else 'reconstructed',
        warmup_ready_frames=warmup, matched_ready_frames=len(selected), ready_frame_range=[selected[0],selected[-1]],
        excluded_capture_ready_frames=excluded, camera_equality='all observed ready frames exactly equal',
        sunlight_equality='all available captures exactly equal; uncaptured repetitions have no direct sun sample',
        cooked_identity_recorded_runs=asset_coverage['cooked_metadata_recorded_runs'], total_runs=len(cases),
        asset_identity_coverage=asset_coverage,
        asset_identity_note=asset_coverage['note'],
        variants=summaries, comparisons=comparisons)


def summarize(root):
    entries = json.loads((root / 'matrix.json').read_text())
    case_paths = [str(Path(entry['case']).resolve()).casefold() for entry in entries]
    if len(set(case_paths)) != len(case_paths):
        raise ValueError('Matrix repetitions must use distinct captured runs')
    groups = {}
    for entry in entries:
        case = read_case(entry)
        key = f"{case['result']['backend']}-{entry['size']}-mode{entry['mode']}-{entry['workload']}"
        groups.setdefault(key, []).append(case)
    if not groups: raise ValueError('Empty matrix')
    return dict(schema_version=1,
        aggregation='median of run-level mean/median/p95/p99; worst is maximum across all runs',
        noise='Three-run observed spread check, not a statistical significance or visual-quality claim',
        groups={key: summarize_group(cases) for key,cases in groups.items()})


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    args=parser.parse_args()
    report=summarize(args.root)
    (args.root / 'matrix-summary.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({key:value['comparisons'] for key,value in report['groups'].items()}, indent=2))
