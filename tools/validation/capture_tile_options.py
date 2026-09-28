"""Explicit tiled inputs and settled-window evidence for map capture comparisons."""
import csv
import re

from tile_input_evidence import record_inputs
from tile_metric_records import TileMetricRecords, REQUIRED
from performance_summary import read_profile, distribution


def add_tile_options(parser):
    parser.add_argument('--tile-gpu-budget-mib', type=int,
                        help='Explicit tiled owner budget; OS 70-percent admission remains active')
    parser.add_argument('--require-all-tiles', action='store_true')
    parser.add_argument('--settled-ready-window', type=int, nargs=2, metavar=('START', 'COUNT'),
                        help='Exact ready-frame window after a complete pre-window capture')


def prepare_tile_options(args, manifest_path, manifest, case, env):
    budget = args.tile_gpu_budget_mib
    files = manifest.get('tile_files', [])
    if budget is not None and (not files or not 64 <= budget <= 32768):
        raise ValueError('Tile GPU budget requires a tiled manifest and64..32768 MiB')
    if args.require_all_tiles and (not files or not args.captures):
        raise ValueError('All-tile qualification requires a tiled manifest and a capture')
    window = args.settled_ready_window
    if window and (window[0] < 180 or window[1] < 1 or sum(window) > args.frames or
                   args.capture_min_frame + max(0, args.captures - 1) * args.stride + 4 >= window[0] or
                   not args.captures or args.camera_motion):
        raise ValueError('Settled static window requires an earlier capture, positive count and bounded frames')
    if budget is not None:
        env['OCTARYN_CLIENT_TILE_GPU_BUDGET_MIB'] = str(budget)
    if files:
        record_inputs(manifest_path, case / 'tile-inputs.json')
    return dict(enabled=bool(files), tile_count=len(files), gpu_budget_mib=budget,
                require_all_tiles=args.require_all_tiles, settled_ready_window=window,
                tile_geometry_metadata_receipt='tile-inputs.json' if files else None,
                payload_identity_scope='DDS receipts only; separate actual-payload preflight required',
                loading_cache_state='unknown; pre-run identity hashing may warm files')


def fields(line):
    result = {}
    for token in line.split()[1:]:
        match = re.fullmatch(r'(\w+)=(\d+)', token)
        if not match or match[1] in result:
            raise ValueError('Malformed or duplicate capture-state field')
        result[match[1]] = int(match[2])
    required(result, {'frame', 'resident', 'wanted', 'preparing', 'uploading', 'generation'})
    return result


def required(row, names):
    missing = set(names) - row.keys()
    if missing:
        raise ValueError(f'Missing qualification fields: {sorted(missing)}')


