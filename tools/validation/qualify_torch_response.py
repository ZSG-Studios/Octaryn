#!/usr/bin/env python3
"""Capture cold/add/remove torch response through isolated authoritative file commands."""
import argparse
import hashlib
import json
import os
import re
import statistics
import struct
import subprocess
import tempfile
import time
from pathlib import Path

from lighting_profile_summary import WORLD_GI_PASSES, read_rows, summarize
from world_gi_capture import assert_world_gi
from lighting_tunnel_fixture import prepare as prepare_tunnel, receivers, difference
from validate_lighting_architecture import environment, fixture, stop_case
from validate_rhi_client_diagnostic import inspect_result


def write(path, value):
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')


def read_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return None


def prepare(bundle, case, args):
    fixture(bundle, case, args)
    tunnel = prepare_tunnel(case, bundle, 2)
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {row['id'].split('.')[-1]: i for i, row in enumerate(catalog)}
    path = case / 'world/world_blocks.json'
    blocks = read_json(path)
    torch_ids = {i for name, i in ids.items() if name.endswith('_torch')}
    for block in blocks['blocks']:
        if block['block'] in torch_ids:
            block['block'] = 0
    write(path, blocks)
    settings = read_json(case / 'settings.json')
    settings.update(frameCapFps=args.frame_cap)
    write(case / 'settings.json', settings)
    write(case / 'world/world_time.json', dict(version=1, day_index=0, seconds_of_day=0))
    actions = [dict(action='seal_roof', target=row['target'], hit=row['hit'], block=ids['stone'], threshold=0)
               for row in tunnel['startup_roof']]
    actions += [dict(action='add', target=[0, 161, 5], hit=[0, 160, 5], block=ids['white_torch'], threshold=12),
                dict(action='remove', target=[0, 161, 5], hit=[0, 161, 5], block=0, threshold=32)]
    write(case / 'fixture.json', dict(tunnel=tunnel, actions=actions, voxel_size_metres=1))
    return actions


def drive(process, case, actions, timeout):
    started = time.monotonic()
    captures, submitted, seen = [], [], set()
    previous_capture_poll = previous_server_poll = 0.0
    server = case / 'world/logs/server/local-session.log'
    runtime = case / 'world/runtime'
    capture_pattern = re.compile(r'world_capture frame=(\d+).*?eye=([\d.-]+),([\d.-]+),([\d.-]+) yaw=([\d.-]+) pitch=([\d.-]+).*?path=([^\r\n]+)\r?\n')
    while process.poll() is None:
        now = time.monotonic() - started
        if now > timeout:
            raise RuntimeError('Torch response runtime deadline exceeded')
        text = (case / 'client.log').read_text(errors='replace')
        capture_now = time.monotonic()-started
        for match in capture_pattern.finditer(text):
            frame = int(match[1])
            if frame not in seen:
                seen.add(frame)
                captures.append(dict(frame=frame, pose=[float(match[i]) for i in range(2, 7)],
                                     path=match[7].strip(), observed_seconds=capture_now,
                                     observation_interval_seconds=[previous_capture_poll, capture_now]))
        log = server.read_text(errors='replace') if server.exists() else ''
        server_now = time.monotonic()-started
        batch = read_json(runtime / 'block_results.json')
        for command in submitted:
            if batch and batch.get('version') == 1:
                for receipt in batch.get('receipts', []):
                    if receipt.get('commandID') == command['request']:
                        command['receipt'] = dict(session=batch.get('session'), **receipt)
                        if not receipt.get('accepted'):
                            raise RuntimeError('Authoritative torch command rejected in receipt')
            if 'accepted_seconds' in command:
                continue
            target = ','.join(map(str, command['target']))
            accepted = rf'server_live_block_command rejected=0 kind=SetBlock request={command["request"]} edit=\w+ applied=1 changed=1 block=\({target},{command["block"]}\)'
            if re.search(accepted, log):
                command['accepted_seconds'] = server_now
                command['acceptance_observation_interval_seconds'] = [previous_server_poll, server_now]
                command['acceptance_capture_count'] = len(captures)
            elif re.search(rf'server_live_client_command_rejected .*request={command["request"]} ', log):
                raise RuntimeError(f'Authoritative command rejected: {command}')
            elif now-command['submitted_seconds'] > 15:
                raise RuntimeError(f'Command acceptance deadline exceeded: {command}')
        if len(submitted) < len(actions) and (not submitted or 'accepted_seconds' in submitted[-1]):
            action = actions[len(submitted)]
            pose = read_json(runtime / 'player_state.json')
            if len(captures) >= action['threshold'] and pose and 162 < pose.get('playerY', 0) < 164:
                request = len(submitted)+1
                x, y, z = action['target']
                hx, hy, hz = action['hit']
                command = dict(requestId=request, editX=x, editY=y, editZ=z, block=action['block'],
                               cameraX=pose['playerX'], cameraY=pose['playerY'], cameraZ=pose['playerZ'],
                               hitX=hx, hitY=hy, hitZ=hz)
                path = runtime / 'block_interaction.json'
                if not path.exists():
                    temporary = path.with_suffix('.torch-response.tmp')
                    write(temporary, dict(version=1, frameIndex=request, commands=[command]))
                    temporary.replace(path)
                    submitted.append(dict(action, request=request, submitted_seconds=now,
                                          last_capture_frame=captures[-1]['frame'] if captures else 0))
        write(case / 'sequence.json', dict(commands=submitted, captures=captures))
        previous_capture_poll, previous_server_poll = capture_now, server_now
        time.sleep(.01)
    if len(submitted) != len(actions) or any('accepted_seconds' not in row for row in submitted):
        raise RuntimeError('Incomplete accepted torch sequence')
    for command in submitted:
        receipt = command.get('receipt', {})
        if not receipt.get('accepted') or not any([cell['x'], cell['y'], cell['z']] == command['target'] and
                                                 cell['block'] == command['block'] for cell in receipt.get('blocks', [])):
            raise RuntimeError('Missing durable accepted receipt with the requested authoritative cell')
    if captures and any(row['accepted_seconds'] >= captures[0]['observed_seconds'] for row in submitted if row['action'] == 'seal_roof'):
        raise RuntimeError('Initial cold capture preceded authoritative roof sealing')
    return submitted, captures


