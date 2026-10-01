"""Exercise real tiled upload, publication, camera priority, cancellation and eviction."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from capture_watchdog import run_capture
from case_evidence import record_build
from tile_input_evidence import record_inputs, outside_manifest
from tile_memory_report import cycle_memory
from capture_regions import read_bmp, save_png
from tile_performance_report import report as performance_report, hq200_activation
from performance_summary import read_profile
from tile_quality_evidence import quality_signature
from startup_readiness_report import readiness_activation


def texture_reuse_evidence(log, requested):
    expected = '1' if requested == 'on' else '0'
    markers = re.findall(r'^tile_texture_reuse enabled=([01]) publication=fence_complete cache_namespace=world_immutable$', log, re.M)
    if not markers or set(markers) != {expected}:
        raise ValueError('Missing or mismatched texture reuse activation marker')
    counters = re.findall(r'^tile_stream .* texture_reuses=(\d+) avoided_dds_bytes=(\d+)$', log, re.M)
    if not counters:
        raise ValueError('Missing owner texture reuse counters')
    hits, avoided = (max(int(row[i]) for row in counters) for i in (0, 1))
    if requested == 'off' and (hits or avoided):
        raise ValueError('Disabled texture reuse reported skipped DDS reads')
    return dict(enabled=requested == 'on', completed_asset_reuses=hits, avoided_dds_payload_bytes=avoided,
                encoded_source_key_hashing_retained=True)


def ray_policy_evidence(log, requested):
    policies = re.findall(r'^tile_ray_policy capacity=(\d+) max_operations=(\d+) soft_budget_ms=([\d.]+)$',log,re.M)
    if not policies or any(int(capacity)!=requested or int(operations)!=1 or float(ms)<=0
                           for capacity,operations,ms in policies):
        raise ValueError('Missing or mismatched AS lifecycle policy activation')
    observed = re.findall(r'^tile_ray_schedule .* inflight=(\d+) capacity=(\d+) ',log,re.M)
    if not observed or any(int(capacity)!=requested or int(active)>requested for active,capacity in observed):
        raise ValueError('Missing or mismatched actual AS lifecycle capacity')
    return dict(capacity=requested,max_operations_per_pump=1,soft_cpu_budget_ms=float(policies[0][2]),
                control='serialized new scheduler; not the previous algorithm' if requested==1 else 'pipelined scheduler')


def inspect(case, require_route=True, inputs=None, generated_fixture=True, dimensions=(2560, 1440)):
    log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
    for marker in ('authoritative_player_ready eye=', 'tile_published ',
                   'open_world_exit mode=map code=0', 'world_capture frame=', 'world_capture_tiles frame='):
        if marker not in log:
            raise RuntimeError(f'Missing tile runtime evidence: {marker}')
    errors = re.findall(r'^.*(?:World frame failed|Map startup timed out|tile_\w+_failed|'
                        r'budget_exceeded|rhi_validation severity=error|Validation Error|VUID-|'
                        r'D3D12 ERROR|D3D12 CORRUPTION).*$', log, re.MULTILINE)
    if errors:
        raise RuntimeError('\n'.join(errors[:8]))
    uploads = re.findall(r'tile_upload bytes=(\d+) budget=(\d+) cpu_ms=([\d.]+)', log)
    if not uploads or any(int(actual) > int(budget) for actual, budget, _ in uploads):
        raise RuntimeError('Missing or over-budget staged uploads')
    states = [dict((key, float(value) if '.' in value else int(value)) for key, value in re.findall(r'(\w+)=(-?\d+(?:\.\d+)?)', line))
              for line in log.splitlines() if line.startswith('tile_stream ')]
    published = set(map(int, re.findall(r'tile_published id=(\d+)', log)))
    authority_logs = [case / 'world/logs/server/local-session.log', case / 'world/logs/server.log']
    authority_text = '\n'.join(path.read_text(encoding='utf-8', errors='replace')
                               for path in authority_logs if path.is_file())
    authority_markers = re.findall(r'^.*server_collision_tiles .*authority_streaming=([01]).*$',
                                   authority_text, re.MULTILINE)
    authority_mode = int(authority_markers[-1]) if authority_markers else None
    result = dict(published_tiles=sorted(published), state_samples=len(states),
                  upload_total_bytes=sum(int(row[0]) for row in uploads),
                  max_upload_bytes=max(int(row[0]) for row in uploads),
                  max_upload_cpu_ms=max(float(row[2]) for row in uploads),
                  peak_accounted_bytes=max((s['resident_bytes'] + s['reserved_bytes'] +
                                            s['retired_bytes'] + s.get('texture_bytes', 0) for s in states), default=0),
                  evictions=max((s['evicted'] for s in states), default=0),
                  cancellations=max((s['cancelled'] for s in states), default=0),
                  authority_streaming=authority_mode,
                  authority_collision='bounded residency active; authority traversal unqualified' if authority_mode == 1 else
                      'preloaded; dynamic server residency unqualified' if authority_mode == 0 else
                      'unknown; no isolated server residency marker',
                  authority_collision_logs=[str(path.relative_to(case)) for path in authority_logs if path.is_file()])
    image = case / 'frame.bmp'
    _, _, width, height, _ = read_bmp(image)
    lighting = json.loads((case / 'frame.bmp.lighting.json').read_text())
    observation = json.loads((case / 'frame.bmp.observation.json').read_text())
    captured_frame = int(lighting['render_frame'])
    capture_states = [dict((key, int(value)) for key, value in re.findall(r'(\w+)=(\d+)', line))
                      for line in log.splitlines() if line.startswith('world_capture_tiles ')]
    capture_state = next((state for state in capture_states if state['frame'] == captured_frame), None)
    if not capture_state or not capture_state['resident'] or capture_state['preparing'] or capture_state['uploading']:
        raise RuntimeError('Missing complete tiled capture residency evidence')
    if (width, height) != tuple(dimensions):
        raise RuntimeError(f'Unexpected tile capture dimensions: {width}x{height}')
    save_png(image, case / 'frame.png')
    result['capture'] = dict(bmp='frame.bmp', png='frame.png', width=width, height=height,
                             lighting=lighting, observation=observation, residency=capture_state,
                             timing_qualification=False)
    if require_route:
        rows = list(csv.DictReader((case / 'tile-route.csv').open()))
        captured_row = next((row for row in rows if int(row['frame']) == captured_frame), None)
        if not captured_row or captured_row['phase'] not in ('warmup', 'origin'):
            raise RuntimeError('Tile capture did not occur during a settled origin hold')
        result['capture']['route'] = captured_row
        result['traversal_elapsed_seconds'] = max(float(row['route_elapsed_seconds']) for row in rows)
        phases = {row['phase'] for row in rows}
        if not {'cut', 'outbound', 'far', 'inbound', 'origin'} <= phases:
            raise RuntimeError('Tile route did not complete its motion phases')
        if max(float(row['eye_x']) for row in rows) - min(float(row['eye_x']) for row in rows) < 380:
            raise RuntimeError('Camera did not cross residency boundaries')
        if max(abs(float(row['authority_x']) - float(rows[0]['authority_x'])) for row in rows) > 1:
            raise RuntimeError('Renderer-only route unexpectedly moved authority')
        if generated_fixture and (len(published) < 17 or not result['evictions'] or not result['cancellations']):
            raise RuntimeError(f'Tile route did not exercise all residency transitions: {result}')
        result['residency_transitions_qualified'] = bool(result['evictions'] and result['cancellations'])
        if inputs:
            bounds = inputs['manifest']['tiles']
            result['camera_outside_map_frames'] = sum(outside_manifest(row, bounds) for row in rows)
            result['camera_inside_map_frames'] = len(rows) - result['camera_outside_map_frames']
            result['route_scope'] = '384m camera route; out-of-map views are intentional and are not missing-geometry evidence'
            result['published_fraction_of_manifest'] = len(published) / inputs['tile_count']
        result['completed_cycles'] = max(int(row['cycle']) for row in rows)
        # Session includes startup pumps, so its frame count is not the render count.
        # Compare the same origin pose after eviction, with no queued GPU work.
        origin_x = float(rows[0]['eye_x'])
        settled = [s for s in states if abs(s.get('camera_x', 1e20)-origin_x)<.002 and s['evicted'] > 0
                   and not s['preparing'] and not s['uploading'] and not s['retired_bytes']]
        if settled:
            # Hysteresis can retain different tile counts at the same camera pose.
            count = settled[-1]['resident']
            settled = [s for s in settled if s['resident'] == count]
        result['settled_accounted_bytes'] = [s['resident_bytes'] + s['reserved_bytes'] + s['retired_bytes'] + s.get('texture_bytes', 0)
                                             for s in settled]
        samples = result['settled_accounted_bytes']
        result['memory_plateau_qualified'] = len(samples) >= 3
        if len(samples) >= 3 and max(samples[1:]) > samples[0] + 1024 * 1024:
            raise RuntimeError('Settled accounted tile memory grew by more than 1 MiB')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--performance-profile', choices=('custom', 'HQ200'), default='custom')
    parser.add_argument('--width', type=int, default=2560)
    parser.add_argument('--height', type=int, default=1440)
    parser.add_argument('--uncapped-fps', action='store_true')
    parser.add_argument('--frame-cpu-trace', action='store_true')
    parser.add_argument('--startup-readiness', action='store_true',
                        help='Opt-in diagnostic post-present readiness scans; CPU cost included, app-main clock only')
    parser.add_argument('--fixed-sampling', action='store_true', help='Native quality only; lock ready-frame ray phases and presentation delta')
    parser.add_argument('--camera-origin', help='Locked quality camera x,y,z,yaw,pitch')
    parser.add_argument('--capture-ready-frame', type=int, default=120)
    parser.add_argument('--require-all-tiles', action='store_true')
    parser.add_argument('--require-residency-transitions', action='store_true')
    parser.add_argument('--manifest', type=Path, help='Use an existing cooked tiled world instead of the generated fixture')
    parser.add_argument('--gpu-budget-mib', type=int, default=2048)
    parser.add_argument('--collision-budget-mib', type=int, default=512)
    parser.add_argument('--draw-mode', choices=('direct', 'indirect', 'meshlet'), default='direct')
    parser.add_argument('--texture-reuse', choices=('on', 'off'), default='on')
    parser.add_argument('--as-inflight', type=int, choices=(1,4), default=4)
    parser.add_argument('--frames', type=int, default=2220)
    parser.add_argument('--seconds', type=int, default=0, help='Use elapsed duration instead of frame limit')
    parser.add_argument('--route-warmup-frames', type=int, default=1200,
                        help='Initial origin hold for full residency and capture; excluded from traversal seconds')
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--smoke', action='store_true', help='Startup only; no residency-transition qualification')
    parser.add_argument('--rhi-validation', action='store_true')
    parser.add_argument('--max-frame-ms', type=float, default=50)
    args = parser.parse_args()
    origin = None
    if args.camera_origin:
        try:
            origin = [float(value) for value in args.camera_origin.split(',')]
        except ValueError:
            parser.error('Camera origin must contain five finite numbers')
        if len(origin) != 5 or not all(math.isfinite(value) for value in origin):
            parser.error('Camera origin must contain five finite numbers')
    if args.fixed_sampling and (not args.smoke or args.performance_profile != 'custom' or
                                (args.width, args.height) != (2560, 1440) or origin is None):
        parser.error('Fixed tiled quality requires startup-only custom native1440 and an explicit camera origin')
    if origin is not None and not args.fixed_sampling:
        parser.error('Locked tiled camera is available only for fixed quality sampling')
    if not 120 <= args.capture_ready_frame <= 10000 or (not args.seconds and args.frames <= args.capture_ready_frame):
        parser.error('Capture ready frame must be120..10000 and precede termination')
    if args.width < 320 or args.height < 180:
        parser.error('Capture dimensions must be at least 320x180')
    if args.performance_profile == 'HQ200' and (args.width, args.height) != (2560, 1440):
        parser.error('HQ200 requires the agreed 2560x1440 output')
    if args.performance_profile == 'HQ200' and not args.manifest:
        parser.error('HQ200 requires an explicit cooked tiled manifest; the generated fixture is uncooked')
    if args.smoke and args.require_residency_transitions:
        parser.error('Startup-only smoke cannot qualify residency transitions')
    if args.frames < 300 or args.seconds < 0 or args.timeout <= args.seconds:
        parser.error('Need >=300 frames and a watchdog longer than the requested duration')
    if not 180 <= args.route_warmup_frames <= 10000:
        parser.error('Route warmup must be 180 through 10000 frames')
    if not args.smoke and not args.seconds and args.frames < args.route_warmup_frames + 960:
        parser.error('A full residency route requires warmup plus 960 frames')
    if not 64 <= args.gpu_budget_mib <= 32768:
        parser.error('GPU budget must be 64 through 32768 MiB')
    if not 64 <= args.collision_budget_mib <= 4096:
        parser.error('Collision estimate budget must be 64 through 4096 MiB')
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'tiles-{args.backend}-', dir=args.evidence_root.resolve()))
    (case / 'world').mkdir()
    if args.manifest:
        manifest_path = args.manifest.resolve()
    else:
        subprocess.run([sys.executable, str(Path(__file__).with_name('make_tile_fixture.py')),
                        str(case / 'world'), '--tiles', '17'], check=True)
        manifest_path = case / 'world/map.json'
    inputs = record_inputs(manifest_path, case / 'tile-inputs.json')
    settings = dict(version=16, windowWidth=args.width, windowHeight=args.height, fullscreen=False,
                    renderDistance=4, upscalerMode=6 if args.performance_profile == 'HQ200' else 0,
                    fsrRenderScale=.5, reflectionDistance=1024, pbrEnabled=True,
                    fogEnabled=False, rayTracingEnabled=True, reflectionQuality=2, shadowQuality=2)
    (case / 'settings.json').write_text(json.dumps(settings))
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('OCTARYN_', 'VK_LAYER'))
           and key not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in (('WORLD', 'world'), ('SETTINGS', 'settings.json'), ('LIGHTING', 'lighting.json'),
                      ('INVENTORY', 'inventory.json'), ('FRAME_TIMING', 'frame-timing.csv')):
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_MAP_MODE='1', OCTARYN_CLIENT_GRAPHICS_API=args.backend,
               OCTARYN_CLIENT_PERFORMANCE_PROFILE=args.performance_profile,
               OCTARYN_CLIENT_SERVER_LOG_DIR=str(case / 'world/logs/server'),
               OCTARYN_CLIENT_MAP_MANIFEST=str(manifest_path),
               OCTARYN_CLIENT_MAP_DRAW_MODE=args.draw_mode,
               OCTARYN_CLIENT_TILE_TEXTURE_REUSE='1' if args.texture_reuse == 'on' else '0',
               OCTARYN_CLIENT_TILE_AS_INFLIGHT=str(args.as_inflight),
               OCTARYN_CLIENT_TILE_GPU_BUDGET_MIB=str(args.gpu_budget_mib),
               OCTARYN_SERVER_COLLISION_BUDGET_MIB=str(args.collision_budget_mib),
               OCTARYN_CLIENT_RAY_TRACING='required', OCTARYN_CLIENT_LIVE_FRAME_TIMING='1',
               OCTARYN_CLIENT_MAP_DRAW_DIAGNOSTICS='1', OCTARYN_CLIENT_CAPTURE_PATH=str(case / 'frame.bmp'),
               OCTARYN_CLIENT_CAPTURE_COUNT='1', OCTARYN_CLIENT_CAPTURE_MIN_FRAME='120',
               OCTARYN_CLIENT_CAPTURE_READY_FRAME=str(args.capture_ready_frame))
    env.update(OCTARYN_CLIENT_GPU_PROFILE_PATH=str(case / 'gpu.csv'),
               OCTARYN_CLIENT_LIGHTING_PROFILE_PATH=str(case / 'lighting.csv'))
    if args.uncapped_fps:
        env['OCTARYN_CLIENT_BENCHMARK_UNCAPPED'] = '1'
    if args.frame_cpu_trace:
        env['OCTARYN_CLIENT_FRAME_CPU_TRACE'] = '1'
    if args.startup_readiness:
        env['OCTARYN_CLIENT_STARTUP_READINESS'] = '1'
    if args.fixed_sampling:
        env.update(OCTARYN_CLIENT_FIXED_SAMPLING='1', OCTARYN_CLIENT_MAP_CAMERA_MOTION='static',
                   OCTARYN_CLIENT_MAP_CAMERA_ORIGIN=','.join(map(str, origin)),
                   OCTARYN_CLIENT_MAP_CAMERA_MOTION_PATH=str(case / 'camera-motion.csv'))
    if not args.smoke:
        env.update(OCTARYN_CLIENT_TILE_CAMERA_ROUTE='1',
                   OCTARYN_CLIENT_TILE_CAMERA_WARMUP_FRAMES=str(args.route_warmup_frames),
                   OCTARYN_CLIENT_TILE_CAMERA_ROUTE_PATH=str(case / 'tile-route.csv'))
    if args.rhi_validation:
        env['OCTARYN_CLIENT_RHI_VALIDATION'] = '1'
    command = [str(bundle / 'Octaryn.Client.exe'), '--benchmark-hidden', '--benchmark-settings']
    command += ['--benchmark-seconds', str(args.seconds)] if args.seconds else ['--frames', str(args.frames)]
    record_build(bundle, case)
    result = dict(status='running', backend=args.backend, command=command, renderer_only_route=True,
                  performance_profile=args.performance_profile, dimensions=[args.width, args.height],
                  uncapped_fps=args.uncapped_fps, frame_cpu_trace=args.frame_cpu_trace,
                  startup_readiness_requested=args.startup_readiness,
                  fixed_sampling=args.fixed_sampling, camera_origin=origin,
                  capture_ready_frame=args.capture_ready_frame,
                  rhi_validation=args.rhi_validation, max_frame_ms=args.max_frame_ms,
                  workload='startup only' if args.smoke else '384m frame-driven extreme streaming stress',
                  normal_gameplay_budget_qualified=False,
                  require_all_tiles=args.require_all_tiles,
                  require_residency_transitions=args.require_residency_transitions,
                  authority_movement_qualified=False, generated_fixture=not bool(args.manifest),
                  manifest=str(manifest_path), tile_count=inputs['tile_count'], gpu_budget_mib=args.gpu_budget_mib,
                  collision_estimate_budget_mib=args.collision_budget_mib,
                  draw_mode=args.draw_mode, texture_reuse=args.texture_reuse, as_inflight=args.as_inflight,
                  requested_seconds=args.seconds,
                  route_warmup_frames=args.route_warmup_frames, image_capture_required=True)
    print(f'tile_capture_started evidence={case}', flush=True)
    try:
        with (case / 'client.log').open('wb') as log:
            result['exit_code'] = run_capture(command, case, env, log, args.timeout,
                                               max_frame_ms=args.max_frame_ms)
        if result['exit_code']:
            raise RuntimeError(f'Tile capture exited {result["exit_code"]}')
        result['evidence'] = inspect(case, not args.smoke, inputs, not bool(args.manifest),
                                     (args.width, args.height))
        log_text = (case / 'client.log').read_text(errors='replace')
        result['startup_readiness_activation'] = readiness_activation(log_text, args.startup_readiness)
        result['texture_reuse_activation'] = texture_reuse_evidence(log_text, args.texture_reuse)
        result['as_lifecycle_activation'] = ray_policy_evidence(log_text,args.as_inflight)
        if args.performance_profile == 'HQ200':
            result['profile_activation'] = hq200_activation(read_profile(case / 'frame-timing.csv'))
        if args.fixed_sampling:
            result['quality_signature'] = quality_signature(case, result['evidence'],
                read_profile(case / 'frame-timing.csv'), args.capture_ready_frame, origin, inputs['tile_count'],
                (case / 'client.log').read_text(errors='replace'))
        if args.require_all_tiles and result['evidence']['published_tiles'] != list(range(inputs['tile_count'])):
            raise RuntimeError('Not every manifest tile was published during the workload')
        if args.require_residency_transitions and not result['evidence']['residency_transitions_qualified']:
            raise RuntimeError('Route did not exercise both cancellation and eviction')
        if not args.smoke:
            if args.seconds and result['evidence']['traversal_elapsed_seconds'] < args.seconds:
                raise RuntimeError('Observed traversal duration excludes warmup and did not reach requested seconds')
            result['process_memory'] = cycle_memory(case)
            if args.seconds >= 1800 and not result['process_memory']['passed']:
                raise RuntimeError('Traversal process/GPU memory plateau did not qualify')
        result['status'] = 'passed'
    except Exception as error:
        result['status'] = 'failed'
        result['error'] = str(error)
    try:
        result['performance'] = performance_report(case)
    except Exception as error:
        result['performance_error'] = str(error)
        result['status'] = 'failed'
    (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
