"""Capture the bundled GLB map with isolated settings and GPU evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import tempfile

from case_evidence import record_build
from capture_watchdog import run_capture

QUALITY_TIERS = ('low', 'medium', 'high', 'ultra')


def validate_capture_schedule(frames, first_capture, captures, stride):
    # Map sessions keep readback disabled until the authoritative view warms up.
    first_possible = max(180, first_capture)
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
                          r'rhi_validation severity=error|Validation Error|VUID-|D3D12 ERROR|D3D12 CORRUPTION).*$',
                          log, flags=re.MULTILINE)
    if failures:
        raise RuntimeError('\n'.join(failures[:8]))
    if 'world_capture frame=' not in log:
        raise RuntimeError('No GPU capture recorded; verify the frame budget allows map warmup and readback')
    if log.index('world_capture frame=') < log.index('authoritative_player_ready eye='):
        raise RuntimeError('Capture occurred before the authoritative map pose')
    draws = re.findall(r'map_draw forward=0 submitted=(\d+) culled=(\d+)', log)
    if not draws or not any(int(submitted) > 0 for submitted, _ in draws):
        raise RuntimeError('No opaque map draw submissions recorded')
    paths = [case / 'frame.bmp'] + [case / f'frame.bmp.sample-{i}.bmp'
                                   for i in range(1, captures)]
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
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--ray-tracing', choices=('on', 'off'), default='on')
    parser.add_argument('--upscaler-mode', type=int, choices=range(7), default=0)
    parser.add_argument('--reflection-distance', type=int, choices=range(1025), default=1024)
    parser.add_argument('--temporal-reflections', choices=('on', 'off'), default='on')
    parser.add_argument('--reflection-quality', choices=QUALITY_TIERS, default='high')
    parser.add_argument('--shadow-quality', choices=QUALITY_TIERS, default='high')
    parser.add_argument('--camera-motion', action='store_true',
                        help='run the hidden deterministic camera-only motion fixture')
    parser.add_argument('--debug', type=int, default=0)
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--capture-min-frame', type=int, default=300)
    parser.add_argument('--captures', type=int, choices=range(1, 65), default=3)
    parser.add_argument('--stride', type=int, choices=range(1, 121), default=16)
    parser.add_argument('--width', type=int, default=640)
    parser.add_argument('--height', type=int, default=360)
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-frame-ms', type=float, default=50.0,
                        help='watchdog slow-frame cutoff for GPU warm-up profiling')
    parser.add_argument('--uncapped-fps', action='store_true',
                        help='disable hidden benchmark pacing for FPS qualification')
    parser.add_argument('--rhi-validation', action='store_true')
    parser.add_argument('--disable-map-culling', action='store_true')
    args = parser.parse_args()
    if not 120 <= args.capture_min_frame <= 10000:
        parser.error('--capture-min-frame must be between 120 and 10000')
    try:
        validate_capture_schedule(args.frames, args.capture_min_frame, args.captures, args.stride)
    except ValueError as error:
        parser.error(str(error))
    if min(args.width, args.height, args.timeout) <= 0:
        parser.error('Dimensions and timeout must be positive')
    bundle = args.client_bundle_root.resolve()
    manifest_path = bundle / 'Client/Assets/Maps/map.json'
    manifest = json.loads(manifest_path.read_text())
    map_name = manifest['map']
    if not map_name or '/' in map_name or '\\' in map_name:
        parser.error('Map manifest must reference one adjacent file')
    map_path = manifest_path.parent / map_name
    if not map_path.is_file():
        parser.error(f'Missing map: {map_path}')
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'map-{args.backend}-{args.ray_tracing}-{args.debug}-',
                                 dir=args.evidence_root.resolve()))
    (case / 'world').mkdir()
    enabled = args.ray_tracing == 'on'
    settings = dict(version=15, windowWidth=args.width, windowHeight=args.height,
                    fullscreen=False, renderDistance=4, upscalerMode=args.upscaler_mode,
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
               OCTARYN_CLIENT_RAY_TRACING='required' if enabled else 'off',
               OCTARYN_CLIENT_LIGHTING_DEBUG=str(args.debug),
               OCTARYN_CLIENT_MAP_REFLECTION_TEMPORAL='1' if args.temporal_reflections == 'on' else '0',
               OCTARYN_CLIENT_CAPTURE_COUNT=str(args.captures),
               OCTARYN_CLIENT_CAPTURE_STRIDE=str(args.stride),
               OCTARYN_CLIENT_CAPTURE_MIN_FRAME=str(args.capture_min_frame),
               OCTARYN_CLIENT_FRAME_TIMING_PATH=str(case / 'frame-timing.csv'),
               OCTARYN_CLIENT_GPU_PROFILE_PATH=str(case / 'gpu.csv'),
               OCTARYN_CLIENT_LIGHTING_PROFILE_PATH=str(case / 'lighting.csv'))
    if args.uncapped_fps:
        env['OCTARYN_CLIENT_BENCHMARK_UNCAPPED'] = '1'
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
    command = [str(bundle / 'Octaryn.Client.exe'), '--frames', str(args.frames),
               '--benchmark-hidden', '--benchmark-settings']
    result = dict(status='running', backend=args.backend, ray_tracing=enabled,
                  upscaler_mode=args.upscaler_mode, uncapped_fps=args.uncapped_fps,
                  reflection_distance=args.reflection_distance,
                  temporal_reflections=args.temporal_reflections,
                  reflection_quality=args.reflection_quality, shadow_quality=args.shadow_quality,
                  camera_motion=args.camera_motion,
                  debug=args.debug, command=command, dimensions=[args.width, args.height],
                  map=dict(path=str(map_path), sha256=digest(map_path), manifest=manifest))
    print(f'map_capture_started evidence={case}', flush=True)
    try:
        with (case / 'client.log').open('wb') as log:
            code = run_capture(command, case, env, log, args.timeout,
                               max_frame_ms=args.max_frame_ms)
        result['exit_code'] = code
        if code:
            raise RuntimeError(f'Map capture failed with exit code {code}')
        result['captures'] = inspect(case, args.captures, enabled, (args.width, args.height))
        if args.camera_motion:
            result['camera_motion_evidence'] = inspect_camera_motion(case)
        log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
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
            if key in manifest and abs(result['spawn_look'][key] - float(manifest[key])) > .0001:
                raise RuntimeError(f'Authoritative map {key} differs from manifest')
        result.update(status='captured', visual_acceptance='pending image inspection')
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'map_capture_completed evidence={case}', flush=True)


if __name__ == '__main__':
    main()