def response_timing(case, command, rows):
    acceptance = command['accepted_seconds']
    accepted_bounds = command.get('acceptance_observation_interval_seconds')
    first = rows[0]
    first_bounds = first.get('observation_interval_seconds')
    ray_events, last_capture = [], None
    by_frame = {row['frame']: row for row in rows}
    # AS statistics are periodic CPU log snapshots, not AS-fence timestamps.
    for line in (case / 'client.log').read_text(errors='replace').splitlines():
        capture = re.search(r'world_capture frame=(\d+)', line)
        if capture:
            frame = int(capture[1])
            for event in ray_events:
                if 'next_capture_frame' not in event:
                    event['next_capture_frame'] = frame
            last_capture = frame
            if frame in by_frame:
                by_frame[frame]['preceding_ray_log'] = dict(ray_events[-1]) if ray_events else None
        elif line.startswith('world_ray ready='):
            fields = {key: int(value) for key, value in re.findall(r'(ready|pending|jobs|scene_generation|blas_builds|tlas_builds|blas_refits|tlas_updates)=(\d+)', line)}
            if not ray_events or fields.get('scene_generation') != ray_events[-1].get('scene_generation'):
                ray_events.append(dict(fields, previous_capture_frame=last_capture))
    return dict(first_empty_capture_frame=first['frame'],
                first_empty_capture_observed_delay_seconds=first['observed_seconds']-acceptance,
                first_empty_capture_delay_observation_bounds_seconds=[first_bounds[0]-accepted_bounds[1], first_bounds[1]-accepted_bounds[0]]
                if first_bounds and accepted_bounds else None,
                final_empty_capture_observed_delay_seconds=rows[-1]['observed_seconds']-acceptance,
                as_generation_log_events=ray_events,
                scope='Observer clock, not GPU timestamps. Capture gate waits for zero AS pending columns/jobs. First empty capture bounds completed rendering, not the exact AS publication instant; historical logs without poll intervals provide point observations only.')


def gi_pass_timings(case):
    path = case / 'lighting.csv'
    if not path.is_file():
        return dict(available=False)
    rows = read_rows(path)
    fields = WORLD_GI_PASSES + ('composition_ms', 'local_shade_ms')
    return dict(available=True, rows=len(rows), steady=summarize(rows, fields),
                scope='GPU timestamps from lighting.csv; steady is the second half of completed rows.')