def inspect_tile_options(case, metadata, log, captures):
    result = dict(metadata)
    count = metadata['tile_count']
    records = []
    if metadata['enabled']:
        parser = TileMetricRecords()
        for index, line in enumerate(log.splitlines()):
            if line.split(' ', 1)[0] in REQUIRED:
                marker, row = parser.parse(line, index + 1)
                records.append((index, marker, row))
        budget_samples = [row['os_budget_available'] for _, marker, row in records
                          if marker == 'tile_resource_setup' and 'os_budget_available' in row]
        result['os_budget_telemetry'] = dict(observations=len(budget_samples),
            available=all(value == 1 for value in budget_samples) if budget_samples else None,
            scope='Observed resource setup samples only; logical budget is separate')
    if metadata['enabled']:
        budgets = re.findall(r'^tile_budget gpu_bytes=(\d+) ', log, re.M)
        expected = metadata['gpu_budget_mib']
        if not budgets or (expected is not None and any(int(v) != expected * 1024**2 for v in budgets)):
            raise ValueError('Missing or mismatched tile GPU budget activation')
        result['actual_gpu_budget_bytes'] = int(budgets[-1])
    if metadata['require_all_tiles']:
        published = [row for _, marker, row in records if marker == 'tile_published']
        if {row.get('id') for row in published} != set(range(count)) or any(
                row.get('ray_ready') != 1 or row.get('collision_ready') != 1 for row in published):
            raise ValueError('Every tile must publish ready collision and BLAS')
        capture_states = {}
        for line in log.splitlines():
            if line.startswith('world_capture_tiles '):
                row = fields(line)
                if row['frame'] in capture_states:
                    raise ValueError('Duplicate capture-state frame')
                capture_states[row['frame']] = row
        for capture in captures:
            frame = int(capture['lighting']['render_frame'])
            row = capture_states.get(frame, {})
            if row.get('resident') != count or row.get('wanted') != count or row['preparing'] or row['uploading']:
                raise ValueError('Capture did not contain every resident manifest tile')
            if not capture['lighting'].get('ray_enabled'):
                raise ValueError('Full-tile capture did not have active rays')
        if not captures:
            raise ValueError('Missing full-tile capture')
        frame = int(captures[0]['lighting']['render_frame'])
        lines = log.splitlines()
        start = next((i for i, line in enumerate(lines) if line.startswith(f'world_capture_tiles frame={frame} ')), None)
        if start is None:
            raise ValueError('Missing complete capture marker')
        generations = {capture_states[frame].get('generation')}
        if None in generations:
            raise ValueError('Missing capture resident generation')
        post_counts = {'tile_stream': 0, 'tile_ray_schedule': 0}
        for index, marker, row in records:
            if index <= start:
                continue
            if marker in post_counts:
                post_counts[marker] += 1
            if marker == 'tile_stream':
                required(row, {'frame', 'generation', 'retired_bytes', 'reserved_bytes', 'evicted', 'cancelled'})
                if any(row[key] for key in ('preparing', 'uploading', 'retired_bytes', 'reserved_bytes')) or \
                        row.get('resident') != count or row.get('wanted') != count:
                    raise ValueError('Residency changed after the complete capture')
                generations.add(row.get('generation'))
            if marker == 'tile_upload' and row['bytes']:
                raise ValueError('Active texture/geometry upload after complete capture')
            if marker == 'tile_ray_schedule' and any(row[key] for key in
                    ('operations', 'submissions', 'inflight', 'published', 'cancelled')):
                raise ValueError('Active AS lifecycle after complete capture')
        if len(generations) > 1:
            raise ValueError('Resident generation changed after complete capture')
        if not all(post_counts.values()):
            raise ValueError('Missing post-capture residency or AS lifecycle observations')
        result['post_capture_records'] = post_counts
        result['residency_evidence_scope'] = ('All emitted records after complete capture through process end; '
            'tile pump ordinals are not renderer frames; periodic records do not prove unlogged work absent')
        result['all_manifest_ready_capture_frame'] = frame
    window = metadata['settled_ready_window']
    if window:
        start, count_frames = window
        with (case / 'camera-motion.csv').open(newline='') as source:
            camera = list(csv.DictReader(source))
        selected = [row for row in camera if start <= int(row['ready_frame']) < start + count_frames]
        if [int(row['ready_frame']) for row in selected] != list(range(start, start + count_frames)):
            raise ValueError('Missing or duplicate settled ready-frame observations')
        if any(row['phase'] != 'static' for row in selected):
            raise ValueError('Settled comparison is static only')
        poses = {tuple(row[key] for key in ('eye_x', 'eye_y', 'eye_z', 'yaw', 'pitch')) for row in selected}
        if len(poses) != 1:
            raise ValueError('Static camera changed in settled window')
        frames = [int(row['frame']) for row in selected]
        if any(int(capture['lighting']['render_frame']) + 3 >= frames[0] for capture in captures):
            raise ValueError('Capture readback overlaps settled measurement')
        profiles = {name: read_profile(case / filename) for name, filename in
                    [('gpu', 'gpu.csv'), ('cpu', 'frame-timing.csv'), ('lighting', 'lighting.csv')]}
        for profile in profiles.values():
            if any(frame not in profile for frame in frames):
                raise ValueError('Missing exact-window profile frame')
        gpu = profiles['gpu']
        for frame in frames:
            row = gpu[frame]
            required(row, {'external_as_covered', 'external_as_submissions', 'external_as_ms'})
            if row['schema_version'] != 4 or row['external_as_covered'] != 1 or \
                    row['external_as_submissions'] != 0 or row['external_as_ms'] != 0:
                raise ValueError('Uncovered GPU evidence or active AS in settled window')
        measured = {name: {key: distribution([profile[frame][key] for frame in frames])
                    for key in profile[frames[0]] if key.endswith(('_ms', '_bytes'))}
                    for name, profile in profiles.items()}
        result['settled_window'] = dict(ready_start=start, count=count_frames, renderer_frames=frames,
                                        camera=list(next(iter(poses))), startup_included=False,
                                        measured=measured, scope='Exact selected renderer frames only')
    return result
