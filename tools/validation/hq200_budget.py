"""Check HQ200 render budgets from complete, uncaptured per-frame evidence."""
import argparse
import csv
import json
import math
from pathlib import Path

from performance_summary import distribution, read_profile

GPU_BUDGETS = dict(visibility=.35, opaque=.65, lighting=.30, shadows=.45,
                   reflections=.90, transparency=.35, atmosphere=.25,
                   dynamic=.25, reconstruction=.60, ui=.10, streaming=.20, reserve=.60)
CPU_BUDGETS = dict(input_ui_audio=.40, transport=.35, render_encoding=1.10,
                   streaming=.50, profiling=.15, presentation=.25, reserve=.25)
SERVER_BUDGETS = dict(ingress=.50, player=.60, items=2.80, collision=.40,
                      replication=.50, reserve=1.20)


def evaluate(cpu, gpu, lighting):
    """Inputs contain exactly the selected frames; no outlier removal is allowed."""
    frames = sorted(cpu)
    if not frames or set(frames) != set(gpu) or set(frames) != set(lighting):
        raise ValueError('Missing matched frame records')
    if any(gpu[f]['schema_version'] < 3 or cpu[f]['schema_version'] < 3 for f in frames):
        raise ValueError('HQ200 needs schema3 upload coverage and per-frame resolution evidence')
    external_covered = all(gpu[f]['schema_version'] == 4 for f in frames)
    for frame in frames:
        row = gpu[frame]
        if row['schema_version'] not in (3, 4):
            raise ValueError('Unsupported GPU profile schema')
        if row['schema_version'] == 4:
            required = {'main_gpu_ms', 'external_as_ms', 'external_as_submissions',
                        'external_as_kind', 'external_as_covered'}
            if not required <= row.keys():
                raise ValueError('Schema4 GPU profile is missing independent AS coverage')
            if any(not math.isfinite(row[key]) or row[key] < 0 for key in required):
                raise ValueError('Invalid independent AS timing')
            count, kind = row['external_as_submissions'], row['external_as_kind']
            if (row['external_as_covered'] != 1 or count not in (0, 1) or
                    kind not in ((0,) if count == 0 else (1, 2)) or
                    (count == 0 and row['external_as_ms'] != 0)):
                raise ValueError('Invalid independent AS coverage/count/kind')
            if (abs(row['total_gpu_ms']-row['main_gpu_ms']-row['external_as_ms']) > .000003 or
                    row['streaming_ms'] + .000003 < row['external_as_ms']):
                raise ValueError('Independent AS work is missing from GPU total/streaming cost')
    for frame in frames:
        row = lighting[frame]
        if row.get("schema_version", 3) not in (2, 3, 4):
            raise ValueError("Unsupported lighting profile schema")
        if row.get("schema_version") == 4 and not {"reflection_coverage_ms", "reflection_screen_ms", "clouds_ms", "map_forward_ms", "reactive_copy_ms"} <= row.keys():
            raise ValueError("Schema4 lighting profile is missing screen-reflection costs")
    exclusive_forward = all(lighting[f].get("schema_version", 3) >= 4 for f in frames)
    checks = {}
    def limit(name, values, statistic, ceiling):
        stats = distribution(values)
        checks[name] = dict(measured=stats, statistic=statistic, budget_ms=ceiling,
                            passed=stats[statistic] <= ceiling)
    limit('frame_mean', [cpu[f]['total_ms'] for f in frames], 'mean', 4.5)
    limit('frame_p99', [cpu[f]['total_ms'] for f in frames], 'p99', 5)
    limit('frame_worst', [cpu[f]['total_ms'] for f in frames], 'worst', 8.33)
    limit('gpu_work', [gpu[f]['total_gpu_ms'] for f in frames], 'p99', 4.4)
    groups = {
        'opaque': lambda f: gpu[f]['opaque_ms'],
        'shadows': lambda f: lighting[f]['sun_trace_ms'] + lighting[f]['sun_filter_ms'],
        'reflections': lambda f: sum(v for k, v in lighting[f].items()
                                    if k.startswith('reflection_') and k.endswith('_ms')),
        'dynamic': lambda f: lighting[f]['dynamic_geometry_ms'] + lighting[f]['dynamic_motion_ms'],
        'lighting': lambda f: sum(lighting[f][k] for k in
                                  ('local_cull_ms', 'local_shade_ms', 'composition_ms')),
        'ui': lambda f: gpu[f]['ui_ms'],
        'streaming': lambda f: gpu[f]['streaming_ms'],
    }
    if exclusive_forward:
        groups.update(transparency=lambda f: lighting[f]['map_forward_ms'],
                      atmosphere=lambda f: gpu[f]['sky_ms'] + lighting[f]['clouds_ms'],
                      reconstruction=lambda f: gpu[f]['fsr_ms'] + gpu[f]['tonemap_ms'] + lighting[f]['reactive_copy_ms'])
    for name, value in groups.items():
        limit(name, [value(f) for f in frames], 'p99', GPU_BUDGETS[name])
    resolution = {}
    floor_frames = floor_misses = 0
    for f in frames:
        row = cpu[f]
        if (row['display_width'], row['display_height']) != (2560, 1440):
            raise ValueError('Output is not 2560x1440')
        rw, rh = int(row['render_width']), int(row['render_height'])
        if not (1280 <= rw <= 1920 and 720 <= rh <= 1080):
            raise ValueError('Internal resolution outside HQ200 bounds')
        if abs(rw/2560-rh/1440) > 1/1440:
            raise ValueError('Internal resolution aspect mismatch')
        if row['upscaler_mode'] != 6 or not row['dynamic_resolution'] or not row['ray_tracing_active']:
            raise ValueError('Required reconstruction/dynamic-resolution/RT path is inactive')
        key = f'{rw}x{rh}'
        resolution[key] = resolution.get(key, 0) + 1
        if rw == 1280 and rh == 720:
            floor_frames += 1
            floor_misses += gpu[f]['total_gpu_ms'] > 4.4
    coverage = dict(visibility='included in opaque until a separate span exists',
                    dynamic='requires populated item render evidence',
                    cpu='OS thread-time quantization cannot qualify 3ms p99 alone',
                    server='requires complete authority-loop evidence',
                    quality='requires consecutive image review',
                    streaming='requires real traversal and residency deadlines')
    if not exclusive_forward:
        coverage['mixed_forward'] = 'Historical forward span combines clouds, reactive copy and glass; these pass budgets are unqualified'
    if not external_covered:
        coverage['independent_as'] = ('Historical GPU spans omit independent map BLAS build/compaction submissions; '
            'streaming budgets are unqualified. Static frames with separately proven no AS work retain their measured scope.')
    external = dict(covered=external_covered)
    if external_covered:
        external.update(milliseconds=distribution([gpu[f]['external_as_ms'] for f in frames]),
                        build_submissions=sum(gpu[f]['external_as_kind'] == 1 for f in frames),
                        compaction_submissions=sum(gpu[f]['external_as_kind'] == 2 for f in frames))
    return dict(schema_version=2, profile='HQ200', render_budget_passed=exclusive_forward and external_covered and all(c['passed'] for c in checks.values()),
                independent_as=external,
                contract_passed=False, unqualified=coverage, checks=checks, measured_frames=len(frames),
                duration_seconds=sum(cpu[f]['total_ms'] for f in frames)/1000,
                frame_range=[frames[0], frames[-1]], resolution_histogram=resolution,
                floor_frames=floor_frames, floor_gpu_overruns=floor_misses,
                note='Per-pass allocations are checked independently; whole-frame timing is measured directly.')