def measure(case, commands, captures):
    if len(captures) != 64:
        raise RuntimeError(f'Expected 64 GPU captures, got {len(captures)}')
    if {row['frame'] % 2 for row in captures} != {0, 1}:
        raise RuntimeError('GPU sequence did not exercise both frame-slot parities')
    if any(max(row['pose'][i] for row in captures)-min(row['pose'][i] for row in captures) > .0001 for i in range(5)):
        raise RuntimeError('Camera pose changed during cold/add/remove comparison')
    add, remove = [row for row in commands if row['action'] != 'seal_roof']
    phases = dict(cold=[], added=[], removed=[])
    images = {}
    for row in captures:
        path = Path(row['path'])
        row['counters'] = json.loads(Path(str(path)+'.lighting.json').read_text())
        assert_world_gi(row['counters'])
        regions = receivers(path)
        width = struct.unpack_from('<i', path.read_bytes(), 18)[0]
        columns = list(range(int(width*.35), int(width*.65), 2))
        # Keep the same floor receiver but exclude the central torch silhouette.
        regions['floor'] = [value for i, value in enumerate(regions['floor'])
                            if columns[i % len(columns)] < width*.45 or columns[i % len(columns)] > width*.55]
        images[row['frame']] = regions
        count = row['counters']['block_selected_count']
        if row['frame'] <= add['last_capture_frame'] and count == 0:
            phases['cold'].append(row)
        elif add['last_capture_frame'] < row['frame'] <= remove['last_capture_frame'] and count == 1:
            phases['added'].append(row)
        elif row['frame'] > remove['last_capture_frame'] and count == 0:
            phases['removed'].append(row)
    if any(len(rows) < 4 for rows in phases.values()):
        raise RuntimeError('Insufficient GPU-confirmed cold, added, or removed phases')
    if any(row['counters'][key] != 0 for row in phases['removed']
           for key in ('local_light_count', 'block_source_count', 'evaluated_lights', 'local_visibility_rays')):
        raise RuntimeError('Removed phase retained direct lights or direct-light work')
    if not any(row['counters']['local_visibility_rays'] > 0 for row in phases['added']):
        raise RuntimeError('Added torch did not illuminate any actual GPU receiver')
    summary = {}
    for region in images[captures[0]['frame']]:
        cold = [images[row['frame']][region] for row in phases['cold'][-4:]]
        reference = [statistics.mean(pixel) for pixel in zip(*cold)]
        phase_means = {name: statistics.mean(statistics.mean(images[row['frame']][region]) for row in rows[-4:])
                       for name, rows in phases.items()}
        noise = max(difference(reference, image)['mean_absolute'] for image in cold)
        response = phase_means['added']-phase_means['cold']
        curve = [dict(frame=row['frame'], observed_seconds=row['observed_seconds'],
                      seconds_since_acceptance_observed=row['observed_seconds']-remove['accepted_seconds'],
                      seconds_since_first_empty_capture_observed=row['observed_seconds']-phases['removed'][0]['observed_seconds'],
                      **difference(reference, images[row['frame']][region])) for row in phases['removed']]
        ratio = abs(phase_means['removed']-phase_means['cold'])/abs(response) if abs(response) > 1e-6 else None
        entry = dict(luminance=phase_means, added_delta=response, cold_noise=noise,
                     removed_curve=curve, final_residual=statistics.mean(row['mean_absolute'] for row in curve[-4:]),
                     residual_to_added_ratio=ratio)
        summary[region] = entry
    selected = {name: [row['path'] for row in rows[-4:]] for name, rows in phases.items()}
    # PNGs are inspection copies of actual GPU BMPs, never synthetic screenshots.
    try:
        from PIL import Image
        for name, rows in phases.items():
            Image.open(rows[-1]['path']).save(case / f'{name}.png')
    except ImportError:
        pass
    return dict(receivers=summary, phase_captures=selected, phase_frames={name: [row['frame'] for row in rows] for name, rows in phases.items()},
                frame_parities=sorted({row['frame'] % 2 for row in captures}),
                removal_timing=response_timing(case, remove, phases['removed']),
                gi_pass_timings=gi_pass_timings(case),
                gi_statistics=[dict(frame=row['frame'], **{key: value for key, value in row['counters'].items()
                                      if key.startswith('block_transport_')}) for row in captures],
                scope='Tone-mapped fixed-pose GPU receivers; includes lighting/exposure drift. Metrics are measured, not an automatic visual or convergence pass.')


