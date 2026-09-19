#!/usr/bin/env python3
"""Reproduce foliage-edge sun stripes on an underground wall and measure them."""
import argparse
import json
import os
import struct
import subprocess
import tempfile
from pathlib import Path
from statistics import mean


def write(path, value):
    path.write_text(json.dumps(value), encoding='utf-8')


def read_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return None


def prepare(bundle, case, args):
    case.mkdir(parents=True, exist_ok=True)
    (case / 'world').mkdir()
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {row['id'].split('.')[-1]: i for i, row in enumerate(catalog)}
    # Sealed stone room underground; leaf canopy sits on the surface above it.
    edits = {}
    for x in range(0, 32):
        for z in range(0, 32):
            edits[x, 168, z] = ids['grass']
    for x in range(8, 16):
        for z in range(8, 16):
            for y in range(163, 168):
                edits[x, y, z] = ids['stone']
    for x in range(9, 15):
        for z in range(9, 15):
            for y in range(163, 167):
                edits[x, y, z] = 0
    for x in range(6, 18):
        for z in range(6, 18):
            for y in range(169, 172):
                if not args.no_canopy:
                    edits[x, y, z] = ids['leaves']
    # Sky shaft so the enclosed spawn position is accepted; sealed mid-run.
    for y in range(163, 172):
        edits[12, y, 12] = 0
    write(case / 'world/world_blocks.json', dict(version=1,
        blocks=[dict(x=x, y=y, z=z, block=b) for (x, y, z), b in edits.items()]))
    write(case / 'world/world_generation.json', dict(version=1, generator='octaryn.basegame',
        revision=3, seed=1337, mode=0))
    write(case / 'world/player_1.json', dict(version=1, x=12.5, y=164.62, z=12.5,
        pitch=-.1, yaw=0, block=ids['stone']))
    write(case / 'settings.json', dict(version=10, windowWidth=args.width, windowHeight=args.height,
        fullscreen=False, renderDistance=2, upscalerMode=1, fsrSharpness=.3, fogEnabled=False,
        rayTracingEnabled=True, giVoxelRadius=16, giCoarseRadius=128))
    write(case / 'lighting.json', dict(ambient_strength=.65, sun_strength=.75,
        fog_distance=1024, skylight_floor=.25))
    # Low morning sun: the reported grazing-angle condition.
    write(case / 'world/world_time.json', dict(version=1, day_index=0, seconds_of_day=args.hour * 3600))


def environment(case, args):
    env = {k: v for k, v in os.environ.items() if not k.startswith(('OCTARYN_', 'VK_LAYER'))
           and k not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in (('WORLD', 'world'), ('SETTINGS', 'settings.json'), ('LIGHTING', 'lighting.json'),
                      ('INVENTORY', 'inventory.json'), ('CAPTURE', 'frame.bmp')):
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend, OCTARYN_CLIENT_UPSCALER='native',
               OCTARYN_CLIENT_CAPTURE_COUNT='1', OCTARYN_CLIENT_CAPTURE_STRIDE='120',
               OCTARYN_CLIENT_CAPTURE_TEMPORAL='1', OCTARYN_CLIENT_CAPTURE_TEMPORAL_COUNT='4',
               OCTARYN_CLIENT_LIGHTING_DEBUG=str(args.debug))
    return env


def read_bmp(path):
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise RuntimeError('invalid GPU BMP')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, signed_height, planes, bits = struct.unpack_from('<iiHH', data, 18)
    height = abs(signed_height)
    if planes != 1 or bits != 32:
        raise RuntimeError('expected 32-bit BMP')
    return width, height, data, offset, signed_height > 0


def analyze(path):
    width, height, data, offset, flipped = read_bmp(path)
    # Center crop facing the far wall of the 6x6 room.
    x0, x1 = int(width * .35), int(width * .65)
    y0, y1 = int(height * .30), int(height * .70)
    rows = []
    for y in range(y0, y1):
        row = height - y - 1 if flipped else y
        samples = []
        for x in range(x0, x1):
            address = offset + (row * width + x) * 4
            samples.append((data[address], data[address + 1], data[address + 2],
                            (.2126 * data[address + 2] + .7152 * data[address + 1]
                             + .0722 * data[address]) / 255))
        rows.append(samples)
    lit = [[1 if v[3] > .30 else 0 for v in row] for row in rows]
    stripes = 0
    longest = 0
    for row in lit:
        run = 0
        for value in row + [0]:
            if value:
                run += 1
            else:
                if run >= 3:
                    stripes += 1
                    longest = max(longest, run)
                run = 0
    return dict(width=width, height=height, sampled_rows=len(rows),
                mean_luminance=mean(v[3] for row in rows for v in row),
                mean_direct_red=mean(v[0] for row in rows for v in row),
                mean_nl_green=mean(v[1] for row in rows for v in row),
                mean_visibility_blue=mean(v[2] for row in rows for v in row),
                lit_pixels=sum(v[3] > .30 for row in rows for v in row),
                stripe_runs=stripes, longest_run=longest,
                max_luminance=max(v[3] for row in rows for v in row))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--debug', type=int, default=28)
    parser.add_argument('--hour', type=float, default=7.5)
    parser.add_argument('--no-canopy', action='store_true')
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'stripes-{args.backend}-{args.debug}-',
                                 dir=args.evidence_root.resolve()))
    prepare(args.client_bundle_root, case, args)
    command = [str(args.client_bundle_root / 'Octaryn.Client.exe'), '--frames', '600',
               '--validate-ui', '--benchmark-hidden']
    print(f'stripes_started case={case} hour={args.hour} debug={args.debug}', flush=True)
    runtime = case / 'world/runtime'
    with (case / 'client.log').open('wb') as log:
        result = subprocess.Popen(command, cwd=case, env=environment(case, args),
                                  stdout=log, stderr=subprocess.STDOUT)
        # Let the player settle inside, then seal the sky shaft authoritatively.
        import time
        time.sleep(3)
        stone = ids['stone']        commands = [dict(requestId=i + 1, editX=12, editY=y, editZ=12, block=stone,
                         cameraX=12.5, cameraY=164.62, cameraZ=12.5,
                         hitX=13, hitY=y, hitZ=12) for i, y in enumerate((167, 168))]
        write(runtime / 'block_interaction.json', dict(version=1, frameIndex=1, commands=commands))
        deadline = time.time() + 10
        while time.time() < deadline:
            results = read_json(runtime / 'block_results.json')
            if results and results.get('version') == 1 and len(results.get('receipts', [])) >= 2:
                break
            time.sleep(.2)
        write(case / 'seal.json', dict(sealed=time.time(), receipts=results))
        time.sleep(4)
        try:
            result.wait(timeout=300)
        except subprocess.TimeoutExpired:
            result.kill()
    metrics = analyze(case / 'frame.bmp') if (case / 'frame.bmp').exists() else None
    write(case / 'result.json', dict(status='done' if metrics else 'no-capture',
        returncode=result.returncode, debug=args.debug, hour=args.hour, metrics=metrics))
    print(f'stripes_result case={case} metrics={metrics}')
    if metrics and metrics['stripe_runs'] > 0:
        print('REPRODUCED: clear lit stripes inside the sealed room')


if __name__ == '__main__':
    main()