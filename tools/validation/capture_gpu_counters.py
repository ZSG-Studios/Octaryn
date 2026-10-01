"""One-shot DX12 GPUPerfAPI evidence, never ordinary timing qualification."""
import hashlib
import csv
import json
import math
from pathlib import Path
import sys


def add_gpu_counter_options(parser):
    parser.add_argument('--gpu-counters', action='store_true', help='Diagnostic-only one-pass hardware reflection counters')
    parser.add_argument('--gpu-counter-frame', type=int, default=240)
    parser.add_argument('--gpu-counter-names', help='Exact comma-separated discrete counters, maximum8; multipass rejected')


def resolve_gpu_counters(args):
    if not args.gpu_counters:
        if args.gpu_counter_names or args.gpu_counter_frame != 240:
            raise ValueError('Counter selection/frame requires --gpu-counters')
        return
    if args.backend != 'dx12' or args.ray_tracing != 'on' or args.temporal_reflections != 'on':
        raise ValueError('Hardware counters require DX12 with active temporal RT reflections')
    if (args.width, args.height) != (2560, 1440):
        raise ValueError('Hardware profiling requires2560x1440 output')
    if not 64 <= args.gpu_counter_frame <= args.frames - 60:
        raise ValueError('Counter target needs64 scene-ready frames and60 later readback frames')
    if args.gpu_counter_names is not None:
        names = args.gpu_counter_names.split(',')
        if not 1 <= len(names) <= 8 or len(set(names)) != len(names) or any(not name or name.strip() != name for name in names):
            raise ValueError('Select1–8 unique exact counter names')


def prepare_gpu_counters(args, env, case):
    if not args.gpu_counters:
        return dict(enabled=False)
    root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(root / 'tools/build/support'))
    from acquire_gpu_perf_api import pin, api_version
    expected = pin(root)
    sdk_root = root / 'build/dependencies/tools/gpu-perf-api' / expected['TAG']
    receipt = json.loads((sdk_root / 'receipt.json').read_text())
    if receipt['sha256'] != expected['sha256'] or receipt['TAG'] != expected['TAG']:
        raise ValueError('SDK receipt differs from central pin; run acquire_gpu_perf_api.py')
    relative = expected['SOURCE_SUBDIR'] + '/bin/GPUPerfAPIDX12-x64.dll'
    dll = (sdk_root / 'package' / relative).resolve()
    with dll.open('rb') as source:
        digest = hashlib.file_digest(source, 'sha256').hexdigest()
    if digest != receipt['files'].get(relative):
        raise ValueError('SDK DLL checksum differs from verified archive receipt')
    report = case / 'gpu-counters.jsonl'
    env['OCTARYN_CLIENT_GPU_COUNTERS_PATH'] = str(report)
    env['OCTARYN_CLIENT_GPU_COUNTERS_DLL'] = str(dll)
    env['OCTARYN_CLIENT_GPU_COUNTERS_FRAME'] = str(args.gpu_counter_frame)
    if args.gpu_counter_names is not None:
        env['OCTARYN_CLIENT_GPU_COUNTERS_NAMES'] = args.gpu_counter_names
    return dict(enabled=True, report=str(report), dll=str(dll), dll_sha256=digest,
                sdk_version=expected['TAG'], sdk_api_version=api_version(expected['TAG']),
                sdk_archive_sha256=expected['sha256'],
                requested_frame=args.gpu_counter_frame, requested_names=args.gpu_counter_names,
                fixed_sampling=args.fixed_sampling, output_dimensions=[args.width, args.height],
                frame_cpu_trace_requested=args.frame_cpu_trace,
                clock_mode='none', max_passes=1, diagnostic_only=True, timing_qualification=False,
                scope='reflection_group_excludes_composition', occupancy_claim=False)


