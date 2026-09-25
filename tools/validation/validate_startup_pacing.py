"""Isolated natural-world startup and explicitly paced-frame qualification."""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import tempfile

from validate_lighting_architecture import record_build, stop_case
from validate_rhi_client_diagnostic import inspect_result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='vulkan')
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    parser.add_argument('--cap', type=int, choices=(30, 60, 120, 144), default=60)
    parser.add_argument('--timeout', type=int, default=900)
    args = parser.parse_args()
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix=f'startup-{args.backend}-', dir=args.evidence_root.resolve()))
    (case / 'world').mkdir()
    # Omit renderDistance deliberately: exercise the shipped first-start default.
    (case / 'settings.json').write_text(json.dumps(dict(
        windowWidth=args.width, windowHeight=args.height, fullscreen=False,
        upscalerMode=1, fsrDynamicResolution=0, frameCapFps=args.cap,
        shadowDistance=128, reflectionDistance=128)), encoding='utf-8')
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('OCTARYN_', 'VK_LAYER'))
           and key not in ('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE')}
    for key, name in (('WORLD', 'world'), ('SETTINGS', 'settings.json'),
                      ('LIGHTING', 'lighting.json'), ('INVENTORY', 'inventory.json'),
                      ('CAPTURE', 'frame.bmp'), ('PROFILE', 'profile.csv')):
        env[f'OCTARYN_CLIENT_{key}_PATH'] = str(case / name)
    env.update(OCTARYN_CLIENT_GRAPHICS_API=args.backend,
               OCTARYN_CLIENT_CAPTURE_COUNT='2', OCTARYN_CLIENT_CAPTURE_STRIDE='17',
               OCTARYN_CLIENT_BOOT_CAPTURE_PATH=str(case / 'startup-loading.bmp'),
               OCTARYN_CLIENT_PACING_PROFILE_PATH=str(case / 'pacing.csv'),
               OCTARYN_CLIENT_LIGHTING_PROFILE_PATH=str(case / 'lighting.csv'))
    command = [str(bundle / 'Octaryn.Client.exe'), '--frames', '180', '--validate-frame-pacing']
    record_build(bundle, case)
    result = dict(status='running', backend=args.backend, command=command,
                  dimensions=[args.width, args.height], requested_cap=args.cap,
                  source='Natural generated world; shipped view-distance default')
    (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'startup_pacing_started evidence={case}', flush=True)
    try:
        with (case / 'client.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=case, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired as error:
                stop_case(process, case)
                raise RuntimeError('Startup/pacing run timed out') from error
        text = (case / 'client.log').read_text(errors='replace')
        frames, columns, quads = inspect_result(code, text, 'D3D12' if args.backend == 'dx12' else 'Vulkan')
        if columns != 81:
            raise RuntimeError(f'First-start view distance did not settle at 81 columns: {columns}')
        if any(marker in text for marker in ('world_frame_failed', 'world_fence_timeout')):
            raise RuntimeError('Renderer failed during the paced session')
        boot = re.search(r'client_boot stage=renderer_ready elapsed_ms=(\d+)', text)
        if not boot:
            raise RuntimeError('Missing measured renderer initialization duration')
        pacing_line = re.search(r'^frame_pacing_summary (.+)$', text, re.MULTILINE)
        if not pacing_line:
            raise RuntimeError('Explicit frame-pacing qualification did not execute')
        pacing = dict(re.findall(r'(\w+)=([\d.]+)', pacing_line[1]))
        if int(pacing['cap']) != args.cap or int(pacing['display_queries']) >= int(pacing['frames']) / 4:
            raise RuntimeError(f'Pacing cap or display-query caching contract failed: {pacing}')
        with (case / 'pacing.csv').open(newline='') as source:
            rows = list(csv.DictReader(source))
        settled = [row for row in rows if int(row['columns']) == 81 and int(row['pending_meshes']) == 0]
        values = sorted(float(row['frame_ms']) for row in settled[len(settled) // 2:])
        if len(values) < 30:
            raise RuntimeError('Too few settled paced frames for measurement')
        if statistics.median(values) < (1000 / args.cap) * .9:
            raise RuntimeError('Measured frames bypass the requested pacing cap')
        captures = sorted(case.glob('frame*.bmp'))
        if len(captures) != 2:
            raise RuntimeError(f'Expected two frame-slot captures; found {len(captures)}')
        result.update(status='measured', frames=frames, columns=columns, quads=quads,
                      renderer_init_ms=int(boot[1]), settled_samples=len(values),
                      pacing=pacing,
                      frame_median_ms=statistics.median(values),
                      frame_p95_ms=values[int((len(values) - 1) * .95)],
                      frame_maximum_ms=max(values), captures=[str(path) for path in captures],
                      visual_inspection='required',
                      acceptance='Measured startup/pacing; lighting requires separate GPU qualification')
    except (OSError, ValueError, RuntimeError) as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f'startup_pacing={result["status"]} evidence={case}', flush=True)


if __name__ == '__main__':
    main()