def run_case(bundle, evidence, args):
    case = Path(tempfile.mkdtemp(prefix='torch-transport-', dir=evidence))
    actions = prepare(bundle, case, args)
    env = environment(case, args)
    env.update(OCTARYN_CLIENT_CAPTURE_COUNT='64', OCTARYN_CLIENT_CAPTURE_STRIDE=str(args.capture_stride),
               OCTARYN_CLIENT_CAPTURE_MIN_FRAME=str(args.warmup_frames), OCTARYN_CLIENT_CAPTURE_STABLE_FRAMES='0',
               OCTARYN_SERVER_START_HOUR='0')
    if os.environ.get('TUNNEL_DEBUG_VIEW'):
        env['OCTARYN_CLIENT_LIGHTING_DEBUG'] = os.environ['TUNNEL_DEBUG_VIEW']
    command = [str(bundle / 'Octaryn.Client.exe'), '--frames', str(args.frames),
               '--validate-ui', '--benchmark-hidden', '--validate-lighting-edits']
    if args.frame_cap:
        command.append('--validate-frame-pacing')
        env['OCTARYN_CLIENT_PACING_PROFILE_PATH'] = str(case / 'pacing.csv')
    result = dict(status='running', case=str(case), voxel_size_metres=1,
                  gi='voxel-rgb4-sky4', rhi_validation=not args.no_rhi_validation,
                  pacing_requested=dict(frame_cap_fps=args.frame_cap, capture_stride_frames=args.capture_stride,
                                        warmup_frames=args.warmup_frames,
                                        scope='Requested settings, not measured frame pacing. Native capture minimum is clamped to at least 120 frames.'),
                  command=command, client_sha256=hashlib.sha256((bundle / 'Octaryn.Client.exe').read_bytes()).hexdigest())
    write(case / 'shader-sha256.json', {str(path.relative_to(bundle)): hashlib.sha256(path.read_bytes()).hexdigest()
                                      for path in (bundle / 'Client/Shaders').rglob('*.slang')})
    print(f'torch_response_started evidence={case}', flush=True)
    try:
        with (case / 'client.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=case, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                commands, captures = drive(process, case, actions, args.timeout)
            except Exception:
                stop_case(process, case)
                raise
        text = (case / 'client.log').read_text(errors='replace')
        counts = inspect_result(process.returncode, text, 'D3D12' if args.backend == 'dx12' else 'Vulkan', min(600, args.frames))
        metrics = measure(case, commands, captures)
        result.update(status='measured', frames=counts[0], measurements=metrics,
                      pacing_observed=dict(capture_frame_span=captures[-1]['frame']-captures[0]['frame'],
                                           capture_observer_span_seconds=captures[-1]['observed_seconds']-captures[0]['observed_seconds'],
                                           capture_span_average_fps=(captures[-1]['frame']-captures[0]['frame']) /
                                           (captures[-1]['observed_seconds']-captures[0]['observed_seconds']),
                                           profile_csv=str(case / 'profile.csv'),
                                           scope='Average renderer-frame rate between first and last observed capture; includes polling delay, capture stalls and rendering. Not instantaneous pacing or GPU frame time.'),
                      commands=commands, visual_inspection='required', os_events_injected=0)
        print(f'torch_response_measured evidence={case}', flush=True)
    except Exception as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        write(case / 'result.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--frames', type=int, default=3200)
    parser.add_argument('--frame-cap', type=int, default=30, help='Frame cap for isolated hidden qualification')
    parser.add_argument('--capture-stride', type=int, default=31, help='Odd capture interval in frames, within native range 1..119')
    parser.add_argument('--warmup-frames', type=int, default=600, help='Requested first capture frame; native minimum is 120')
    parser.add_argument('--no-rhi-validation', action='store_true',
                        help='Omit the RHI debug layer when measuring production performance')
    parser.add_argument('--timeout', type=int, default=420)
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    args = parser.parse_args()
    if not 1 <= args.frame_cap <= 60:
        parser.error('--frame-cap must be within 1..60')
    if args.capture_stride < 1 or args.capture_stride > 119 or args.capture_stride % 2 == 0:
        parser.error('--capture-stride must be odd and within 1..119')
    if not 0 <= args.warmup_frames <= 10000:
        parser.error('--warmup-frames must be within the native range 0..10000')
    minimum_frames = max(120, args.warmup_frames)+63*args.capture_stride+120
    if args.frames < minimum_frames:
        parser.error(f'--frames must be at least {minimum_frames} for warmup + 63 capture intervals + 120-frame margin')
    args.quality, args.upscaler, args.debug = 'high', 'native', 0
    args.block_lights, args.vegetation_shadows, args.resize, args.captures = True, False, False, 64
    bundle = args.client_bundle_root.resolve()
    evidence = args.evidence_root.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    results = [run_case(bundle, evidence, args)]
    write(evidence / 'torch-response-summary.json', results)


if __name__ == '__main__':
    main()