def inspect_gpu_counters(path, metadata, log):
    if not metadata['enabled']:
        if 'gpu_counter_profile status=' in log:
            raise ValueError('Unexpected hardware counter activation')
        return metadata
    records = [json.loads(line) for line in Path(path).read_text().splitlines() if line]
    events = {}
    for row in records:
        events.setdefault(row['event'], []).append(row)
    if events.get('unsupported_or_failed') or 'profile_writer_failed' in log:
        raise ValueError('Hardware counter diagnostic failed or unsupported: ' + str(events.get('unsupported_or_failed')))
    for key in ('configuration', 'sdk', 'device', 'selection', 'submitted', 'observation', 'complete'):
        if len(events.get(key, [])) != 1:
            raise ValueError('Missing/duplicate hardware counter event: ' + key)
    config, sdk, selection = (events[key][0] for key in ('configuration', 'sdk', 'selection'))
    if config.get('schema') != 1 or config.get('clock_mode') != 'none' or config.get('diagnostic_only') is not True:
        raise ValueError('Invalid hardware counter diagnostic mode')
    if (sdk['version'] != metadata['sdk_api_version'] or
            sdk.get('version_order') != 'major,minor,build,update' or
            sdk.get('release_version') != metadata['sdk_version']):
        raise ValueError('Actual SDK version differs from matched package')
    if Path(sdk['dll']).resolve() != Path(metadata['dll']).resolve():
        raise ValueError('Actual SDK DLL differs from verified package')
    if selection['passes'] != 1 or not 1 <= selection['counters'] <= 8:
        raise ValueError('Unsupported hardware multipass/count selection')
    inventory = {row['index']: row for row in events.get('counter', [])}
    enabled, results = events.get('enabled', []), events.get('result', [])
    if len(enabled) != selection['counters'] or len(results) != len(enabled):
        raise ValueError('Counter selection/result count mismatch')
    if len({row['index'] for row in enabled}) != len(enabled):
        raise ValueError('Duplicate enabled counter')
    for ordinal, (counter, result) in enumerate(zip(enabled, results)):
        actual = inventory[counter['index']]
        if counter['ordinal'] != ordinal or counter['name'] != actual['name'] or result['name'] != actual['name'] or result['index'] != counter['index']:
            raise ValueError('Result is not in actual SDK enabled order')
        if actual['sample_type'] != 1 or not actual.get('uuid') or not actual.get('description') or not actual.get('type'):
            raise ValueError('Counter metadata does not prove public discrete sampling')
        if isinstance(result['value'], bool) or not isinstance(result['value'], (int, float)) or not math.isfinite(result['value']):
            raise ValueError('Invalid numeric hardware counter result')
    if metadata.get('requested_names') is not None and {row['name'] for row in enabled} != set(metadata['requested_names'].split(',')):
        raise ValueError('Explicit counter selection was silently changed')
    submitted, complete = events['submitted'][0], events['complete'][0]
    frame = submitted['renderer_frame']
    if not metadata['requested_frame'] <= frame <= metadata['requested_frame'] + 120 or frame != complete['renderer_frame']:
        raise ValueError('Counter sample frame differs from bounded request')
    if submitted['ready_frames'] < 64 or submitted.get('requested_and_ray_scene_ready') is not True or submitted['frame_fence'] < 1:
        raise ValueError('Missing settled scene/fence evidence')
    if complete.get('timing_qualification') is not False or complete.get('occupancy_claim') is not False:
        raise ValueError('Diagnostic sample incorrectly labelled performance/occupancy proof')
    if f'gpu_counter_profile status=complete frame={frame} counters={len(results)} passes=1' not in log:
        raise ValueError('Missing successful hardware diagnostic marker')
    observation = events['observation'][0]
    if observation['renderer_frame'] != frame or observation['dimensions'][2:] != metadata['output_dimensions']:
        raise ValueError('Hardware sample dimensions/frame differ from request')
    if len(observation['camera']) != 6 or any(not math.isfinite(value) for value in observation['camera']):
        raise ValueError('Missing actual sample camera')
    if observation['fixed_sampling'] != metadata['fixed_sampling']:
        raise ValueError('Sample fixed-phase mode differs from request')
    if metadata['fixed_sampling'] and (observation['sampling_frame'] != observation['ready_frame'] or
                                      abs(observation['delta_ms'] - 1000 / 60) > .001):
        raise ValueError('Sample temporal phase/delta differs from fixed presentation')
    return metadata | dict(status='complete', actual_frame=frame, selected=enabled, results=results,
                           exclusions=events.get('excluded', []), inventory_count=len(inventory),
                           observation=observation,
                           scene_revision=submitted['scene_revision'], frame_fence=submitted['frame_fence'])


def join_gpu_counter_frame(case, evidence):
    if not evidence['enabled']:
        return evidence
    frame = evidence['actual_frame']
    matched = {}
    for name in ('gpu.csv', 'lighting.csv'):
        with (case / name).open(newline='') as source:
            rows = [row for row in csv.DictReader(source) if int(row['frame']) == frame]
        if len(rows) != 1:
            raise ValueError('Hardware sample lacks unique renderer-frame join: ' + name)
        if None in rows[0] or any(value is None for value in rows[0].values()):
            raise ValueError('Invalid profiling CSV column count')
        matched[name] = rows[0]
    actual = [int(matched['gpu.csv'][key]) for key in ('width', 'height')]
    if actual != evidence['observation']['dimensions'][2:]:
        raise ValueError('Sample output extent differs from submitted GPU profile')
    retirement = case / 'frame-retirement.csv'
    if evidence.get('frame_cpu_trace_requested') and not retirement.is_file():
        raise ValueError('Requested CPU trace is missing: frame-retirement.csv')
    if retirement.exists():
        with retirement.open(newline='') as source:
            records = list(csv.DictReader(source))
        rows = [row for row in records if int(row['renderer_frame']) == frame]
        if not any(row['record'] == 'frame' and row['stage'] == 'complete' for row in rows):
            raise ValueError('Hardware sample lacks complete CPU owner-frame observation')
        matched['cpu_owner_frame'] = rows
        fence = [row for row in records if row['record'] == 'fence' and int(row['source_frame']) == frame
                 and int(row['fence_value']) == evidence['frame_fence'] and row['success'] == '1']
        if not fence:
            raise ValueError('Sample submission lacks a matching successful owner fence retirement')
        matched['cpu_fence_retirement'] = fence
    return evidence | dict(renderer_frame_join=matched)
