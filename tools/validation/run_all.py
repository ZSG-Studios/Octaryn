#!/usr/bin/env python3
"""One-command validation loop: build, smoke, networking, GPU and unit checks.

Runs the repaired validation surface end to end and prints a compact summary.
Individual steps keep writing their usual evidence under logs/<owner>/.

Usage:
    python tools/validation/run_all.py [--preset release-windows] [--skip-build]
                                       [--only STEP[,STEP...]]
Steps: build, unit, smoke, rejoin, temporal, rml, pacing, visual, patches,
       wire, abi, shader-bundle, static
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
VALIDATION = ROOT / 'tools/validation'

STEPS = ('build', 'unit', 'smoke', 'rejoin', 'temporal', 'rml', 'pacing',
         'visual', 'patches', 'wire', 'abi', 'shader-bundle', 'static')


def build_env():
    env = dict(os.environ)
    if os.name == 'nt':
        env.setdefault('ProgramFiles', r'C:\Program Files')
        env.setdefault('ProgramFiles(x86)', r'C:\Program Files (x86)')
        dotnet = Path(env['ProgramFiles']) / 'dotnet'
        if dotnet.is_dir():
            env['PATH'] = str(dotnet) + os.pathsep + env.get('PATH', '')
    return env


def run(name, command, timeout, env, cwd=ROOT):
    started = time.monotonic()
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                            text=True, timeout=timeout)
    lines = (result.stdout + result.stderr).strip().splitlines()
    return dict(step=name, code=result.returncode,
                seconds=round(time.monotonic() - started, 1),
                tail=lines[-3:] if lines else [])


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--preset', default='release-windows')
    parser.add_argument('--skip-build', action='store_true')
    parser.add_argument('--only', type=lambda value: value.split(','))
    parser.add_argument('--timeout', type=int, default=300,
                        help='per-step timeout in seconds')
    args = parser.parse_args()
    unknown = set(args.only or ()) - set(STEPS)
    if unknown:
        parser.error(f'unknown steps: {sorted(unknown)}')

    env = build_env()
    bundle = ROOT / 'build' / args.preset / 'client/bundle'
    server_bundle = ROOT / 'build' / args.preset / 'server/bundle'
    cmake_dir = ROOT / 'build' / args.preset / 'cmake'
    py = sys.executable

    def v(script, *arguments):
        return [py, str(VALIDATION / script), *arguments]

    plan = {
        'build': (['python', str(ROOT / 'tools/build/windows.py'), '--action', 'build',
                   '--preset', args.preset, '--target', 'octaryn_all'], args.timeout),
        'unit': ([py, '-m', 'unittest', 'discover', '-s', 'tools/validation',
                  '-p', 'test_*.py'], args.timeout),
        'smoke': (v('capture_map_world.py', '--client-bundle-root', bundle,
                    '--evidence-root', ROOT / 'logs/client/map-smoke'), args.timeout),
        'rejoin': (v('validate_session_rejoin.py', '--client-bundle', bundle,
                     '--server-bundle', server_bundle,
                     '--evidence-root', ROOT / 'logs/server/rejoin'), args.timeout),
        'temporal': (v('validate_temporal.py', '--client-bundle-root', bundle,
                       '--evidence-root', ROOT / 'logs/client/temporal',
                       '--backend', 'dx12', '--frames-in-flight', '2'), args.timeout),
        'rml': (v('validate_rml_ui.py', '--client-bundle-root', bundle,
                  '--evidence-root', ROOT / 'logs/client/rml-ui'), 3 * args.timeout),
        'pacing': (v('validate_startup_pacing.py', '--client-bundle-root', bundle,
                     '--evidence-root', ROOT / 'logs/client/pacing'), args.timeout),
        'visual': (v('capture_visual_sequence.py', '--client-bundle-root', bundle,
                     '--evidence-root', ROOT / 'logs/client/visual-seq',
                     '--source-case', 'latest-smoke', '--captures', '3',
                     '--backend', 'dx12'), args.timeout),
        'patches': (v('validate_slang_rhi_patches.py'), args.timeout),
        'wire': (v('validate_remote_wire.py', '--repo-root', ROOT), 60),
        'abi': (v('validate_native_abi_contracts.py', '--repo-root', ROOT), 60),
        'shader-bundle': (v('validate_client_shader_bundle.py', '--source-root',
                            ROOT / 'octaryn-client/Shaders',
                            '--bundle-shader-root', bundle / 'Client/Shaders'), 60),
        'static': (v('validate_cmake_target_inventory.py', '--repo-root', ROOT,
                     '--build-dir', cmake_dir), 120),
    }

    selected = args.only or [step for step in STEPS if not (args.skip_build and step == 'build')]
    results = []
    smoke_case = None
    for step in selected:
        if step == 'visual' and smoke_case is None:
            # The visual sequence copies player/settings evidence from the latest smoke case.
            smoke_root = ROOT / 'logs/client/map-smoke'
            cases = sorted(smoke_root.glob('map-*'), key=lambda path: path.stat().st_mtime)
            if not cases:
                results.append(dict(step='visual', code=2, seconds=0.0,
                                    tail=['no smoke case available; run the smoke step first']))
                continue
            command, timeout = plan['visual']
            command[command.index('latest-smoke')] = str(cases[-1])
        else:
            command, timeout = plan[step]
        if args.skip_build and step == 'build':
            continue
        print(f'run_all step={step} started', flush=True)
        try:
            outcome = run(step, [str(part) for part in command], timeout, env)
        except subprocess.TimeoutExpired:
            outcome = dict(step=step, code=124, seconds=float(timeout),
                           tail=[f'timed out after {timeout}s'])
        if step == 'smoke' and outcome['code'] == 0:
            smoke_case = True
        results.append(outcome)
        status = 'passed' if outcome['code'] == 0 else f"failed({outcome['code']})"
        print(f"run_all step={step} {status} seconds={outcome['seconds']}", flush=True)
        if outcome['code'] != 0:
            for line in outcome['tail']:
                print(f'  {line}', flush=True)

    failed = [row['step'] for row in results if row['code'] != 0]
    total = sum(row['seconds'] for row in results)
    if failed:
        print(f'run_all=failed steps={failed} seconds={round(total, 1)}')
        return 1
    print(f'run_all=passed steps={len(results)} seconds={round(total, 1)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
