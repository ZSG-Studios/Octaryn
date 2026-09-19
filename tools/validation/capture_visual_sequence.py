"""Capture a settled frame sequence from a copy of a saved world's pose and settings."""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile

from validate_lighting_architecture import record_build, stop_case


def timing_summary(case, observations):
    def summary(values):
        values = sorted(values)
        if not values:
            return dict(samples=0)
        return dict(samples=len(values), median_ms=statistics.median(values),
                    p95_ms=values[int((len(values) - 1) * .95)], maximum_ms=max(values))
    result = {}
    with (case / 'lighting.csv').open(newline='') as source:
        rows = list(csv.DictReader(source))
    rows = rows[len(rows) // 2:]
    if rows:
        passes = ('ddgi_trace_ms', 'ddgi_update_ms')
        result['ddgi_gpu'] = summary([sum(float(row[key]) for key in passes) for row in rows])
    if (case / 'frame-timing.csv').exists():
        # Renderer stats count completed frames; readbacks label the just-submitted index.
        captured = {entry['frame'] + 1 for entry in observations}
        with (case / 'frame-timing.csv').open(newline='') as source:
            rows = list(csv.DictReader(source))
        rows = rows[len(rows) // 2:]
        result['frame_wall_excluding_captures'] = summary([
            float(row['total_ms']) for row in rows if int(row['frame']) not in captured])
    result['window'] = 'latter half; explicit readback frames excluded from wall timing'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--source-case', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--debug', type=int, default=0)
    parser.add_argument('--upscaler', choices=('off', 'native'), default='native')
    parser.add_argument('--backend', choices=('vulkan', 'dx12'), default='vulkan')
    parser.add_argument('--captures', type=int, choices=range(3, 65), default=32)
    parser.add_argument('--width', type=int)
    parser.add_argument('--height', type=int)
    parser.add_argument('--motion', action='store_true')
    parser.add_argument('--world-edits', action='store_true', help='Copy authored world edits and time from the isolated source fixture')
    parser.add_argument('--stride', type=int, choices=range(1, 120), default=1)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'visual-{args.backend}-{args.debug}-{args.upscaler}-',
                                 dir=args.evidence_root.resolve()))
    source = args.source_case.resolve()
    (case / 'world').mkdir()
    for name in ('world_generation.json', 'player_1.json'):
        shutil.copy2(source / 'world' / name, case / 'world' / name)
    if args.world_edits:
        for name in ('world_blocks.json', 'world_time.json'):
            if (source / 'world' / name).exists():
                shutil.copy2(source / 'world' / name, case / 'world' / name)
    for name in ('settings.json', 'lighting.json'):
        shutil.copy2(source / name, case / name)
    settings = json.loads((case / 'settings.json').read_text())
    if args.width:
        settings['windowWidth'] = args.width
    if args.height:
        settings['windowHeight'] = args.height
    (case / 'settings.json').write_text(json.dumps(settings))
    # Authored edits are copied only when explicitly requested for a fixture.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('OCTARYN_', 'VK_LAYER'))
           and key not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in (('WORLD', 'world'), ('SETTINGS', 'settings.json'),
                      ('LIGHTING', 'lighting.json'), ('INVENTORY', 'inventory.json'),
                      ('CAPTURE', 'frame.bmp'), ('PROFILE', 'profile.csv')):
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend,
               OCTARYN_CLIENT_UPSCALER=args.upscaler,
               OCTARYN_CLIENT_LIGHTING_DEBUG=str(args.debug),
               OCTARYN_CLIENT_CAPTURE_COUNT=str(args.captures), OCTARYN_CLIENT_CAPTURE_STRIDE=str(args.stride),
               OCTARYN_CLIENT_CAPTURE_TEMPORAL='1',
               OCTARYN_CLIENT_CAPTURE_TEMPORAL_COUNT='4',
               OCTARYN_CLIENT_BOOT_CAPTURE_PATH=str(case / 'startup-loading.bmp'),
               OCTARYN_CLIENT_FRAME_TIMING_PATH=str(case / 'frame-timing.csv'),
               OCTARYN_CLIENT_GPU_PROFILE_PATH=str(case / 'gpu-profile.csv'),
               OCTARYN_CLIENT_LIGHTING_PROFILE_PATH=str(case / 'lighting.csv'))
    bundle = args.client_bundle_root.resolve()
    record_build(bundle, case)
    command = [str(bundle / 'Octaryn.Client.exe'), '--benchmark-seconds',
               str(max(12, args.captures)), '--benchmark-hidden', '--benchmark-settings']
    if args.motion:
        command = [str(bundle / 'Octaryn.Client.exe'), '--frames',
                   str(max(600, 300 + args.captures * args.stride)), '--validate-ui',
                   '--benchmark-hidden', '--validate-lighting-motion']
    result = dict(status='running', source=str(source), backend=args.backend,
                  debug=args.debug, upscaler=args.upscaler, command=command,
                  motion=args.motion, stride=args.stride,
                  dimensions=[settings['windowWidth'], settings['windowHeight']])
    print(f'visual_sequence_started evidence={case}', flush=True)
    try:
        with (case / 'client.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=case, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=480)
            except subprocess.TimeoutExpired:
                stop_case(process, case)
                raise RuntimeError('Visual sequence timed out')
        captures = list(case.glob('frame*.bmp'))
        if code or len(captures) != args.captures:
            raise RuntimeError(f'Visual sequence exit={code}, captures={len(captures)}')
        observations = [json.loads(Path(str(path) + '.observation.json').read_text())
                        for path in captures]
        observations.sort(key=lambda value: value['frame'])
        for path in captures:
            counters = json.loads(Path(str(path) + '.lighting.json').read_text())
            for setting, counter in (('giVoxelRadius', 'ddgi_requested_voxel_radius'),
                                     ('giCoarseRadius', 'ddgi_requested_coarse_radius')):
                if setting in settings and counters[counter] != settings[setting]:
                    raise RuntimeError(f'Captured {counter} differs from requested {setting}')
        result['observations'] = observations
        result['timing'] = timing_summary(case, observations)
        if any(observation['reset'] for observation in observations[1:]):
            raise RuntimeError('Temporal history reset during the captured sequence')
        if any(second['frame'] != first['frame'] + args.stride
               for first, second in zip(observations, observations[1:])):
            raise RuntimeError('Captured sequence did not maintain the requested stride')
        result.update(status='captured', captures=len(captures), visual_acceptance='pending inspection')
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'visual_sequence_captured evidence={case}', flush=True)


if __name__ == '__main__':
    main()
