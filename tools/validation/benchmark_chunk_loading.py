"""Measure hidden, capped startup and exact column loading with an owned watchdog.

Every run gets its own world and settings. Optional source files are copied,
never opened by the client in place. Residency milestones are column counts;
they do not prove which near-camera coordinates have become visible.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import time

from benchmark_presentation import content_digest, digest, distribution, require
from capture_watchdog import run_capture
from benchmark_route import resolve_origin, origin_arguments, verify_origin


def durable_world_file(name):
    return name in {'world_generation.json', 'world_blocks.json', 'world_items.bin',
                    'world_time.json', 'world_meta.json'} or bool(re.fullmatch(
                        r'(?:chunk_-?\d+_-?\d+|player_-?\d+)\.json', name))


def fixture_digest(case):
    paths = [case / 'settings.json', case / 'lighting.json']
    paths += sorted(path for path in (case / 'world').iterdir() if path.is_file())
    entries = {path.relative_to(case).as_posix(): digest(path) for path in paths}
    return dict(sha256=hashlib.sha256(json.dumps(entries, sort_keys=True).encode()).hexdigest(),
                files=entries)


def read_rows(path):
    if not path.is_file():
        return []
    with path.open(newline='', encoding='utf-8-sig') as source:
        return [row for row in csv.DictReader(source) if None not in row and None not in row.values()]


def failure_lines(log):
    # Match error words/markers, not fields such as admission_failures=4708.
    failure = re.compile(r'\b(?:\w+_)?(?:failed|failure)\b|\btimed out\b|'
                         r'\b(?:\w+_)?timeout\b(?!\s*=\s*\d)|Validation Error:|'
                         r'\brhi_validation\b[^\r\n]*\bseverity=error\b', re.I)
    return [line for line in log.splitlines() if failure.search(line)]


def summarize(case, radius):
    rows = read_rows(case / 'frame-timing.csv')
    total = 0.0
    previous = 0.0
    milestones = {}
    expected = (2 * radius + 1) ** 2
    thresholds = sorted({count for count in (1, 9, 81, 289, 1089, expected) if count <= expected})
    for row in rows:
        milliseconds = float(row['total_ms'])
        require(math.isfinite(milliseconds) and milliseconds >= 0, 'Invalid frame timing')
        total += milliseconds / 1000
        count = int(row['columns'])
        for threshold in thresholds:
            if count >= threshold:
                milestones.setdefault(f'columns_{threshold}', dict(
                    frame=int(row['frame']), seconds_bounds=[previous, total]))
        if count == expected and int(row['pending_meshes']) == 0:
            milestones.setdefault('full_columns_and_meshes', dict(
                frame=int(row['frame']), seconds_bounds=[previous, total]))
            if int(row['ray_pending']) == 0:
                milestones.setdefault('full_columns_meshes_and_rays', dict(
                    frame=int(row['frame']), seconds_bounds=[previous, total]))
                if int(row.get('gi_ready', -1)) == 1:
                    milestones.setdefault('full_columns_meshes_rays_and_lighting', dict(
                        frame=int(row['frame']), seconds_bounds=[previous, total]))
        previous = total
    result = dict(expected_columns=expected, observed_frames=len(rows), milestones=milestones,
                  milestone_clock='Cumulative completed world-frame time, excluding renderer initialization',
                  milestone_scope='Resident column counts; individual loaded coordinates are not recorded')
    if rows:
        result.update(last_columns=int(rows[-1]['columns']),
                      last_pending_meshes=int(rows[-1]['pending_meshes']),
                      last_ray_pending=int(rows[-1]['ray_pending']),
                      world_frame_seconds=total,
                      maximum_pending_meshes=max(int(row['pending_meshes']) for row in rows),
                      frame_ms=distribution([float(row['total_ms']) for row in rows]))
        if 'gi_ready' in rows[-1]:
            result['last_gi_ready'] = int(rows[-1]['gi_ready']) == 1
    log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
    priority = re.search(r'capture_process_priority requested=(\S+) actual=(0x[0-9a-f]+) confirmed=(\d+)', log)
    if priority:
        result['process_priority_class'] = dict(requested=priority[1], actual=int(priority[2], 16),
                                               confirmed=priority[3] == '1')
    result['failures'] = failure_lines(log)
    result['block_transport_statistics'] = [line for line in log.splitlines()
                                            if line.startswith('block_transport_statistics ')]
    boot = re.search(r'client_boot stage=renderer_ready elapsed_ms=(\d+)', log)
    if boot:
        result['renderer_ready_ms'] = int(boot[1])
    device = re.search(r'world_device backend=slang_rhi api=(\S+) adapter=([^\r\n]+)', log)
    if device:
        result.update(api=device[1], adapter=device[2])
    final = re.search(r'open_world_exit code=(\d+) frames=(\d+) columns=(\d+) quads=(\d+) gpu_bytes=(\d+)', log)
    if final:
        result['completion'] = dict(zip(('code', 'frames', 'columns', 'quads', 'gpu_bytes'), map(int, final.groups())))
    pacing = re.search(r'frame_pacing_summary[^\r\n]+', log)
    if pacing:
        result['frame_pacing_summary'] = pacing[0]
    return result


def route_summary(case, args):
    rows = read_rows(case / 'stream-profile.csv')
    origin = verify_origin(rows, args.route_origin, args.speed * args.seconds)
    log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
    marker = re.search(r'world_benchmark measurement=start frame=(\d+)', log)
    require(marker is not None, 'Moving camera produced no measurement boundary')
    first_frame = int(marker[1])
    # The boundary frame renders before recording starts, then receives the moving label.
    moving = [row for row in rows if row['phase'] == 'moving' and int(row['frame']) > first_frame]
    initial = [row for row in rows if int(row['frame']) <= first_frame]
    settled = [row for row in rows if row['phase'] == 'settle']
    require(len(moving) >= 2, 'Moving camera produced no usable frames')
    span = float(moving[-1]['time_seconds']) - float(moving[0]['time_seconds'])
    distance = float(moving[0]['camera_z']) - float(moving[-1]['camera_z'])
    require(span > 0 and abs(distance - args.speed * span) < .2, 'Camera route does not match requested speed')
    require(settled and int(settled[-1]['columns']) == (2 * args.radius + 1) ** 2
            and int(settled[-1]['pending_meshes']) == 0 and int(settled[-1]['ray_pending']) == 0
            and int(settled[-1].get('gi_ready', -1)) == 1,
            'Moving route did not settle geometry, rays, and world transport completely')
    require(initial, 'Moving camera produced no initial position')
    full_distance = float(initial[-1]['camera_z']) - float(settled[-1]['camera_z'])
    require(abs(full_distance - args.speed * args.seconds) < .2,
            'Camera endpoint does not match requested route length')
    return dict(scope='Explicit linear render-camera and stream request; authoritative player remains stationary',
                origin=origin, measurement_boundary_frame=first_frame, sampled_frames=len(moving),
                span_seconds=span, distance_metres=distance, full_distance_metres=full_distance,
                centers=len({(row['center_x'], row['center_z']) for row in moving}),
                minimum_columns=min(int(row['columns']) for row in moving),
                maximum_pending_meshes=max(int(row['pending_meshes']) for row in moving),
                gi_unready_frames=sum(int(row['gi_ready']) != 1 for row in moving),
                frame_ms=distribution([float(row['frame_ms']) for row in moving]))


def inspect_snapshot(case, radius):
    snapshot = (case / 'world/runtime/chunk_stream.json.bin').read_bytes()
    require(len(snapshot) >= 128 and snapshot[:8] == b'OCSTRM01'
            and struct.unpack_from('<I', snapshot, 8)[0] == 3, 'Unexpected stream snapshot format')
    x, z, saved_radius = struct.unpack_from('<iiI', snapshot, 28)
    count, edits = struct.unpack_from('<II', snapshot, 120)
    require(saved_radius == radius and count == (2 * radius + 1) ** 2, 'Incomplete authoritative window')
    require(len(snapshot) == 128 + 24 * count + 14 * edits, 'Invalid stream snapshot length')
    actual = {struct.unpack_from('<ii', snapshot, 128 + 24 * index) for index in range(count)}
    expected = {(cx, cz) for cx in range(x - radius, x + radius + 1)
                for cz in range(z - radius, z + radius + 1)}
    require(actual == expected, 'Authoritative stream identities differ from the requested window')
    return dict(center=[x, z], radius=radius, columns=count, edits=edits, identities='complete exact window')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--gi-mode', choices=('block-transport', 'direct'), default='block-transport',
                        help='World transport, or explicit direct-only geometry qualification')
    parser.add_argument('--radius', type=int, choices=(4, 8, 12, 16, 20, 24, 32), default=16)
    parser.add_argument('--settings', type=Path, help='Copy saved user settings into the isolated fixture')
    parser.add_argument('--lighting', type=Path, help='Copy saved lighting into the isolated fixture')
    parser.add_argument('--world', type=Path, help='Copy persistent world files; omit generated runtime files')
    parser.add_argument('--width', type=int)
    parser.add_argument('--height', type=int)
    parser.add_argument('--seconds', type=float, default=2, help='Settled measurement seconds, or camera route duration')
    parser.add_argument('--speed', type=float, default=0, help='Optional camera-only linear route speed in metres/sec')
    parser.add_argument('--route-origin', type=float, nargs=3, metavar=('X', 'Y', 'Z'),
                        help='Absolute render eye; otherwise use copied player_1.json eye plus 48 m')
    parser.add_argument('--timeout', type=float, default=240)
    parser.add_argument('--process-priority', choices=('normal', 'below-normal'), default='below-normal',
                        help='Windows process class; normal matches ordinary launch')
    parser.add_argument('--ray-tracing', choices=('settings', 'on', 'off'), default='settings')
    parser.add_argument('--cpu-stage-trace', action='store_true', help='Opt in to slow CPU-stage wall/thread timing diagnostics')
    parser.add_argument('--capture', action='store_true', help='Read back a settled world screenshot for visual inspection')
    parser.add_argument('--boot-capture', action='store_true', help='Read back the loading screen; adds diagnostic startup cost')
    args = parser.parse_args()
    require(math.isfinite(args.seconds) and 1 <= args.seconds <= 120, '--seconds must be 1..120')
    require(math.isfinite(args.speed) and (args.speed == 0 or 1 <= args.speed <= 120),
            '--speed must be 0 (stationary) or 1..120')
    require(args.route_origin is None or args.speed > 0, '--route-origin requires --speed')
    require(math.isfinite(args.timeout) and 10 <= args.timeout <= 600, '--timeout must be 10..600')
    require(not args.speed or args.seconds * args.speed >= 64, 'Camera route must traverse at least two columns')
    require(args.gi_mode != 'block-transport' or args.ray_tracing != 'off',
            'World transport requires hardware ray tracing')
    bundle = args.client_bundle_root.resolve()
    executable = bundle / ('Octaryn.Client.exe' if os.name == 'nt' else 'Octaryn.Client')
    require(executable.is_file(), f'Missing packaged client: {executable}')
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'chunks-{args.backend}-r{args.radius}-', dir=args.evidence_root.resolve()))
    (case / 'world').mkdir()
    if args.world:
        require(args.world.is_dir(), f'Missing source world: {args.world}')
        for source in args.world.iterdir():
            if source.is_file() and durable_world_file(source.name):
                shutil.copy2(source, case / 'world' / source.name)
    args.route_origin = resolve_origin(case / 'world', args.route_origin, args.speed * args.seconds, args.radius) if args.speed else None
    settings = json.loads(args.settings.read_text(encoding='utf-8-sig')) if args.settings else dict(
        version=15, windowWidth=1280, windowHeight=720, upscalerMode=1)
    settings.update(fullscreen=False, renderDistance=args.radius, frameCapFps=30)
    if args.width:
        settings['windowWidth'] = args.width
    if args.height:
        settings['windowHeight'] = args.height
    lighting = json.loads(args.lighting.read_text(encoding='utf-8-sig')) if args.lighting else dict(
        version=1, ambient_strength=.65, sun_strength=.75, sun_fallback_strength=.75,
        fog_distance=1024, skylight_floor=.25)
    (case / 'settings.json').write_text(json.dumps(settings, indent=2), encoding='utf-8')
    (case / 'lighting.json').write_text(json.dumps(lighting, indent=2), encoding='utf-8')
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('OCTARYN_', 'VK_LAYER'))
           and key not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    overrides = {f'OCTARYN_CLIENT_{key}_PATH': str(case / name) for key, name in (
        ('WORLD', 'world'), ('SETTINGS', 'settings.json'), ('LIGHTING', 'lighting.json'),
        ('INVENTORY', 'inventory.json'), ('PROFILE', 'profile.csv'),
        ('FRAME_TIMING', 'frame-timing.csv'), ('GPU_PROFILE', 'gpu-profile.csv'),
        ('STREAM_PROFILE', 'stream-profile.csv'), ('LIGHTING_PROFILE', 'lighting-profile.csv'))}
    overrides.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend, OCTARYN_CLIENT_LIVE_FRAME_TIMING='1',
                     OCTARYN_CLIENT_GI=args.gi_mode)
    if args.ray_tracing != 'settings':
        overrides['OCTARYN_CLIENT_RAY_TRACING'] = 'required' if args.ray_tracing == 'on' else 'off'
    if args.cpu_stage_trace:
        overrides['OCTARYN_CLIENT_FRAME_CPU_TRACE'] = '1'
    if args.capture:
        overrides['OCTARYN_CLIENT_CAPTURE_PATH'] = str(case / 'frame.bmp')
        overrides['OCTARYN_CLIENT_CAPTURE_GEOMETRY'] = '0'
    if args.boot_capture:
        overrides['OCTARYN_CLIENT_BOOT_CAPTURE_PATH'] = str(case / 'startup-loading.bmp')
    env.update(overrides)
    command = [str(executable), '--benchmark-settings', '--benchmark-hidden', '--benchmark-seconds',
               str(args.seconds), '--render-distance', str(args.radius)]
    if args.speed:
        command += ['--benchmark-streaming-speed', str(args.speed), *origin_arguments(args.route_origin)]
    report = dict(status='running', evidence=str(case), command=command, settings=settings, lighting=lighting,
                  source_settings=str(args.settings) if args.settings else None,
                  source_world=str(args.world) if args.world else None, route_origin=args.route_origin,
                  client_sha256=digest(executable), bundle_content=content_digest(bundle),
                  initial_fixture=fixture_digest(case),
                  environment_overrides=overrides, requested_backend=args.backend, gi_mode=args.gi_mode, radius=args.radius,
                  hidden=True, frame_cap_fps=30, watchdog_max_frame_ms=50, process_priority=args.process_priority,
                  cpu_stage_trace=args.cpu_stage_trace, capture_requested=args.capture, visual_acceptance='Pending human/model inspection')
    path = case / 'result.json'
    path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'chunk_loading_started evidence={case}', flush=True)
    started = time.monotonic()
    try:
        with (case / 'client.log').open('wb') as log:
            report['exit_code'] = run_capture(command, case, env, log, args.timeout,
                                              process_priority=args.process_priority)
        report.update(summarize(case, args.radius))
        require(report['exit_code'] == 0, f"Client exited with {report['exit_code']}")
        require(not report['failures'], 'Client reported failure; inspect client.log')
        require(report.get('api') == ('D3D12' if args.backend == 'dx12' else 'Vulkan'), 'Requested backend was not used')
        require('full_columns_meshes_rays_and_lighting' in report['milestones'],
                'Initial world did not finish geometry, rays, and world transport')
        require(report.get('completion', {}).get('code') == 0, 'Client did not report clean completion')
        require(report['completion']['columns'] == (2 * args.radius + 1) ** 2, 'Final rendered window is incomplete')
        require(report.get('last_gi_ready') is True, 'Final world transport is incomplete')
        report['authoritative_snapshot'] = inspect_snapshot(case, args.radius)
        require(digest(executable) == report['client_sha256'] and content_digest(bundle) == report['bundle_content'],
                'Bundle changed during measurement')
        if args.speed:
            report['moving_camera'] = route_summary(case, args)
        if args.capture:
            require((case / 'frame.bmp').is_file(), 'Requested settled GPU screenshot was not captured')
            report['capture'] = str(case / 'frame.bmp')
        report['status'] = 'passed'
    except Exception as error:
        report.update(status='failed', error=str(error))
        if (case / 'client.log').exists():
            report.update(summarize(case, args.radius))
        raise
    finally:
        report['wall_seconds'] = time.monotonic() - started
        path.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'chunk_loading_passed evidence={case}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f'chunk_loading_failed: {error}', file=sys.stderr)
        sys.exit(1)
