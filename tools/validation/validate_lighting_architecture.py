#!/usr/bin/env python3
"""Qualify the integrated lighting graph in an isolated production-client world."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
from validate_rhi_client_diagnostic import inspect_result
from validate_temporal import inspect_log as inspect_temporal_log
from lighting_sequence import inspect_sequence


def fixture(bundle, case, args):
    world = case / 'world'
    world.mkdir()
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {block['id'].split('.')[-1]: index for index, block in enumerate(catalog)}
    edits = {}
    # Elevated authored geometry keeps generation below the measurement surface.
    for z in range(-25, 15):
        for x in range(-19, 20):
            edits[x, 160, z] = ids['stone']
            for y in range(161, 174):
                edits[x, y, z] = 0
    if args.vegetation_shadows:
        for x, name in zip((-4, -2, 0, 2, 4), ('bush', 'bluebell', 'gardenia', 'rose', 'lavender')):
            edits[x, 161, 2] = ids[name]
    else:
        for x in (-7, 7):
            for y in range(161, 169):
                for z in (-15, -14):
                    edits[x, y, z] = ids['log']
        for x in range(-8, 9):
            for z in (-15, -14):
                edits[x, 169, z] = ids['planks']
        for y in range(161, 165):
            for z in range(-9, -5):
                edits[1, y, z] = ids['stone']
        for x in range(-2, 3):
            for z in range(-18, -15):
                edits[x, 167, z] = ids['leaves']
    if args.block_lights:
        for x, name in [(-4, 'red_torch'), (0, 'white_torch'), (4, 'blue_torch')]:
            edits[x, 161, -4] = ids[name]
    values = {
        world / 'world_blocks.json': dict(version=1, blocks=[dict(x=x, y=y, z=z, block=block)
                                                            for (x, y, z), block in edits.items()]),
        world / 'world_generation.json': dict(version=1, generator='octaryn.basegame', revision=3, seed=1337, mode=0),
        world / 'player_1.json': dict(version=1, x=0, y=162.62, z=7, pitch=-.18, yaw=0, block=14),
        case / 'settings.json': dict(version=10, windowWidth=args.width, windowHeight=args.height,
                                    fullscreen=False, renderDistance=4, upscalerMode=1,
                                    fsrSharpness=.3, fogEnabled=False,
                                    rayTracingEnabled=args.quality in ('high', 'ultra')),
    }
    if args.vegetation_shadows:
        values[world / 'world_time.json'] = dict(version=1, day_index=0, seconds_of_day=9 * 3600)
        values[world / 'player_1.json']['pitch'] = -.35
    for path, value in values.items():
        path.write_text(json.dumps(value), encoding='utf-8')


def environment(case, args):
    env = {k: v for k, v in os.environ.items() if not k.startswith(('OCTARYN_', 'VK_LAYER'))
           and k not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in [('WORLD', 'world'), ('SETTINGS', 'settings.json'), ('LIGHTING', 'lighting.json'),
                      ('INVENTORY', 'inventory.json'), ('CAPTURE', 'frame.bmp'), ('PROFILE', 'profile.csv')]:
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend, OCTARYN_CLIENT_UPSCALER=args.upscaler,
               OCTARYN_CLIENT_RHI_VALIDATION='0' if getattr(args, 'no_rhi_validation', False) else '1',
               OCTARYN_CLIENT_CAPTURE_TEMPORAL='1',
               OCTARYN_CLIENT_LIGHTING_FIXTURE='1', OCTARYN_CLIENT_LIGHTING_QUALITY=args.quality,
               OCTARYN_CLIENT_LIGHTING_PROFILE_PATH=str(case / 'lighting.csv'),
               OCTARYN_CLIENT_LIGHTING_DEBUG=str(args.debug),
               OCTARYN_CLIENT_RAY_TRACING='required' if args.quality in ('high', 'ultra') else 'off')
    env['OCTARYN_CLIENT_CAPTURE_COUNT'] = str(args.captures)
    env['OCTARYN_CLIENT_CAPTURE_STRIDE'] = '16'
    env['OCTARYN_CLIENT_GI'] = getattr(args, 'gi', 'ddgi')
    if args.block_lights:
        env.pop('OCTARYN_CLIENT_LIGHTING_FIXTURE', None)
    if getattr(args, 'vegetation_shadows', False):
        env['OCTARYN_SERVER_START_HOUR'] = '9'
    if args.resize:
        env.pop('OCTARYN_CLIENT_UPSCALER')
        env['OCTARYN_CLIENT_FRAMES_IN_FLIGHT'] = '2'
    if args.backend == 'vulkan':
        layers = Path(__file__).resolve().parents[2] / 'build/dependencies/vulkan-validation'
        env.update(VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers),
                   VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation')
    return env


def stop_case(process, case):
    runtime = case / 'world/runtime'
    runtime.mkdir(exist_ok=True)
    (runtime / 'shutdown.request').write_text('stop\n', encoding='utf-8')
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def inspect_capture(path, quality, ddgi_enabled=True):
    counters = json.loads(path.read_text(encoding='utf-8'))
    required = ['shaded_pixels', 'evaluated_lights']
    if quality in ('high', 'ultra'):
        required += ['local_visibility_rays']
        if ddgi_enabled:
            required.append('valid_probes')
    for key in required:
        value = counters.get(key)
        if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
            raise RuntimeError(f'Capture lacks actual nonzero GPU work: {key}={value}')
    for key, value in counters.items():
        if key in ('ddgi_coarse_enabled', 'ddgi_fine_enabled'):
            if not isinstance(value, bool):
                raise RuntimeError(f'Invalid captured volume flag: {key}={value}')
            continue
        if key in ('ddgi_coarse_counts', 'ddgi_coarse_coverage_min', 'ddgi_coarse_coverage_max'):
            if not isinstance(value, list) or len(value) != 3 or any(
                    isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in value):
                raise RuntimeError(f'Invalid captured volume bounds: {key}={value}')
            if key == 'ddgi_coarse_counts' and any(not isinstance(v, int) or v < 0 for v in value):
                raise RuntimeError(f'Invalid captured grid dimensions: {value}')
            continue
        if key in ('ddgi_coarse_spacing', 'ddgi_oldest_update_seconds') or key.endswith(
                ('_irradiance_sum', '_irradiance_mean', '_irradiance_max')):
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0:
                raise RuntimeError(f'Invalid captured GPU measurement: {key}={value}')
            continue
        if not isinstance(value, int) or isinstance(value, bool) or value < 0:
            raise RuntimeError(f'Invalid captured GPU counter: {key}={value}')
    for key in ('local_visibility_rays', 'tile_overflow_pixels'):
        if key not in counters:
            raise RuntimeError(f'Missing direct-light GPU counter: {key}')
    if counters['tile_overflow_pixels'] > counters['shaded_pixels']:
        raise RuntimeError('Overflow pixel count exceeds shaded pixels')
    # Each evaluated area light uses at most four fixed visibility samples.
    # Multiple contributing lights can legitimately cast several rays per pixel.
    if counters['local_visibility_rays'] > 4 * counters['evaluated_lights']:
        raise RuntimeError('Local visibility rays exceed the evaluated-light sample bound')
    if quality in ('low', 'medium') and counters.get('local_visibility_rays') != 0:
        raise RuntimeError('Non-RT fallback unexpectedly issued hardware local visibility rays')
    if quality in ('low', 'medium'):
        if counters.get('local_shadow_valid') != 1 or counters.get('local_shadow_selected') not in (0, 1):
            raise RuntimeError('Fallback did not shadow a supported point/spot fixture light')
        for key in ('local_shadow_updates', 'local_shadow_draws'):
            if counters.get(key, 0) <= 0:
                raise RuntimeError(f'Fallback local shadow maps lack actual raster work: {key}')
    return counters


def inspect_profile(path, quality, minimum_frames=120, ddgi_enabled=True):
    with path.open(newline='') as source:
        rows = list(csv.DictReader(source))
    if len(rows) < minimum_frames:
        raise RuntimeError(f'Lighting profile lacks {minimum_frames} completed frames')
    expected = ('local_cull_ms', 'local_shade_ms', 'sun_trace_ms', 'composition_ms')
    if quality in ('high', 'ultra'):
        expected += ('sun_filter_ms',)
        if ddgi_enabled:
            expected += ('ddgi_trace_ms', 'ddgi_update_ms')
    summary = {}
    for field in expected:
        if any(field not in row for row in rows):
            raise RuntimeError(f'Missing lighting GPU timestamp: {field}')
        values = [float(row[field]) for row in rows]
        if not all(math.isfinite(value) and value >= 0 for value in values) or max(values) <= 0:
            raise RuntimeError(f'Lighting pass lacks finite nonzero GPU execution: {field}')
        stable = values[len(values) // 2:]
        summary[field] = dict(median=statistics.median(stable), maximum=max(stable))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), required=True)
    parser.add_argument('--quality', choices=('low', 'medium', 'high', 'ultra'), default='high')
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--timeout', type=int, default=240)
    parser.add_argument('--debug', type=int, default=0)
    parser.add_argument('--upscaler', choices=('native', 'quality', 'balanced', 'performance'), default='native')
    parser.add_argument('--captures', type=int, choices=range(1, 33), default=1)
    parser.add_argument('--block-lights', action='store_true', help='Use placed torch voxels instead of diagnostic API lights')
    parser.add_argument('--vegetation-shadows', action='store_true', help='Place grass and all flowers on a clear receiver at 09:00')
    parser.add_argument('--resize', action='store_true', help='Run the existing nine-phase FSR/mode/window-resize qualification with lighting enabled')
    parser.add_argument('--gi', choices=('ddgi', 'src'), default='ddgi', help='Select the diffuse GI backend under qualification')
    parser.add_argument('--no-rhi-validation', action='store_true',
                        help='Omit the RHI debug layer; required for SRC runs where the D3D12 debug layer adds second-scale per-frame overhead unrelated to correctness')
    args = parser.parse_args()
    if args.width < 320 or args.height < 240 or args.frames < 180:
        parser.error('Qualification requires at least 320x240 and 180 frames')
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'lighting-{args.backend}-{args.quality}-', dir=args.evidence_root.resolve()))
    fixture(bundle, case, args)
    print(f'lighting_architecture_started evidence={case}', flush=True)
    options = ['--validate-temporal', '--benchmark-hidden'] if args.resize else [
        '--frames', str(args.frames), '--validate-ui', '--benchmark-hidden']
    with (case / 'client.log').open('wb') as log:
        process = subprocess.Popen([str(bundle / 'Octaryn.Client.exe'), *options], cwd=case,
                                   env=environment(case, args), stdout=log, stderr=subprocess.STDOUT)
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired as error:
            stop_case(process, case)
            raise RuntimeError(f'Lighting qualification timed out; evidence: {case}') from error
    text = (case / 'client.log').read_text(errors='replace')
    if code:
        print(f'lighting_client_exit decimal={code} hex=0x{code & 0xffffffff:08x} log={case / "client.log"}', file=sys.stderr)
        print('\n'.join(text.splitlines()[-35:]), file=sys.stderr)
    counts = inspect_result(code, text, 'D3D12' if args.backend == 'dx12' else 'Vulkan', minimum_frames=108 if args.resize else 180)
    if not (case / 'frame.bmp').is_file():
        raise RuntimeError(f'Missing production GPU capture; evidence: {case}')
    ddgi_active = args.gi == 'ddgi'
    if args.quality in ('high', 'ultra') and ddgi_active and not re.search(r'world_ray ready=81 pending=0 jobs=0', text):
        raise RuntimeError(f'RT scene never reached complete fixture coverage; evidence: {case}')
    timings = inspect_profile(case / 'lighting.csv', args.quality, minimum_frames=108 if args.resize else 120,
                              ddgi_enabled=ddgi_active)
    counters = inspect_capture(case / 'frame.bmp.lighting.json', args.quality, ddgi_enabled=ddgi_active)
    if args.vegetation_shadows:
        pose = json.loads((case / 'world/runtime/player_state.json').read_text())
        if not .35 < pose.get('worldTimeDayFraction', 0) < .4:
            raise RuntimeError('Vegetation shadow fixture did not receive angled morning sunlight')
    if args.block_lights and counters.get('block_selected_count', 0) < 3:
        raise RuntimeError('Placed torch voxels did not enter the active lighting registry')
    resize = inspect_temporal_log(code, text, args.backend, 2) if args.resize else None
    result = dict(status='passed', backend=args.backend, quality=args.quality,
                  dimensions=[args.width, args.height], upscaler=args.upscaler, debug=args.debug,
                  frames=counts[0], columns=counts[1], quads=counts[2], timings=timings, gpu_counters=counters,
                  capture=str(case / 'frame.bmp'), visual_inspection='required',
                  block_lights=args.block_lights,
                  vegetation_shadows=args.vegetation_shadows,
                  runtime_resize=resize if resize else 'not exercised; pass --resize for the nine-phase production qualification')
    if args.captures > 1:
        result['sequence'] = inspect_sequence(case / 'frame.bmp', args.captures)
    (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f'lighting_architecture=passed evidence={case}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        print(f'lighting_architecture=failed: {error}', file=sys.stderr)
        sys.exit(1)
