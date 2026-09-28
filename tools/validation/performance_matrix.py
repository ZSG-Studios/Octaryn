"""Run three serialized matched repetitions per requested rendering workload."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, default=Path('build/release-windows/client/bundle'))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--sizes', nargs='+', choices=('1440p', '4k'), default=['1440p'])
    parser.add_argument('--modes', nargs='+', type=int, choices=(0, 1, 2), default=[0, 1, 2])
    parser.add_argument('--variants', nargs='+', choices=('reference', 'adaptive', 'history', 'sparse', 'indirect', 'lod', 'meshlet'),
                        default=['reference', 'adaptive'])
    parser.add_argument('--workloads', nargs='+', choices=('static', 'motion'), default=['motion'])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    runs = []
    for size in args.sizes:
        width, height = (2560, 1440) if size == '1440p' else (3840, 2160)
        for mode in args.modes:
            for workload in args.workloads:
                group = args.output / f'{args.backend}-{size}-mode{mode}-{workload}'
                for repeat in range(3):
                    for variant in args.variants:
                        command = [sys.executable, str(Path(__file__).with_name('capture_map_world.py')),
                                   '--client-bundle-root', str(args.bundle.resolve()), '--evidence-root', str(group),
                                   '--backend', args.backend, '--width', str(width), '--height', str(height),
                                   '--upscaler-mode', str(mode), '--frames', '480', '--capture-min-frame', '360',
                                   '--captures', '3' if repeat == 0 else '0', '--stride', '16',
                                   '--reflection-quality', 'ultra', '--shadow-quality', 'ultra',
                                   '--uncapped-fps', '--max-frame-ms', '250', '--timeout', '180']
                        if workload == 'motion': command.append('--camera-motion')
                        if variant == 'reference': command.append('--rt-reference')
                        if variant == 'sparse': command.append('--rt-sparse')
                        if variant == 'history': command.append('--rt-history-search')
                        if variant in ('indirect', 'lod'): command += ['--draw-mode', 'indirect']
                        if variant == 'meshlet': command += ['--draw-mode', 'meshlet']
                        if variant == 'lod': command += ['--lod-pixels', '.5']
                        print(f'matrix size={size} mode={mode} workload={workload} repeat={repeat} variant={variant}', flush=True)
                        subprocess.run(command, check=True)
                        case = max(group.iterdir(), key=lambda path: path.stat().st_mtime)
                        runs.append(dict(size=size, mode=mode, workload=workload, repeat=repeat,
                                         variant=variant, case=str(case)))
                        (args.output / 'matrix.json').write_text(json.dumps(runs, indent=2))


if __name__ == '__main__':
    main()
