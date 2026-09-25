"""Capture world-space transport and direct-light controls with separate readiness."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import tempfile

from benchmark_chunk_loading import fixture_digest, inspect_snapshot, summarize
from benchmark_presentation import content_digest, digest
from gi_quality_metrics import capture_poses, require, same_pose
from capture_watchdog import run_capture
from gi_quality_fixture import write_json
from gi_quality_metrics import compare, measure
from world_gi_capture import assert_world_gi, assert_world_gi_sequence
from block_transport_values import summarize as summarize_values, compare as compare_values


def runtime_summary(case):
    result = summarize(case, 4)
    # Admission misses are measured cache pressure, not engine failure messages.
    statistics = re.compile(r'block_transport_statistics(?: [a-z_]+=\d+)+')
    result['transport_statistics'] = [line for line in result['failures'] if statistics.fullmatch(line)]
    result['failures'] = [line for line in result['failures'] if not statistics.fullmatch(line)]
    return result


def capture_readiness(rows, mode):
    for row in rows:
        require(row.get('gi_mode') == mode, 'Captured GI mode differs from the requested mode')
        require(row.get('ray_coverage_complete') is True and row.get('ray_pending_columns') == 0,
                'Captured hardware ray scene was incomplete')
        if mode == 'block-transport':
            assert_world_gi(row)
            require(row.get('block_transport_statistics_frame') == row.get('render_frame'),
                    'Transport readiness was measured on a different GPU frame')
            require(row.get('block_transport_active') is True, 'Block transport was silently inactive')
            require(row.get('block_transport_coverage_valid') is True,
                    'Block transport coverage was not certified')
            counters = row.get('block_transport_counters')
            require(isinstance(counters, list) and len(counters) == 12
                    and all(isinstance(value, int) and value >= 0 for value in counters),
                    'Block transport capture omitted its twelve actual GPU counters')
            require(counters[0] > 0 and counters[6] > 0,
                    'Block transport recorded no actual admission or transport rays')
            require(row.get('block_transport_gpu_bytes', 0) > 0
                    and row.get('block_transport_total_gpu_bytes', -1) >= row['block_transport_gpu_bytes'],
                    'Block transport resource accounting is absent or inconsistent')
        else:
            require(row.get('block_transport_active') is False,
                    'Comparison mode also executed block transport')
    if mode == 'block-transport':
        assert_world_gi_sequence(rows)


def compare_reference(args, result, poses, paths, regions):
    reference = json.loads((args.reference_case / 'result.json').read_text(encoding='utf-8'))
    require(reference.get('status') == 'captured', 'Reference is not a completed capture run')
    require(reference['initial_fixture']['sha256'] == result['initial_fixture']['sha256'],
            'Reference saved-world/settings/light fixture differs')
    require(reference['bundle_content'] == result['bundle_content']
            and reference['client_sha256'] == result['client_sha256'],
            'Comparison requires the exact same executable and complete shader/asset bundle')
    require(reference.get('process_priority') == args.process_priority,
            'Reference process priority differs')
    require(reference.get('debug') == 0 and reference.get('benchmark_seconds') == args.seconds,
            'Reference view or measurement duration differs')
    require(reference.get('fixed_hour') == args.fixed_hour, 'Reference lighting clock differs')
    require(reference.get('third_person', False) == args.third_person, 'Reference player presentation differs')
    require(len(reference['poses']) == len(poses)
            and all(same_pose(a['pose'], b['pose']) for a, b in zip(reference['poses'], poses)),
            'Reference actual camera poses differ')
    require(reference['fixture_metadata']['regions'] == regions, 'Reference ROI boxes differ')
    reference_paths = list(map(Path, reference['captures']))
    require(all(path.is_file() for path in reference_paths), 'Reference images are missing')
    require([digest(path) for path in reference_paths] == reference['capture_sha256'],
            'Reference captures changed after their recorded run')
    result.update(reference_case=str(args.reference_case.resolve()),
        comparison_scope='Same-build GI mode comparison. Direct light, materials, animation and time-of-day remain in display RGB; no radiometric ground truth.',
        matched_image_difference=compare(reference_paths[-1], paths[-1], regions))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', required=True, type=Path)
    parser.add_argument('--fixture', required=True, type=Path)
    parser.add_argument('--evidence-root', required=True, type=Path)
    parser.add_argument('--mode', required=True, choices=('block-transport', 'direct'))
    parser.add_argument('--reference-case', type=Path)
    parser.add_argument('--seconds', type=int, choices=range(8, 31), default=8)
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--process-priority', choices=('normal', 'below-normal'), default='normal')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--fixed-hour', type=float)
    parser.add_argument('--cache-values', action='store_true')
    parser.add_argument('--capture-count', type=int, default=3)
    parser.add_argument('--capture-stride', type=int, default=17)
    parser.add_argument('--min-frame', type=int, default=120)
    parser.add_argument('--third-person', action='store_true')
    args = parser.parse_args()
    require(20 <= args.timeout <= 300, 'Overall timeout must be between 20 and 300 seconds')
    require(args.fixed_hour is None or 0 <= args.fixed_hour < 24, 'Fixed hour must be in [0,24)')
    require(3 <= args.capture_count <= 16 and 1 <= args.capture_stride <= 120,
            'Capture count must be 3..16 and stride 1..120')
    require(120 <= args.min_frame <= 10000, 'Minimum capture frame must be 120..10000')
    bundle, source = args.client_bundle_root.resolve(), args.fixture.resolve()
    executable = bundle / 'Octaryn.Client.exe'
    require(executable.is_file(), f'Missing packaged client: {executable}')
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'block-transport-{args.mode}-', dir=args.evidence_root.resolve()))
    shutil.copytree(source / 'world', case / 'world')
    for name in ('settings.json', 'lighting.json', 'fixture.json'):
        shutil.copy2(source / name, case / name)
    fixture = json.loads((case / 'fixture.json').read_text(encoding='utf-8'))
    settings = json.loads((case / 'settings.json').read_text(encoding='utf-8'))
    require(settings['frameCapFps'] == 30 and settings['renderDistance'] == 4,
            'Authored fixture must preserve its 30 FPS cap and radius 4')
    require(settings.get('fullscreen') is False, 'Capture fixture must remain windowed and hidden')
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('OCTARYN_', 'VK_LAYER'))
           and key not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in (('WORLD', 'world'), ('SETTINGS', 'settings.json'),
                      ('LIGHTING', 'lighting.json'), ('INVENTORY', 'inventory.json'),
                      ('CAPTURE', 'frame.bmp'), ('PROFILE', 'profile.csv'),
                      ('FRAME_TIMING', 'frame-timing.csv'), ('GPU_PROFILE', 'gpu-profile.csv'),
                      ('LIGHTING_PROFILE', 'lighting-profile.csv')):
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_GRAPHICS_API='dx12', OCTARYN_CLIENT_RAY_TRACING='required',
        OCTARYN_CLIENT_RHI_VALIDATION='1', OCTARYN_CLIENT_UPSCALER='native',
        OCTARYN_CLIENT_CAPTURE_COUNT=str(args.capture_count), OCTARYN_CLIENT_CAPTURE_STRIDE=str(args.capture_stride),
        OCTARYN_CLIENT_CAPTURE_MIN_FRAME=str(args.min_frame), OCTARYN_CLIENT_LIGHTING_DEBUG='0',
        OCTARYN_CLIENT_LIVE_FRAME_TIMING='1', OCTARYN_CLIENT_GI=args.mode,
        OCTARYN_SERVER_START_HOUR='9')
    if args.fixed_hour is not None:
        env['OCTARYN_CLIENT_CAPTURE_FIXED_HOUR'] = str(args.fixed_hour)
    if args.cache_values:
        env['OCTARYN_CLIENT_CAPTURE_GI_CACHE'] = '1'
    command = [str(executable), '--benchmark-settings', '--benchmark-hidden',
               '--benchmark-seconds', str(args.seconds), '--render-distance', '4']
    if args.third_person:
        command.append('--third-person')
    result = dict(status='prepared', command=command, fixture=str(source),
        initial_fixture=fixture_digest(case), fixture_metadata=fixture,
        client_sha256=digest(executable), bundle_content=content_digest(bundle),
        mode=args.mode, debug=0, benchmark_seconds=args.seconds, hidden=True, frame_cap_fps=30,
        watchdog_max_frame_ms=50, watchdog_stall_seconds=2, watchdog_sustained_slow_seconds=1,
        process_priority=args.process_priority, warmup='world sweep and populated-cache completion',
        fixed_hour=args.fixed_hour, cache_values=args.cache_values, capture_count=args.capture_count,
        capture_stride=args.capture_stride, minimum_frame=args.min_frame,
        third_person=args.third_person,
        readiness_scope='Geometry/rays verified separately. World transport requires actual cache admissions/rays and completed world coverage. Warmup does not establish radiance convergence or visual quality.',
        visual_acceptance='pending actual image inspection',
        metric_scope='Display RGB with fixture-defined receiver boxes, optionally accompanied by exact linear world-cache dumps. Neither is an independent reference or automatic visual acceptance.',
        radius_32_status='Not tested here. Earlier radius-32 watchdog failures remain unqualified.')
    write_json(case / 'result.json', result)
    print(f'block_transport_capture_prepared mode={args.mode} evidence={case}', flush=True)
    if args.prepare_only:
        return
    try:
        with (case / 'client.log').open('wb') as log:
            code = run_capture(command, case, env, log, args.timeout, process_priority=args.process_priority)
        result.update(exit_code=code, runtime=runtime_summary(case))
        require(code == 0, f'Client exited {code}')
        require(not result['runtime']['failures'], 'Client log contains failures')
        require('full_columns_meshes_and_rays' in result['runtime']['milestones'],
                'Complete geometry and hardware-ray readiness was not recorded')
        result['authoritative_window'] = inspect_snapshot(case, 4)
        paths = [case / 'frame.bmp'] + [case / f'frame.bmp.sample-{index}.bmp' for index in range(1, args.capture_count)]
        require(all(path.is_file() for path in paths), 'All requested actual GPU screenshots are required')
        poses = capture_poses((case / 'client.log').read_text(encoding='utf-8', errors='replace'))
        require(len(poses) == args.capture_count and all(same_pose(pose['pose'], poses[0]['pose']) for pose in poses),
                'Capture camera changed; fixed-receiver measurements would be invalid')
        require(len({pose['frame'] % 2 for pose in poses}) == 2, 'Both renderer frame slots were not captured')
        counters = [json.loads(Path(str(path) + '.lighting.json').read_text(encoding='utf-8')) for path in paths]
        result.update(captures=list(map(str, paths)), capture_sha256=[digest(path) for path in paths],
                      poses=poses, counters=counters)
        capture_readiness(counters, args.mode)
        if args.fixed_hour is not None:
            require('lighting_capture_clock fixed=1' in (case / 'client.log').read_text(errors='replace'),
                    'Client did not acknowledge the fixed presentation clock')
            for field in ('sky_light_direction', 'sky_time'):
                require(all(row.get(field) == counters[0].get(field) and row.get(field) for row in counters),
                        'Captured solar lighting or celestial presentation clock changed')
        if args.cache_values and args.mode == 'block-transport':
            dumps = [Path(str(path) + '.gi.bin') for path in paths]
            result['linear_cache'] = [summarize_values(path, row) for path, row in zip(dumps, counters)]
            result['linear_cache_differences'] = [compare_values(a, b) for a, b in zip(dumps, dumps[1:])]
            result['linear_cache_sha256'] = [digest(path) for path in dumps]
        require(content_digest(bundle) == result['bundle_content'] and digest(executable) == result['client_sha256'],
                'Executable/shader bundle changed during capture; evidence is not one build')
        result['regions'] = measure(paths, fixture['regions'])
        if args.reference_case:
            compare_reference(args, result, poses, paths, fixture['regions'])
        result['status'] = 'captured'
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        if 'runtime' not in result and (case / 'client.log').is_file():
            try:
                result['runtime'] = runtime_summary(case)
            except (OSError, ValueError, KeyError, RuntimeError) as error:
                result['summary_error'] = str(error)
        write_json(case / 'result.json', result)
    print(f'block_transport_capture_complete mode={args.mode} evidence={case}', flush=True)


if __name__ == '__main__':
    main()
