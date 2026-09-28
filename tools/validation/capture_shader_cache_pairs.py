"""Three isolated empty/reused engine shader-cache pairs; never clears user caches."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from shader_cache_report import cache_snapshot, load_case, validate_pair, report, markdown


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12','vulkan'), default='dx12')
    parser.add_argument('--width', type=int, default=2560)
    parser.add_argument('--height', type=int, default=1440)
    parser.add_argument('--mode', type=int, choices=(0,1,2), default=0)
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-frame-ms', type=float, default=250)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if min(args.width,args.height,args.timeout) <= 0 or not 0 < args.max_frame_ms < float('inf'):
        parser.error('Dimensions, timeout and frame watchdog must be positive and finite')
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='shader-cache-pairs-', dir=args.evidence_root.resolve()))
    plan = dict(scope='Engine shader/pipeline caches only; OS/driver/filesystem states unknown.',
                timing_qualification=False, commands=[])
    for repeat in range(3):
        directory = root / f'pair-{repeat}' / 'engine-cache'
        directory.mkdir(parents=True, exist_ok=False)
        for state in ('engine_cache_empty','engine_cache_reused'):
            evidence = directory.parent / state
            command = [sys.executable, str(Path(__file__).with_name('capture_map_world.py')),
                '--client-bundle-root', str(args.client_bundle_root.resolve()), '--shader-cache-dir', str(directory),
                '--evidence-root', str(evidence), '--backend', args.backend, '--width', str(args.width),
                '--height', str(args.height), '--upscaler-mode', str(args.mode), '--frames', '240',
                '--warmup-frames', '120', '--captures', '0', '--rt-reference', '--draw-mode', 'direct',
                '--reflection-quality', 'ultra', '--shadow-quality', 'ultra', '--timeout', str(args.timeout),
                '--max-frame-ms', str(args.max_frame_ms)]
            if args.manifest: command += ['--manifest', str(args.manifest.resolve())]
            plan['commands'].append(dict(repeat=repeat, state=state, cache_directory=str(directory),
                                         evidence=str(evidence), command=command))
    (root / 'plan.json').write_text(json.dumps(plan, indent=2))
    print(f'shader_cache_pair_plan={root / "plan.json"}', flush=True)
    if args.dry_run: return
    completed, observed = [], []
    try:
        for entry in plan['commands']:
            directory = Path(entry['cache_directory'])
            before = cache_snapshot(directory)
            if entry['state'] == 'engine_cache_empty' and (before['files'] or any(directory.iterdir())):
                raise ValueError('New pair cache directory was modified before the first launch')
            if entry['state'] == 'engine_cache_reused' and completed[-1]['after'] != before:
                raise ValueError('Engine cache changed between first launch and reuse')
            (directory.parent / (entry['state']+'-before.json')).write_text(json.dumps(before, indent=2))
            subprocess.run(entry['command'], check=True, timeout=args.timeout+60)
            results = list(Path(entry['evidence']).glob('*/result.json'))
            if len(results) != 1: raise RuntimeError('Expected exactly one isolated startup capture')
            completed.append(dict(entry, before=before, after=cache_snapshot(directory), case=str(results[0].parent)))
            (root / 'cases.json').write_text(json.dumps(completed, indent=2))
            observed.append(load_case(completed[-1]))
            if observed[-1]['identity'] != observed[0]['identity'] or observed[-1]['namespace'] != observed[0]['namespace']:
                raise ValueError('Recorded build/asset/settings identity changed during cache comparison')
            if entry['state'] == 'engine_cache_reused': validate_pair(*observed[-2:])
        data = report(completed)
        (root / 'report.json').write_text(json.dumps(data, indent=2))
        (root / 'report.md').write_text(markdown(data))
    finally:
        (root / 'cases.json').write_text(json.dumps(completed, indent=2))
    print(f'shader_cache_pair_report={root / "report.json"}', flush=True)


if __name__ == '__main__':
    main()
