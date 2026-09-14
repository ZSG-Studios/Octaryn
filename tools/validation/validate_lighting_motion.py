#!/usr/bin/env python3
"""Capture an isolated lateral camera sweep through the production lighting graph."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from lighting_sequence import inspect_sequence
from lighting_edit_sequence import prepare as prepare_edits, drive as drive_edits
from lighting_tunnel_fixture import prepare as prepare_tunnel, inspect as inspect_tunnel, inspect_stationary
from cave_lighting_fixture import prepare as prepare_cave, lower_tunnel, inspect_far_wall
from validate_lighting_architecture import environment, fixture, inspect_capture, inspect_profile, stop_case
from validate_rhi_client_diagnostic import inspect_result


def inspect_motion(text, captures):
    if 'lighting_motion fixture=lateral_sine amplitude_m=4 period_frames=256' not in text:
        raise RuntimeError('Missing explicit production lateral camera fixture')
    rows = re.findall(r'world_capture frame=(\d+).*?eye=([\d.-]+),([\d.-]+),([\d.-]+) yaw=([\d.-]+) pitch=([\d.-]+)', text)
    if len(rows) != captures:
        raise RuntimeError(f'Expected {captures} captures, received {len(rows)}')
    poses = [[float(value) for value in row] for row in rows]
    span = max(row[1] for row in poses) - min(row[1] for row in poses)
    if span < 7.5 or poses[-1][0] - poses[0][0] < 240:
        raise RuntimeError('Captured frames do not cover both sides of the lateral sweep')
    if any(max(row[field] for row in poses) - min(row[field] for row in poses) > .0001
           for field in range(2, 6)):
        raise RuntimeError('Lateral fixture unexpectedly changed height, depth or orientation')
    return dict(lateral_span_m=span, frame_span=poses[-1][0] - poses[0][0], camera_poses=poses,
                scope='Actual moving-camera GPU captures; receiver pixel differences include parallax and are not a noise score.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), required=True)
    parser.add_argument('--quality', choices=('low', 'medium', 'high', 'ultra'), default='high')
    parser.add_argument('--upscaler', choices=('native', 'quality', 'balanced', 'performance'), default='native')
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--frames', type=int, default=900)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--debug', type=int, default=0)
    parser.add_argument('--probe-states', action='store_true', help='Capture probe cells, relocation and history for stability diagnosis')
    parser.add_argument('--stationary', action='store_true', help='Same receiver with a fixed camera for temporal noise metrics')
    parser.add_argument('--third-person', action='store_true')
    parser.add_argument('--digging', action='store_true', help='Remove and replace a nearby block through authoritative server commands')
    parser.add_argument('--ddgi-off', action='store_true', help='Isolate direct lighting with both DDGI volumes disabled')
    parser.add_argument('--tunnel', action='store_true', help='Use an enclosed stone tunnel; combine with --digging for wall opening and restoration')
    parser.add_argument('--tunnel-width', type=int, choices=(1, 2), default=2)
    parser.add_argument('--underground', action='store_true', help='Move the tunnel and accepted edits below Y=0')
    parser.add_argument('--cave-motion', action='store_true', help='Sweep inside a sealed underground hall across both probe cascade fades')
    args = parser.parse_args()
    if args.underground and not args.tunnel:
        parser.error('--underground requires --tunnel')
    if args.cave_motion and (args.tunnel or args.digging or args.stationary or args.third_person):
        parser.error('--cave-motion requires the first-person moving camera without other fixtures')
    if args.frames < 600 or args.width < 320 or args.height < 240:
        parser.error('Require at least 600 frames and 320x240')
    args.captures = 48 if args.digging else 32
    if args.digging:
        args.frames = max(args.frames, 1200)
    args.block_lights = True
    args.vegetation_shadows = False
    args.resize = False
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    if args.tunnel:
        if args.third_person:
            parser.error('Tunnel receiver qualification requires the first-person camera')
        args.stationary = True
    if args.digging:
        args.stationary = True
    mode = 'digging' if args.digging else 'stationary' if args.stationary else 'lateral'
    case = Path(tempfile.mkdtemp(prefix=f'lighting-{mode}-{args.backend}-', dir=args.evidence_root.resolve()))
    fixture(bundle, case, args)
    tunnel = prepare_tunnel(case, bundle, args.tunnel_width) if args.tunnel else None
    stone = prepare_edits(case, bundle) if args.digging or args.tunnel else None
    if args.underground:
        lower_tunnel(case, tunnel)
    cave = prepare_cave(case, bundle) if args.cave_motion else None
    if cave:
        catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
        stone = next(i for i, block in enumerate(catalog) if block['id'].endswith('.stone'))
    enclosure = cave or tunnel
    # Same angled sun for repeatable shadow receiver appearance.
    (case / 'world/world_time.json').write_text(json.dumps(dict(version=1, day_index=0, seconds_of_day=9*3600)))
    env = environment(case, args)
    if args.probe_states:
        env['OCTARYN_CLIENT_CAPTURE_DDGI_STATES'] = '1'
    # An odd stride exercises both in-flight frame resource slots.
    env['OCTARYN_CLIENT_CAPTURE_STRIDE'] = '9'
    env['OCTARYN_SERVER_START_HOUR'] = '9'
    if enclosure:
        env['OCTARYN_CLIENT_CAPTURE_MIN_FRAME'] = '300'
    if args.digging:
        env['OCTARYN_CLIENT_CAPTURE_STABLE_FRAMES'] = '0'
    if args.ddgi_off:
        env['OCTARYN_CLIENT_DDGI'] = 'off'
    options = ['--frames', str(args.frames), '--validate-ui', '--benchmark-hidden']
    if not args.stationary:
        options += ['--validate-lighting-motion']
    else:
        # Keep stationary references in the same isolated, input-free app mode.
        options += ['--validate-lighting-edits']
    if args.third_person:
        options += ['--third-person']
    print(f'lighting_motion_started mode={mode} evidence={case}', flush=True)
    with (case / 'client.log').open('wb') as log:
        process = subprocess.Popen([str(bundle / 'Octaryn.Client.exe'), *options], cwd=case,
                                   env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            edits = drive_edits(process, case, stone, args.timeout,
                                enclosure.get('startup_roof', []) if enclosure else [],
                                measure_edits=args.digging,
                                floor_y=enclosure['floor_y'] if enclosure else 160) if args.digging or enclosure else None
            if edits:
                (case / 'edit_sequence.json').write_text(json.dumps(edits, indent=2), encoding='utf-8')
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired as error:
            stop_case(process, case)
            raise RuntimeError(f'Lighting motion capture timed out; evidence: {case}') from error
        except (OSError, ValueError, RuntimeError):
            stop_case(process, case)
            raise
    text = (case / 'client.log').read_text(errors='replace')
    frames, columns, quads = inspect_result(code, text, 'D3D12' if args.backend == 'dx12' else 'Vulkan', 600)
    paths = [case / 'frame.bmp'] + [case / f'frame.bmp.sample-{i}.bmp' for i in range(1, args.captures)]
    if not all(path.is_file() for path in paths):
        raise RuntimeError(f'Incomplete GPU capture sequence; evidence: {case}')
    capture_frames = [int(frame) for frame in re.findall(r'world_capture frame=(\d+)', text)]
    if {frame % 2 for frame in capture_frames} != {0, 1}:
        raise RuntimeError('Capture sequence did not exercise both frame-resource slots')
    counters = []
    for path in paths:
        counter_path = Path(str(path) + '.lighting.json')
        counters.append(inspect_capture(counter_path, args.quality, ddgi_enabled=not args.ddgi_off))
    if args.ddgi_off and any(row.get('valid_probes') != 0 or row.get('fine_valid_probes') != 0 for row in counters):
        raise RuntimeError('DDGI-disabled isolation did not report zero valid probes in both volumes')
    if args.tunnel:
        capture_frames = [int(frame) for frame in re.findall(r'world_capture frame=(\d+)', text)]
        if not capture_frames or min(capture_frames) < 300:
            raise RuntimeError('Tunnel captures did not honor the initial 300-frame probe warmup')
    measurements = edits if args.digging else inspect_sequence(paths[0], args.captures) if args.stationary else inspect_motion(text, args.captures)
    tunnel_receivers = inspect_tunnel(paths, edits) if args.tunnel and args.digging else None
    result = dict(status='passed', mode=mode, backend=args.backend, quality=args.quality,
                  frames=frames, columns=columns, quads=quads, captures=[str(path) for path in paths],
                  measurements=measurements, gpu_counters=counters,
                  timings=inspect_profile(case / 'lighting.csv', args.quality, ddgi_enabled=not args.ddgi_off),
                  visual_inspection='required', os_events_injected=0,
                  ddgi_disabled=args.ddgi_off,
                  tunnel=tunnel, cave=cave, tunnel_receivers=tunnel_receivers,
                  cave_far_wall=inspect_far_wall(paths) if cave else None,
                  stationary_receivers=inspect_stationary(paths) if args.tunnel and not args.digging else None,
                  startup_commands=edits.get('startup_commands', []) if edits else [])
    (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f'lighting_motion=passed mode={mode} evidence={case}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        print(f'lighting_motion=failed: {error}', file=sys.stderr)
        sys.exit(1)
