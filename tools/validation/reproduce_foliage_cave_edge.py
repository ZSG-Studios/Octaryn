#!/usr/bin/env python3
"""Run the grazing-sun foliage-over-cave RT visibility regression fixture."""
import argparse
import json
import struct
import subprocess
import sys
from pathlib import Path


VALIDATION = Path(__file__).resolve().parent
sys.path.insert(0, str(VALIDATION))
from lighting_tunnel_fixture import prepare as prepare_tunnel  # noqa: E402
from validate_lighting_architecture import fixture  # noqa: E402


def read_bmp(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, signed_height, planes, bits = struct.unpack_from('<iiHH', data, 18)
    if planes != 1 or bits != 32:
        raise RuntimeError('expected a 32-bit GPU capture')
    return data, offset, width, abs(signed_height), signed_height > 0


def stray_sun_pixels(path):
    data, offset, width, height, bottom_up = read_bmp(path)
    x0, x1 = int(width * .30), int(width * .70)
    y0, y1 = int(height * .53), int(height * .85)
    count = 0
    for y in range(y0, y1):
        row = height - y - 1 if bottom_up else y
        for x in range(x0, x1):
            b, g, r, _ = data[offset + (row * width + x) * 4:offset + (row * width + x + 1) * 4]
            if (r + g + b) / 3 > 8:
                count += 1
    return count


def prepare_case(bundle, case, width, height, hour):
    args = type('FixtureArgs', (), dict(width=width, height=height,
                                        quality='high', vegetation_shadows=True,
                                        block_lights=False))()
    fixture(bundle, case, args)
    prepare_tunnel(case, bundle, 2)
    settings_path = case / 'settings.json'
    settings = json.loads(settings_path.read_text())
    settings.update(shadowDistance=1024, reflectionDistance=1024,
                    rayTracingEnabled=True)
    settings_path.write_text(json.dumps(settings))
    (case / 'lighting.json').write_text(json.dumps({
        'version': 1, 'ambient_strength': .65, 'sun_strength': .75,
        'sun_fallback_strength': 1, 'fog_distance': 1024, 'skylight_floor': .25}))
    (case / 'world/world_time.json').write_text(json.dumps({
        'version': 1, 'day_index': 0, 'seconds_of_day': hour * 3600}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--hour', type=float, default=9.0)
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    args = parser.parse_args()
    if abs(args.hour - 9.0) > .01:
        raise SystemExit('the regression fixture requires the 09:00 grazing sun angle')
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = args.evidence_root / 'source-case'
    if case.exists():
        import shutil
        shutil.rmtree(case)
    case.mkdir()
    prepare_case(args.client_bundle_root.resolve(), case, args.width, args.height, args.hour)
    command = [sys.executable, str(VALIDATION / 'capture_visual_sequence.py'),
               '--client-bundle-root', str(args.client_bundle_root), '--source-case', str(case),
               '--evidence-root', str(args.evidence_root), '--backend', args.backend,
               '--upscaler', 'native', '--captures', '3', '--stride', '17', '--world-edits',
               '--debug', '28', '--width', str(args.width), '--height', str(args.height)]
    subprocess.run(command, check=True)
    captures = sorted(path for path in args.evidence_root.glob('visual-*/frame.bmp'))
    if not captures:
        raise RuntimeError('the client did not produce a direct-sun capture')
    pixels = stray_sun_pixels(captures[-1])
    blocks = json.loads((case / 'world/world_blocks.json').read_text())['blocks']
    canopy = sum(row['block'] != 0 and row['y'] in (167, 168) for row in blocks)
    result = dict(status='passed' if pixels == 0 and canopy else 'failed', hour=args.hour,
                  canopy_blocks=canopy, stray_sun_pixels=pixels, capture=str(captures[-1]))
    (args.evidence_root / 'result.json').write_text(json.dumps(result, indent=2))
    if result['status'] != 'passed':
        raise SystemExit(f'foliage_cave_edge_failed={result}')
    print(f"foliage_cave_edge=passed hour={args.hour:.2f} canopy_blocks={canopy} stray_sun_pixels=0")


if __name__ == '__main__':
    main()
