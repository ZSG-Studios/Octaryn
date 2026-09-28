"""Capture the bundled GLB map with isolated settings and GPU evidence."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import tempfile

from case_evidence import record_build
from asset_evidence import cooked_asset_identity
from capture_watchdog import run_capture
from performance_summary import summarize_case, distribution
from capture_render_options import (map_only_traversal, add_render_options, resolve_render_options,
                                    apply_render_options, render_option_evidence, inspect_render_options)
from capture_ray_counter_mode import add_counter_option, resolve_counter_mode, apply_counter_mode, inspect_counter_mode
from capture_reflection_wave import add_wave_option, resolve_wave_mode, apply_wave_mode, inspect_wave_mode
from capture_gpu_counters import (add_gpu_counter_options, resolve_gpu_counters, prepare_gpu_counters,
                                  inspect_gpu_counters, join_gpu_counter_frame)
from capture_tile_options import add_tile_options, prepare_tile_options, inspect_tile_options

QUALITY_TIERS = ('low', 'medium', 'high', 'ultra')


def validate_capture_schedule(frames, first_capture, captures, stride):
    # Map sessions keep readback disabled until the authoritative view warms up.
    first_possible = max(180, first_capture)
    if captures == 0:
        if frames < 180:
            raise ValueError('Performance runs require at least 180 frames')
        return
    if captures < 1 or stride < 1 or frames <= first_possible + (captures - 1) * stride:
        raise ValueError('--frames must leave time for all captures after map warmup (at least 180 frames)')


def quality_settings(reflections, shadows):
    return dict(reflectionQuality=QUALITY_TIERS.index(reflections),
                shadowQuality=QUALITY_TIERS.index(shadows))


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def capture_dimensions(path):
    with path.open('rb') as image:
        header = image.read(54)
    if len(header) < 54 or header[:2] != b'BM' or struct.unpack_from('<I', header, 14)[0] < 40:
        raise RuntimeError(f'Unsupported BMP header: {path.name}')
    width, height = struct.unpack_from('<ii', header, 18)
    if width <= 0 or height == 0:
        raise RuntimeError(f'Invalid BMP dimensions: {path.name}')
    return width, abs(height)


def inspect_render_dimensions(log, mode, dimensions):
    extents = re.findall(r'world_fsr2 version=2\.2\.1 mode=(\d+) '
                         r'render=(\d+)x(\d+) output=(\d+)x(\d+)', log)
    if not extents:
        if mode:
            raise RuntimeError('Missing render-resolution evidence')
        return None
    for actual_mode, width, height, out_width, out_height in extents:
        if int(actual_mode) != mode or (int(out_width), int(out_height)) != tuple(dimensions):
            raise RuntimeError('Render mode/output dimensions differ from request')
        if mode == 1 and (int(width), int(height)) != tuple(dimensions):
            raise RuntimeError('Native-AA capture rendered below the requested resolution')
    return [int(extents[-1][1]), int(extents[-1][2])]


def inspect(case, captures, ray_tracing, dimensions):
    log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
    for marker in ('map_renderer_loaded', 'authoritative_player_ready eye=',
                   'open_world_exit mode=map code=0'):
        if marker not in log:
            raise RuntimeError(f'Missing runtime evidence: {marker}')
    failures = re.findall(r'^.*(?:map_model_load_failed|World frame failed|'
                          r'World graphics completion failed|Map startup timed out|'
                          r'profile_writer_failed|rhi_validation severity=error|Validation Error|VUID-|D3D12 ERROR|D3D12 CORRUPTION).*$',
                          log, flags=re.MULTILINE)
    if failures:
        raise RuntimeError('\n'.join(failures[:8]))
    if captures and 'world_capture frame=' not in log:
        raise RuntimeError('No GPU capture recorded; verify the frame budget allows map warmup and readback')
    if captures and log.index('world_capture frame=') < log.index('authoritative_player_ready eye='):
        raise RuntimeError('Capture occurred before the authoritative map pose')
    draws = re.findall(r'map_draw forward=0 submitted=(\d+) culled=(\d+)', log)
    indirect = re.findall(r'map_draw forward=0 indirect=1 command_slots=(\d+) cpu_submissions=1', log)
    meshlets = re.findall(r'map_draw forward=0 meshlet=1 meshlets=(\d+) cpu_submissions=1', log)
    if not any(int(submitted) > 0 for submitted, _ in draws) and not any(int(slots)>0 for slots in indirect+meshlets):
        raise RuntimeError('No opaque map draw submissions recorded')
    paths = ([case / 'frame.bmp'] + [case / f'frame.bmp.sample-{i}.bmp'
                                   for i in range(1, captures)]) if captures else []
    if ray_tracing:
        allocations = re.findall(r'map_ray_allocation geometries=(\d+) triangles=(\d+)', log)
        if not allocations or not any(int(geometries) > 0 and int(triangles) > 0
                                      for geometries, triangles in allocations):
            raise RuntimeError('Ray tracing enabled but no map BLAS was built')
    evidence = []
    for path in paths:
        if not path.is_file() or path.stat().st_size < 54:
            raise RuntimeError(f'Missing GPU capture: {path.name}')
        actual_dimensions = capture_dimensions(path)
        if actual_dimensions != tuple(dimensions):
            raise RuntimeError(f'Capture dimensions {actual_dimensions} != requested {tuple(dimensions)}: {path.name}')
        counters = json.loads(Path(str(path) + '.lighting.json').read_text())
        observation = json.loads(Path(str(path) + '.observation.json').read_text())
        # shaded_pixels counts the local-light deferred pass only; with no local
        # lights the pass is legitimately skipped and the counter stays zero.
        if ray_tracing and counters.get('local_light_count', 0) > 0 \
                and counters.get('shaded_pixels', 0) <= 0:
            raise RuntimeError(f'No actual deferred shading work in {path.name}')
        evidence.append(dict(path=path.name, dimensions=list(actual_dimensions), sha256=digest(path), lighting=counters,
                             observation=observation))
    return evidence


def inspect_camera_motion(case):
    path = case / 'camera-motion.csv'
    if not path.is_file():
        raise RuntimeError('Camera motion fixture did not write evidence')
    rows = path.read_text(encoding='utf-8').splitlines()
    if len(rows) < 20:
        raise RuntimeError('Camera motion evidence is too short')
    phases = {row.split(',')[2] for row in rows[1:] if len(row.split(',')) >= 3}
    required = {'motion', 'settle', 'cut', 'post_cut'}
    if not required.issubset(phases):
        raise RuntimeError(f'Camera motion phases missing: {sorted(required - phases)}')
    points = [row.split(',') for row in rows[1:] if len(row.split(',')) >= 8]
    def point(phase):
        for row in points:
            if row[2] == phase:
                return tuple(float(row[index]) for index in (3, 4, 5, 6, 7))
        raise RuntimeError(f'No camera evidence for {phase}')
    motion, settle, cut = point('motion'), point('settle'), point('cut')
    translation = sum((motion[i] - settle[i]) ** 2 for i in range(3)) ** .5
    rotation = abs(cut[3] - settle[3]) + abs(cut[4] - settle[4])
    if translation < 0.01 or rotation < 0.05:
        raise RuntimeError('Camera motion fixture did not move and rotate camera')
    return dict(path=path.name, phases=sorted(phases), translation=translation, cut_rotation=rotation)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, help='Explicit adjacent-map asset variant to capture')
    parser.add_argument('--connect', help='Existing isolated authority fixture endpoint')
    parser.add_argument('--audio-voices', type=int, choices=(0,8), default=0)
    parser.add_argument('--gameplay-route', type=Path, help='Time-driven real authority input route')
    parser.add_argument('--shader-cache-dir', type=Path, help='Isolated shader/pipeline cache; never clears the user cache')
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--ray-tracing', choices=('on', 'off'), default='on')
    parser.add_argument('--upscaler-mode', type=int, choices=range(7), default=0)
    parser.add_argument('--performance-profile', choices=('custom', 'HQ200'), default='custom')
    parser.add_argument('--render-scale', type=float, default=2/3, help='Fixed custom scale for matched comparisons')
    parser.add_argument('--reflection-distance', type=int, choices=range(1025), default=1024)
    parser.add_argument('--temporal-reflections', choices=('on', 'off'), default='on')
    parser.add_argument('--reflection-quality', choices=QUALITY_TIERS, default='high')
    parser.add_argument('--shadow-quality', choices=QUALITY_TIERS, default='high')
    parser.add_argument('--camera-motion', action='store_true',
                        help='run the hidden deterministic camera-only motion fixture')
    parser.add_argument('--camera-origin', type=float, nargs=5, default=[0,2,-20,.6,-.25],
                        metavar=('X','Y','Z','YAW','PITCH'), help='Fixed benchmark view; rendering only')
    parser.add_argument('--rt-reference', action='store_true', help='use original conservative ray sampling')
    parser.add_argument('--rt-queued', action='store_true', help='exercise compact map reflection work queues')
    add_render_options(parser)
    add_counter_option(parser)
    add_wave_option(parser)
    add_gpu_counter_options(parser)
    add_tile_options(parser)
    parser.add_argument('--rt-screen-hits', action='store_true', help='experimental screen intersections; opaque occlusion unqualified; requires --rt-queued')
    parser.add_argument('--rt-queue-coherent', action='store_true', help='opt-in coherent recovery candidate; requires --rt-queued')
    parser.add_argument('--rt-queue-reference-recovery', action='store_true', help='full-direction queue recovery control; requires --rt-queued')
    parser.add_argument('--rt-temporal-full-fresh', action='store_true', help='diagnostic temporal control with all current-frame directions')
    parser.add_argument('--rt-sparse', action='store_true', help='classify stable rough tiles for alternating temporal samples')
    parser.add_argument('--rt-history-search', action='store_true', help='opt-in strict 2x2 reflection history search; shadows and reference stay unchanged')
    parser.add_argument('--fixed-sampling', action='store_true', help='quality-only ready-frame jitter/ray phases and fixed presentation delta')
    parser.add_argument('--ray-diagnostics', action='store_true', help='count actual queries; invalidates FPS qualification')
    parser.add_argument('--frame-cpu-trace', action='store_true', help='Trace CPU submission stages; invalidates FPS qualification')
    parser.add_argument('--draw-mode', choices=('direct', 'indirect', 'meshlet'), default='direct')
    parser.add_argument('--map-occlusion', action='store_true', help='two-phase Hi-Z occlusion for indirect map draws')
    parser.add_argument('--lod-pixels', type=float, default=0, help='Cooked geometry error in output pixels; zero preserves full detail')
    parser.add_argument('--warmup-frames', type=int, default=120)
    parser.add_argument('--debug', type=int, default=0)
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--capture-min-frame', type=int, default=300)
    parser.add_argument('--captures', type=int, choices=range(65), default=3)
    parser.add_argument('--stride', type=int, choices=range(1, 121), default=16)
    parser.add_argument('--width', type=int, default=2560)
    parser.add_argument('--height', type=int, default=1440)
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-frame-ms', type=float, default=50.0,
                        help='watchdog slow-frame cutoff for GPU warm-up profiling')
    parser.add_argument('--uncapped-fps', action='store_true',
                        help='disable hidden benchmark pacing for FPS qualification')
    parser.add_argument('--rhi-validation', action='store_true')
    parser.add_argument('--disable-map-culling', action='store_true')
    args = parser.parse_args()
    try:
        resolve_render_options(args, os.name)
        resolve_gpu_counters(args)
        counter_mode = resolve_counter_mode(args.ray_counter_shader, args.ray_diagnostics, args.backend)
        wave_mode = resolve_wave_mode(args.reflection_wave_size, args.backend, args.ray_diagnostics)
        if args.reflection_wave_size and (not args.rt_map_only or args.reflection_distance <= 0):
            raise ValueError('Forced reflection wave requires active map-only temporal reflections')
    except ValueError as error:
        parser.error(str(error))
    if args.rt_screen_hits and not args.rt_queued:
        parser.error('--rt-screen-hits requires --rt-queued')
    if args.rt_queue_coherent or args.rt_queue_reference_recovery:
        if not args.rt_queued or (args.rt_queue_coherent and args.rt_queue_reference_recovery):
            parser.error('Select one queue recovery variant with --rt-queued')
    if args.rt_temporal_full_fresh and (args.rt_queued or args.temporal_reflections != 'on'):
        parser.error('Full-fresh diagnostic requires legacy temporal reflections')
    route=None
    if args.gameplay_route:
        if args.camera_motion or args.fixed_sampling:
            parser.error('Gameplay routes cannot use camera-only motion or fixed presentation time')
        route=json.loads(args.gameplay_route.read_text())
        route_seconds=math.ceil(sum(float(phase['seconds']) for phase in route['phases']))+2
        if not 1 <= route_seconds <= 86402 or args.timeout < route_seconds+30:
            parser.error('Gameplay route needs a bounded duration and timeout at least duration+30 seconds')
    if not 1/3 <= args.render_scale <= 1:
        parser.error('--render-scale must be in [1/3,1]')
    if args.performance_profile == 'HQ200':
        if (args.width, args.height) != (2560, 1440) or args.ray_tracing != 'on':
            parser.error('HQ200 requires 2560x1440 output and ray tracing')
        args.upscaler_mode=6
        args.reflection_quality=args.shadow_quality='ultra'
        args.reflection_distance=1024
    if not 120 <= args.capture_min_frame <= 10000:
        parser.error('--capture-min-frame must be between 120 and 10000')
    try:
        validate_capture_schedule(args.frames, args.capture_min_frame, args.captures, args.stride)
    except ValueError as error:
        parser.error(str(error))
    if min(args.width, args.height, args.timeout) <= 0:
        parser.error('Dimensions and timeout must be positive')
    if args.warmup_frames < 0 or args.warmup_frames >= args.frames:
        parser.error('Warmup must leave measured frames')
    if not 0 <= args.lod_pixels <= 4 or (args.lod_pixels and args.draw_mode != 'indirect'):
        parser.error('Geometry LOD requires indirect draws and an error budget in [0,4]')
    if args.camera_motion and args.frames < 360:
        parser.error('Motion fixture needs at least 360 frames for all phases')
    bundle = args.client_bundle_root.resolve()
    manifest_path = args.manifest.resolve() if args.manifest else bundle / 'Client/Assets/Maps/map.json'
    manifest = json.loads(manifest_path.read_text())
    map_name = manifest['map']
    if not isinstance(map_name,str) or not map_name or Path(map_name).is_absolute() or Path(map_name).drive:
        parser.error('Map manifest must reference a relative map file')
    map_path = (manifest_path.parent / map_name).resolve()
    if not map_path.is_relative_to(manifest_path.parent):
        parser.error('Map manifest escapes its asset directory')
    if not map_path.is_file():
        parser.error(f'Missing map: {map_path}')
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'map-{args.backend}-{args.ray_tracing}-{args.debug}-',
                                 dir=args.evidence_root.resolve()))
    (case / 'world').mkdir()
    enabled = args.ray_tracing == 'on'
    settings = dict(version=16, windowWidth=args.width, windowHeight=args.height,
                    fullscreen=False, renderDistance=4, upscalerMode=args.upscaler_mode,
                    fsrRenderScale=args.render_scale,
                    reflectionDistance=args.reflection_distance,
                    pbrEnabled=True, fogEnabled=False, rayTracingEnabled=enabled)
    settings.update(quality_settings(args.reflection_quality, args.shadow_quality))
    (case / 'settings.json').write_text(json.dumps(settings))
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('OCTARYN_', 'VK_LAYER'))
           and key not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in (('WORLD', 'world'), ('SETTINGS', 'settings.json'),
                      ('LIGHTING', 'lighting.json'), ('INVENTORY', 'inventory.json'),
                      ('CAPTURE', 'frame.bmp'), ('PROFILE', 'profile.csv')):
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_MAP_MODE='1', OCTARYN_CLIENT_GRAPHICS_API=args.backend,
               OCTARYN_CLIENT_PERFORMANCE_PROFILE=args.performance_profile,
               OCTARYN_CLIENT_SERVER_LOG_DIR=str(case / 'world/logs/server'),
               OCTARYN_CLIENT_MAP_MANIFEST=str(manifest_path),
               OCTARYN_CLIENT_RAY_TRACING='required' if enabled else 'off',
               OCTARYN_CLIENT_LIGHTING_DEBUG=str(args.debug),
               OCTARYN_CLIENT_MAP_REFLECTION_TEMPORAL='1' if args.temporal_reflections == 'on' else '0',
               OCTARYN_CLIENT_CAPTURE_COUNT=str(args.captures),
               OCTARYN_CLIENT_CAPTURE_STRIDE=str(args.stride),
               OCTARYN_CLIENT_CAPTURE_MIN_FRAME=str(args.capture_min_frame),
               OCTARYN_CLIENT_CAPTURE_READY_FRAME=str(args.capture_min_frame),
               OCTARYN_CLIENT_FRAME_TIMING_PATH=str(case / 'frame-timing.csv'),
               OCTARYN_CLIENT_GPU_PROFILE_PATH=str(case / 'gpu.csv'),
               OCTARYN_CLIENT_LIGHTING_PROFILE_PATH=str(case / 'lighting.csv'))
    if not args.captures:
        env.pop('OCTARYN_CLIENT_CAPTURE_PATH', None)
    if args.audio_voices:
        env['OCTARYN_CLIENT_AUDIO_WORKLOAD']=str(args.audio_voices)
        env['OCTARYN_CLIENT_AUDIO_PROFILE']=str(case / 'audio.csv')
    if args.rt_reference:
        env['OCTARYN_CLIENT_RT_REFERENCE'] = '1'
    apply_render_options(args, env)
    apply_counter_mode(args.ray_counter_shader, args.ray_diagnostics, env, args.backend)
    apply_wave_mode(args.reflection_wave_size, env)
    hardware_counters = prepare_gpu_counters(args, env, case)
    tile_options = prepare_tile_options(args, manifest_path, manifest, case, env)
    if args.rt_queued:
        if args.rt_reference:
            parser.error('--rt-queued and --rt-reference select different implementations')
        env['OCTARYN_CLIENT_RT_QUEUED'] = '1'
    if args.rt_queue_coherent:
        env['OCTARYN_CLIENT_RT_QUEUE_COHERENT_RECOVERY'] = '1'
    if args.rt_screen_hits:
        env['OCTARYN_CLIENT_RT_SCREEN_HITS'] = '1'
    if args.rt_queue_reference_recovery:
        env['OCTARYN_CLIENT_RT_QUEUE_REFERENCE_RECOVERY'] = '1'
    if args.rt_temporal_full_fresh:
        env['OCTARYN_CLIENT_RT_TEMPORAL_FULL_FRESH'] = '1'
    if args.rt_sparse:
        env['OCTARYN_CLIENT_RT_SPARSE'] = '1'
    if args.rt_history_search:
        env['OCTARYN_CLIENT_RT_HISTORY_SEARCH'] = '1'
    if args.fixed_sampling:
        env['OCTARYN_CLIENT_FIXED_SAMPLING'] = '1'
    cache_state = dict(directory='user default', initial_files=None, os_file_cache='unknown')
    if args.shader_cache_dir:
        cache_path = args.shader_cache_dir.resolve()
        cache_files = [path for path in cache_path.rglob('*') if path.is_file()] if cache_path.exists() else []
        cache_state.update(directory=str(cache_path), initial_files=len(cache_files),
                           initial_bytes=sum(path.stat().st_size for path in cache_files))
        env['OCTARYN_CLIENT_SHADER_CACHE_PATH'] = str(cache_path)
    env['OCTARYN_CLIENT_MAP_DRAW_MODE'] = args.draw_mode
    if args.map_occlusion:
        env['OCTARYN_CLIENT_MAP_OCCLUSION'] = '1'
    env['OCTARYN_CLIENT_MAP_LOD_PIXELS'] = str(args.lod_pixels)
    if args.ray_diagnostics:
        env['OCTARYN_CLIENT_RAY_DIAGNOSTICS'] = str(case / 'ray-diagnostics.csv')
    if args.frame_cpu_trace:
        env['OCTARYN_CLIENT_FRAME_CPU_TRACE'] = '1'
        env['OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH'] = str(case / 'frame-retirement.csv')
    if args.uncapped_fps:
        env['OCTARYN_CLIENT_BENCHMARK_UNCAPPED'] = '1'
    if route:
        env['OCTARYN_CLIENT_GAMEPLAY_ROUTE']=str(args.gameplay_route.resolve())
        env['OCTARYN_CLIENT_GAMEPLAY_ROUTE_PATH']=str(case / 'gameplay-route.csv')
    else:
        env.update(OCTARYN_CLIENT_MAP_CAMERA_MOTION='1' if args.camera_motion else 'static',
                   OCTARYN_CLIENT_MAP_CAMERA_ORIGIN=','.join(map(str,args.camera_origin)),
                   OCTARYN_CLIENT_MAP_CAMERA_MOTION_PATH=str(case / 'camera-motion.csv'))
    if args.camera_motion:
        if not args.ray_tracing == 'on':
            parser.error('--camera-motion requires --ray-tracing on')
        env.update(OCTARYN_CLIENT_MAP_CAMERA_MOTION='1',
                   OCTARYN_CLIENT_MAP_CAMERA_MOTION_PATH=str(case / 'camera-motion.csv'),
                   OCTARYN_CLIENT_CAPTURE_STABLE_FRAMES='0')
    if args.rhi_validation:
        env['OCTARYN_CLIENT_RHI_VALIDATION'] = '1'
    env['OCTARYN_CLIENT_MAP_DRAW_DIAGNOSTICS'] = '1'
    env['OCTARYN_CLIENT_LIVE_FRAME_TIMING'] = '1'
    if args.disable_map_culling:
        env['OCTARYN_CLIENT_MAP_DISABLE_CULLING'] = '1'
    record_build(bundle, case)
    # Map sessions use --frames for termination; benchmark mode disables input
    # and hides the window so ordinary desktop activity cannot steer captures.
    command = [str(bundle / 'Octaryn.Client.exe'), '--benchmark-hidden', '--benchmark-settings']
    command += ['--benchmark-seconds',str(route_seconds)] if route else ['--frames',str(args.frames)]
    if args.connect:
        command += ['--connect', args.connect]
    result = dict(status='running', backend=args.backend, ray_tracing=enabled,
                  performance_profile=args.performance_profile, requested_render_scale=args.render_scale,
                  upscaler_mode=args.upscaler_mode, uncapped_fps=args.uncapped_fps,
                  reflection_distance=args.reflection_distance,
                  temporal_reflections=args.temporal_reflections,
                  reflection_quality=args.reflection_quality, shadow_quality=args.shadow_quality,
                  camera_motion=args.camera_motion, camera_origin=args.camera_origin, fixed_lighting=True,
                  rt_reference=args.rt_reference, rt_sparse=args.rt_sparse, warmup_frames=args.warmup_frames,
                  rt_queued=args.rt_queued,
                  **render_option_evidence(args),
                  **counter_mode,
                  **wave_mode,
                  gpu_hardware_counters=hardware_counters,
                  tiled_capture=tile_options,
                  rt_screen_hits=args.rt_screen_hits,
                  rt_queue_coherent=args.rt_queue_coherent,
                  rt_queue_reference_recovery=args.rt_queue_reference_recovery,
                  rt_temporal_full_fresh=args.rt_temporal_full_fresh,
                  rt_history_search=args.rt_history_search and not args.rt_reference,
                  fixed_sampling=args.fixed_sampling,
                  shader_cache=cache_state,
                  draw_mode=args.draw_mode, lod_pixels=args.lod_pixels, ray_diagnostics=args.ray_diagnostics,
                  rhi_validation=args.rhi_validation,
                  frame_cpu_trace=args.frame_cpu_trace,
                  timing_qualification=not (args.gpu_counters or args.ray_diagnostics or args.rhi_validation or args.debug or args.fixed_sampling or args.frame_cpu_trace or args.rt_temporal_full_fresh),
                  watchdog_max_frame_ms=args.max_frame_ms, process_priority='below-normal',
                  debug=args.debug, command=command, dimensions=[args.width, args.height],
                  authority_endpoint=args.connect,
                  audio_voices=args.audio_voices,
                  gameplay_route=route,
                  map=dict(path=str(map_path), sha256=digest(map_path), manifest=manifest,
                           manifest_sha256=digest(manifest_path),
                           cooked_identity=cooked_asset_identity(map_path, manifest_path, manifest),
                           cooked_metadata={str(path.relative_to(map_path.parent)): digest(path)
                               for path in (Path(str(map_path) + '.textures') / 'map-texture-cook.json',
                                            Path(str(map_path) + '.lods.sha256')) if path.is_file()}))
    print(f'map_capture_started evidence={case}', flush=True)
    try:
        with (case / 'client.log').open('wb') as log:
            code = run_capture(command, case, env, log, args.timeout,
                               max_frame_ms=args.max_frame_ms)
        result['exit_code'] = code
        if args.shader_cache_dir and 'shader_cache explicit_directory=1' not in (case / 'client.log').read_text(errors='replace'):
            raise RuntimeError('Executable did not activate the requested isolated shader cache')
        if code:
            raise RuntimeError(f'Map capture failed with exit code {code}')
        result['captures'] = inspect(case, args.captures, enabled, (args.width, args.height))
        if args.fixed_sampling:
            log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
            if 'map_validation_sampling fixed=1 index=ready_frame' not in log:
                raise RuntimeError('Requested fixed sampling was not activated')
            for capture in result['captures']:
                observation=capture['observation']
                if not observation.get('fixed_sampling') or abs(observation.get('delta_ms',0)-1000/60)>.001:
                    raise RuntimeError('Captured presentation sampling does not match the quality fixture')
        if args.camera_motion:
            result['camera_motion_evidence'] = inspect_camera_motion(case)
        log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
        result['tiled_capture'] = inspect_tile_options(case, tile_options, log, result['captures'])
        if args.frame_cpu_trace:
            from frame_retirement_report import read_trace
            result['frame_retirement'] = read_trace(case / 'frame-retirement.csv', log, case / 'gpu.csv')
        if args.rt_temporal_full_fresh and 'map_reflections temporal_full_fresh=1 diagnostic=1' not in log:
            raise RuntimeError('Executable did not activate the full-fresh temporal diagnostic')
        inspect_render_options(args, log, result)
        result.update(inspect_counter_mode(log, counter_mode))
        result.update(inspect_wave_mode(log, wave_mode))
        result['gpu_hardware_counters'] = join_gpu_counter_frame(case,
            inspect_gpu_counters(case / 'gpu-counters.jsonl', hardware_counters, log))
        if args.rt_screen_hits and 'map_reflections screen_hits=1 conservative_coverage=1' not in log:
            raise RuntimeError('Requested screen intersections are unavailable or inactive')
        result['render_dimensions'] = inspect_render_dimensions(
            log, args.upscaler_mode, (args.width, args.height))
        reflection_extents = re.findall(r'map_reflections temporal=1 width=(\d+) height=(\d+)', log)
        if reflection_extents:
            result['reflection_dimensions'] = list(map(int, reflection_extents[-1]))
            if any(int(extent[0]) <= 0 or int(extent[1]) <= 0 or
                   int(extent[0]) > args.width or int(extent[1]) > args.height
                   for extent in reflection_extents):
                raise RuntimeError('Temporal reflection extent is invalid for the requested output')
        look = re.search(r'authoritative_player_ready eye=[^\n]+ yaw=([-+\d.eE]+) pitch=([-+\d.eE]+)', log)
        if not look:
            raise RuntimeError('Missing authoritative map look evidence')
        if args.upscaler_mode:
            temporal = re.search(r'world_fsr2 version=2\.2\.1 mode=(\d+) ', log)
            if not temporal or int(temporal[1]) != args.upscaler_mode:
                raise RuntimeError('Requested temporal mode was not initialized')
            if temporal.start() > look.start() or 'map_boot_temporal' not in log:
                raise RuntimeError('Temporal setup was deferred until gameplay')
        result['spawn_look'] = dict(yaw=float(look[1]), pitch=float(look[2]))
        for key in ('yaw', 'pitch'):
            difference=result['spawn_look'][key]-float(manifest.get(key,result['spawn_look'][key]))
            if key=='yaw':difference=math.remainder(difference,math.tau)
            if abs(difference) > .0001:
                raise RuntimeError(f'Authoritative map {key} differs from manifest')
        result['performance'] = summarize_case(case, args.warmup_frames)
        if route:
            from gameplay_route_report import summarize_gameplay_route
            result['gameplay_route_evidence']=summarize_gameplay_route(case,route)
        if args.audio_voices:
            with (case / 'audio.csv').open(newline='') as source:
                audio=list(csv.DictReader(source))
            if not audio or any(int(row['active_voices'])!=args.audio_voices for row in audio):
                raise RuntimeError('Requested continuous audio voices were not active')
            costs=[float(row['cpu_ms']) for row in audio]
            if any(not math.isfinite(value) or value<0 for value in costs):
                raise RuntimeError('Invalid audio timing evidence')
            result['audio']=dict(voices=args.audio_voices,output='OpenAL loopback',
                                 cpu_ms=distribution(costs),mixed_samples=sum(int(row['mixed_samples']) for row in audio))
        if args.ray_diagnostics:
            from validate_ray_diagnostics import validate
            result['ray_counters'] = validate(case / 'ray-diagnostics.csv', args.rt_reference, expected_wave=args.reflection_wave_size)
            result['reflection_wave_actual'] = result['ray_counters']['reflection_wave']
        result.update(status='captured' if args.captures else 'measured',
                      visual_acceptance='pending image inspection' if args.captures else 'not assessed')
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'map_capture_completed evidence={case}', flush=True)


if __name__ == '__main__':
    main()
