"""Prepare or capture an isolated revision-3 forest using the production client."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace

from validate_lighting_architecture import environment, stop_case
from validate_rhi_client_diagnostic import inspect_result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path,
                        default=Path('build/release-windows/client/bundle'))
    parser.add_argument('--scene', type=Path, default=Path('logs/server/vegetation-scene.json'))
    parser.add_argument('--evidence-root', type=Path, default=Path('logs/client'))
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--run-case', type=Path)
    args = parser.parse_args()
    if args.run_case:
        case = args.run_case.resolve()
        report = json.loads((case / 'result.json').read_text())
        if report['status'] != 'prepared':
            raise RuntimeError('Only a prepared case may be run; preserve earlier evidence')
    else:
        scene = json.loads(args.scene.read_text(encoding='utf-8-sig'))
        if scene['revision'] != 3 or scene['seed'] != 1337 or scene['biome'] != 'Forest':
            raise RuntimeError('Scene must come from the revision-3 production forest sampler')
        args.evidence_root.mkdir(parents=True, exist_ok=True)
        case = Path(tempfile.mkdtemp(prefix=f'natural-vegetation-{args.backend}-',
                                     dir=args.evidence_root.resolve()))
        world = case / 'world'
        world.mkdir()
        values = {
            world / 'world_generation.json': dict(version=1, generator='octaryn.basegame',
                                                   revision=3, seed=1337, mode=0),
            world / 'player_1.json': dict(version=1, block=1,
                **{key: scene[key] for key in ('x', 'y', 'z', 'pitch', 'yaw')}),
            case / 'settings.json': dict(version=10, windowWidth=1280, windowHeight=720,
                fullscreen=False, renderDistance=4, upscalerMode=1, fogEnabled=False,
                cloudsEnabled=False, rayTracingEnabled=True),
        }
        for path, value in values.items():
            path.write_text(json.dumps(value, indent=2), encoding='utf-8')
        report = dict(status='prepared', backend=args.backend, scene=scene,
                      bundle=str(args.client_bundle_root.resolve()), authored_blocks=0)
        (case / 'result.json').write_text(json.dumps(report, indent=2))
    print(f'natural_vegetation_case={case}', flush=True)
    if args.prepare_only:
        return
    options = SimpleNamespace(backend=report['backend'], upscaler='native', quality='high',
                              debug=0, captures=4, block_lights=True, resize=False)
    env = environment(case, options)
    # No diagnostic light or authored voxel fixture: capture the actual natural generator.
    env.pop('OCTARYN_CLIENT_LIGHTING_FIXTURE', None)
    command = [str(Path(report['bundle']) / 'Octaryn.Client.exe'), '--frames', '600',
               '--validate-ui', '--benchmark-hidden']
    report.update(status='running', command=command)
    try:
        with (case / 'client.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=case, env=env, stdout=log,
                                       stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=240)
            except subprocess.TimeoutExpired:
                stop_case(process, case)
                raise RuntimeError('Natural vegetation capture timed out')
        text = (case / 'client.log').read_text(errors='replace')
        report['diagnostic'] = inspect_result(code, text,
            'D3D12' if report['backend'] == 'dx12' else 'Vulkan', minimum_frames=180)
        if not (case / 'frame.bmp').is_file():
            raise RuntimeError('Missing actual GPU frame capture')
        identity = json.loads((case / 'world/world_generation.json').read_text())
        if identity['revision'] != 3:
            raise RuntimeError('Runtime did not preserve revision-3 world identity')
        report.update(status='captured', visual_inspection='pending')
    except Exception as error:
        report.update(status='failed', error=str(error))
        raise
    finally:
        (case / 'result.json').write_text(json.dumps(report, indent=2))
    print(f'natural_vegetation_capture=ready_for_visual_inspection evidence={case}', flush=True)


if __name__ == '__main__':
    main()
