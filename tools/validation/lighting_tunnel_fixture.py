"""Authored isolated stone tunnel and fixed-receiver measurements from real GPU BMPs."""
import json
import statistics
import struct


def prepare(case, bundle, width):
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {block['id'].split('.')[-1]: i for i, block in enumerate(catalog)}
    path = case / 'world/world_blocks.json'
    data = json.loads(path.read_text())
    blocks = {(row['x'], row['y'], row['z']): row['block'] for row in data['blocks']}
    left = 1 - width
    # Two-block-thick enclosure, with a three-block-high interior and an open
    # entrance at negative Z. The player chamber is sealed by the wall at Z=3.
    for x in range(left - 2, 3):
        for y in range(159, 166):
            for z in range(-8, 12):
                interior = left <= x <= 0 and 161 <= y <= 163 and z <= 9
                blocks[x, y, z] = 0 if interior and z != 3 else ids['stone']
    # Replace the open-platform lights with actual placed tunnel light blocks.
    for x in (-4, 0, 4):
        blocks[x, 161, -4] = 0
    blocks[0, 161, 8] = ids['red_torch']
    blocks[0, 161, 1] = ids['white_torch']
    # Production spawn alignment starts above the highest solid in its column.
    # Leave this skylight for spawning; the helper seals it through ordinary
    # authoritative placement commands before the first warmup capture.
    blocks[0, 164, 7] = 0
    blocks[0, 165, 7] = 0
    data['blocks'] = [dict(x=x, y=y, z=z, block=block) for (x, y, z), block in blocks.items()]
    path.write_text(json.dumps(data), encoding='utf-8')
    pose = dict(version=1, x=.5, y=162.62, z=7, pitch=-.1, yaw=0, block=ids['stone'])
    (case / 'world/player_1.json').write_text(json.dumps(pose), encoding='utf-8')
    description = dict(width_blocks=width, height_blocks=3, floor_y=160,
                       interior_x=[left, 0], interior_z=[-8, 9], eye=[.5, 162.62, 7],
                       edit_block=[0, 162, 3], replacement_support=[0, 161, 3],
                       startup_roof=[dict(target=[0, 164, 7], hit=[1, 164, 7]),
                                     dict(target=[0, 165, 7], hit=[0, 164, 7])],
                       lights=[dict(block='red_torch', position=[0, 161, 8]),
                               dict(block='white_torch', position=[0, 161, 1])],
                       scope='Production save fixture; runtime removal and replacement go through authoritative client-interaction commands.')
    (case / 'tunnel_fixture.json').write_text(json.dumps(description, indent=2), encoding='utf-8')
    return description


def receivers(path):
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise RuntimeError(f'Invalid GPU BMP: {path}')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, signed_height, planes, bits = struct.unpack_from('<iiHH', data, 18)
    height = abs(signed_height)
    if width <= 0 or height == 0 or planes != 1 or bits != 32 or len(data) < offset + width*height*4:
        raise RuntimeError('Expected complete production 32-bit BMP')
    regions = dict(floor=(.35, .63, .65, .79), left_wall=(.12, .28, .28, .55), right_wall=(.72, .28, .88, .55))
    result = {}
    for name, (x0, y0, x1, y1) in regions.items():
        samples = []
        for y in range(int(height*y0), int(height*y1), 2):
            row = height-y-1 if signed_height > 0 else y
            for x in range(int(width*x0), int(width*x1), 2):
                address = offset + (row*width+x)*4
                b, g, r = data[address:address+3]
                samples.append((.2126*r+.7152*g+.0722*b)/255)
        result[name] = samples
    return result


def difference(first, second):
    if len(first) != len(second):
        raise RuntimeError('Fixed-receiver dimensions changed')
    absolute = sorted(abs(a-b) for a, b in zip(first, second))
    return dict(mean_absolute=statistics.mean(absolute), p99_absolute=absolute[int(.99*(len(absolute)-1))],
                mean_signed=statistics.mean(b-a for a, b in zip(first, second)))


def inspect_stationary(paths):
    images = [receivers(path) for path in paths[-16:]]
    result = {}
    for name in images[0]:
        means = [statistics.mean(image[name]) for image in images]
        changes = [difference(a[name], b[name]) for a, b in zip(images, images[1:])]
        result[name] = dict(luminance=means, luminance_span=max(means)-min(means),
                            mean_pair_change=statistics.mean(row['mean_absolute'] for row in changes),
                            max_pair_change=max(row['mean_absolute'] for row in changes), pairs=changes)
    return dict(samples=len(images), receivers=result,
                scope='Last sixteen fixed-camera GPU captures; tone-mapped image variation includes all enabled lighting and animated presentation.')


def inspect(paths, edits):
    images = {str(path): receivers(path) for path in paths}
    counters = {str(path): json.loads(path.with_name(path.name + '.lighting.json').read_text()) for path in paths}
    captures = edits['captures']
    phases = []
    for index, command in enumerate(edits['commands']):
        before = [row for row in captures if row['frame'] <= command['last_capture_frame']]
        # Include captures observed in the same poll as server acceptance; the
        # captured scene revision identifies the earliest actual render update.
        after = [row for row in captures if row['frame'] > command['last_capture_frame']]
        if index + 1 < len(edits['commands']):
            next_command = edits['commands'][index+1]
            after = [row for row in after if row['frame'] <= next_command['last_capture_frame']]
        if not before or len(after) < 3:
            raise RuntimeError('Insufficient receiver captures around accepted tunnel edit')
        baseline, first, settled = before[-1], after[0], after[-1]
        baseline_revision = counters[baseline['path']]['scene_revision']
        revised = [row for row in after if counters[row['path']]['scene_revision'] != baseline_revision]
        if not revised:
            raise RuntimeError('Accepted tunnel edit never reached the captured rendering scene revision')
        metrics = {}
        for name in images[baseline['path']]:
            metrics[name] = dict(pixel_count=len(images[baseline['path']][name]),
                                 baseline_luminance=statistics.mean(images[baseline['path']][name]),
                                 first_luminance=statistics.mean(images[first['path']][name]),
                                 first_scene_update_luminance=statistics.mean(images[revised[0]['path']][name]),
                                 settled_luminance=statistics.mean(images[settled['path']][name]),
                                 first_change=difference(images[baseline['path']][name], images[first['path']][name]),
                                 first_scene_update_change=difference(images[baseline['path']][name], images[revised[0]['path']][name]),
                                 settled_change=difference(images[baseline['path']][name], images[settled['path']][name]),
                                 late_frame_changes=[difference(images[a['path']][name], images[b['path']][name])
                                                     for a, b in zip(after[-3:], after[-2:])])
        phases.append(dict(action=command['action'], baseline_frame=baseline['frame'],
                           first_frame=first['frame'], settled_frame=settled['frame'],
                           accepted_observation_interval_seconds=command['acceptance_observation_interval_seconds'],
                           first_scene_revision_frame=revised[0]['frame'],
                           first_scene_revision=counters[revised[0]['path']]['scene_revision'],
                           baseline_scene_revision=baseline_revision, receivers=metrics))
    return dict(phases=phases,
                scope='Fixed stone receiver regions in final tone-mapped GPU frames. Includes direct light, GI, textures and day/night drift; visual inspection must confirm region placement. No GI-only convergence threshold is asserted.')
