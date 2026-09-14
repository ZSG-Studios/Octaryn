"""Sealed underground receiver crossing both DDGI cascades during camera motion."""
import json
import statistics
import struct


def prepare(case, bundle):
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {block['id'].split('.')[-1]: i for i, block in enumerate(catalog)}
    floor = -34
    blocks = {}
    # A wide, long chamber allows the existing eight-metre lateral sweep to stay
    # entirely in air. Its far wall crosses the coarse cascade fade region.
    for x in range(-11, 12):
        for y in range(floor-2, floor+6):
            for z in range(-39, 14):
                air = -9 <= x <= 9 and floor < y < floor+4 and -37 <= z <= 11
                blocks[x, y, z] = 0 if air else ids['stone']
    for x, z, name in [(-6, -8, 'red_torch'), (6, -8, 'blue_torch'), (0, 9, 'white_torch')]:
        blocks[x, floor+1, z] = ids[name]
    # Spawn alignment needs a shaft; seal it via authoritative commands before
    # warmup captures. No terrain above the chamber remains visible to its probes.
    for y in range(floor+4, 256):
        blocks[0, y, 7] = 0
    world = case / 'world'
    (world / 'world_blocks.json').write_text(json.dumps(dict(version=1, blocks=[
        dict(x=x, y=y, z=z, block=value) for (x, y, z), value in blocks.items()])))
    (world / 'player_1.json').write_text(json.dumps(dict(version=1, x=.5, y=floor+2.62,
        z=7, yaw=0, pitch=-.08, block=ids['stone'])))
    result = dict(floor_y=floor, eye=[.5, floor+2.62, 7],
        startup_roof=[dict(target=[0, floor+4, 7], hit=[1, floor+4, 7]),
                      dict(target=[0, floor+5, 7], hit=[0, floor+4, 7])],
        scope='Actual signed-Y underground chamber with authoritative roof closure and moving camera; no injected OS input.')
    (case / 'cave_fixture.json').write_text(json.dumps(result, indent=2))
    return result


def lower_tunnel(case, tunnel):
    """Move the digging fixture underground without changing its local geometry."""
    floor = -34
    offset = floor - tunnel['floor_y']
    world = case / 'world'
    path = world / 'world_blocks.json'
    data = json.loads(path.read_text())
    # Keep only the actual tunnel; the original exterior display platform would
    # collide with the underground chamber after translation.
    left = tunnel['interior_x'][0]
    data['blocks'] = [row for row in data['blocks']
        if left-2 <= row['x'] <= 2 and 159 <= row['y'] <= 165 and -8 <= row['z'] <= 11]
    for row in data['blocks']:
        row['y'] += offset
    for y in range(floor+6, 256):
        data['blocks'].append(dict(x=0, y=y, z=7, block=0))
    path.write_text(json.dumps(data))
    pose_path = world / 'player_1.json'
    pose = json.loads(pose_path.read_text())
    pose['y'] += offset
    pose_path.write_text(json.dumps(pose))
    tunnel['floor_y'] = floor
    tunnel['eye'][1] += offset
    for key in ('edit_block', 'replacement_support'):
        tunnel[key][1] += offset
    for row in tunnel['startup_roof']:
        row['target'][1] += offset
        row['hit'][1] += offset
    for row in tunnel['lights']:
        row['position'][1] += offset
    tunnel['underground'] = True
    (case / 'tunnel_fixture.json').write_text(json.dumps(tunnel, indent=2))


def inspect_far_wall(paths):
    """This sealed wall is beyond probe coverage and all local-light ranges."""
    means = []
    for path in paths:
        data = path.read_bytes()
        if data[:2] != b'BM':
            raise RuntimeError('Expected actual cave GPU BMP')
        offset = struct.unpack_from('<I', data, 10)[0]
        width, signed_height, planes, bits = struct.unpack_from('<iiHH', data, 18)
        height = abs(signed_height)
        if planes != 1 or bits != 32 or len(data) < offset+width*height*4:
            raise RuntimeError('Incomplete cave GPU capture')
        samples = []
        for y in range(int(height*.445), int(height*.47)):
            row = height-y-1 if signed_height > 0 else y
            for x in range(int(width*.45), int(width*.55)):
                address = offset+(row*width+x)*4
                b, g, r = data[address:address+3]
                samples.append((.2126*r+.7152*g+.0722*b)/255)
        means.append(statistics.mean(samples))
    if max(means) > .02:
        raise RuntimeError(f'Sealed far wall regained unoccluded ambient: {max(means):.6f}')
    return dict(mean_luminance=means, maximum=max(means), limit=.02,
                scope='Fixed far-wall ROI across both frame slots during lateral motion; checks false ambient in this fully enclosed fixture.')