def check_case(case, warmup=120):
    case = Path(case)
    result = json.loads((case / 'result.json').read_text())
    if result.get('status') not in ('measured', 'captured') or not result.get('timing_qualification'):
        raise ValueError('Run is not valid performance evidence')
    if result.get('captures') or list(case.glob('*.observation.json')):
        raise ValueError('Use a separate run without capture overhead')
    if not result.get('uncapped_fps') or result.get('performance_profile') != 'HQ200':
        raise ValueError('Requires uncapped HQ200 run')
    cpu, gpu, lighting = (read_profile(case / name) for name in
                          ('frame-timing.csv', 'gpu.csv', 'lighting.csv'))
    route = result.get('gameplay_route')
    if route:
        from gameplay_route_report import summarize_gameplay_route
        summarize_gameplay_route(case, route)
        with (case / 'gameplay-route.csv').open(newline='') as stream:
            ready = [int(row['frame']) for row in csv.DictReader(stream)]
        if ready != sorted(set(ready)):
            raise ValueError('Duplicate or regressing gameplay frames')
        workload = 'authoritative gameplay route; every recorded route frame included'
    else:
        with (case / 'camera-motion.csv').open(newline='') as stream:
            ready = sorted(int(row['frame']) for row in csv.DictReader(stream) if int(row['ready_frame']) >= warmup)
        workload = 'presentation camera only; gameplay traversal unqualified'
    if not ready:
        raise ValueError('No post-warmup authoritative frames')
    if any(f not in records for f in ready for records in (cpu, gpu, lighting)):
        raise ValueError('Missing completed gameplay frame; refusing biased intersection')
    report = evaluate(*({f: records[f] for f in ready} for records in (cpu, gpu, lighting)))
    report['case'] = str(case.resolve())
    report['workload'] = workload
    report['full_duration_passed'] = report['duration_seconds'] >= 120
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('case', type=Path)
    parser.add_argument('--warmup', type=int, default=120)
    args = parser.parse_args()
    report = check_case(args.case, args.warmup)
    path = args.case / 'hq200-budget.json'
    path.write_text(json.dumps(report, indent=2))
    print(json.dumps(dict(report=str(path), render_budget_passed=report['render_budget_passed'],
                          contract_passed=report['contract_passed'], checks={
                              k: dict(ms=v['measured'][v['statistic']], budget=v['budget_ms'], passed=v['passed'])
                              for k, v in report['checks'].items()}), indent=2))
    return 0 if report['render_budget_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
