#!/usr/bin/env python3
"""Render an isolated elevated lake and timber frame with the production client."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from validate_rhi_client_diagnostic import inspect_result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), required=True)
    parser.add_argument('--ray-tracing', choices=('on', 'off'), default='on')
    args = parser.parse_args()
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'lake-{args.backend}-{args.ray_tracing}-',
                                dir=args.evidence_root.resolve()))
    world = case / 'world'
    world.mkdir()
    catalog = json.loads((bundle / 'Data/Blocks/octaryn.basegame.blocks.json').read_text())['blocks']
    ids = {block['id'].split('.')[-1]: index for index, block in enumerate(catalog)}
    edits = {}
    for z in range(-25, 15):
        for x in range(-19, 20):
            edits[x, 160, z] = ids['stone']
            for y in range(161, 174):
                edits[x, y, z] = ids['water'] if y == 161 and -15 <= x <= 15 and -21 <= z <= 1 else 0
    for x in (-7, 7):
        for y in range(162, 169):
            for z in (-15, -14):
                edits[x, y, z] = ids['log']
    for x in range(-8, 9):
        for z in (-15, -14):
            edits[x, 169, z] = ids['planks']
    for x in range(-2, 3):
        for z in range(-17, -12):
            edits[x, 171, z] = ids['leaves']
    values = {
        world / 'world_blocks.json': dict(version=1, blocks=[dict(x=x, y=y, z=z, block=block)
                                                            for (x, y, z), block in edits.items()]),
        world / 'world_generation.json': dict(version=1, generator='octaryn.basegame', revision=2, seed=1337, mode=0),
        world / 'player_1.json': dict(version=1, x=0, y=162.62, z=7, pitch=-.18, yaw=0, block=14),
        case / 'settings.json': dict(version=10, windowWidth=1280, windowHeight=720, fullscreen=False,
                                    renderDistance=4, upscalerMode=1, fsrSharpness=.3,
                                    fogEnabled=False, rayTracingEnabled=args.ray_tracing == 'on'),
    }
    for path, value in values.items():
        path.write_text(json.dumps(value), encoding='utf-8')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('OCTARYN_', 'VK_LAYER'))
           and k not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in [('WORLD', 'world'), ('SETTINGS', 'settings.json'), ('LIGHTING', 'lighting.json'),
                      ('INVENTORY', 'inventory.json'), ('CAPTURE', 'frame.bmp'), ('PROFILE', 'profile.csv')]:
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend, OCTARYN_CLIENT_UPSCALER='native',
               OCTARYN_CLIENT_RHI_VALIDATION='1', OCTARYN_CLIENT_CAPTURE_TEMPORAL='1',
               OCTARYN_CLIENT_RAY_TRACING='required' if args.ray_tracing == 'on' else 'off')
    if args.backend == 'vulkan':
        layers = Path(__file__).resolve().parents[2] / 'build/dependencies/vulkan-validation'
        env.update(VK_LAYER_PATH=str(layers), VK_LAYER_SETTINGS_PATH=str(layers),
                   VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation')
    print(f'ray_lighting_started evidence={case}', flush=True)
    with (case / 'client.log').open('wb') as log:
        run = subprocess.run([str(bundle / 'Octaryn.Client.exe'), '--frames', '1800',
                              '--validate-ui', '--benchmark-hidden'], cwd=case, env=env,
                             stdout=log, stderr=subprocess.STDOUT, timeout=150)
    text = (case / 'client.log').read_text(errors='replace')
    counts = inspect_result(run.returncode, text, 'D3D12' if args.backend == 'dx12' else 'Vulkan')
    if not (case / 'frame.bmp').is_file():
        raise RuntimeError('Missing production GPU capture')
    if args.ray_tracing == 'on' and not re.search(r'world_ray ready=81 pending=0 jobs=0', text):
        raise RuntimeError('Ray scene never reached complete fixture coverage')
    if re.search(r'rhi_validation severity=(?:error|warning)', text):
        raise RuntimeError('Native graphics validation reported an error or warning')
    result = dict(status='passed', backend=args.backend, ray_tracing=args.ray_tracing,
                  frames=counts[0], columns=counts[1], quads=counts[2],
                  capture=str(case / 'frame.bmp'), visual_inspection='required')
    (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'ray_lighting=passed evidence={case}', flush=True)


if __name__ == '__main__':
    main()
