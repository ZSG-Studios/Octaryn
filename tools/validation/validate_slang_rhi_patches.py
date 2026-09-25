"""Reproduce registered SDK repairs from pinned blobs without modifying the active SDK."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BOOTSTRAP = ROOT / 'tools/build/slang-rhi.py'
SOURCE = ROOT / 'build/dependencies/slang-rhi'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def normalize(data):
    return data.replace(b'\r\n', b'\n')


def run(directory, *arguments, check=True):
    result = subprocess.run(['git', '-C', str(directory), *arguments],
                            capture_output=True, timeout=30)
    if check and result.returncode:
        raise RuntimeError(f'git {arguments[0]} failed: {result.stderr.decode(errors="replace")}')
    return result


def load_bootstrap():
    spec = importlib.util.spec_from_file_location('slang_rhi_patch_validation', BOOTSTRAP)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def apply_registered(scratch):
    program = (
        'import importlib.util, pathlib, sys; '
        's=importlib.util.spec_from_file_location("bootstrap",sys.argv[1]); '
        'm=importlib.util.module_from_spec(s); s.loader.exec_module(m); '
        'm.patch_checkout(pathlib.Path(sys.argv[2]))'
    )
    result = subprocess.run([sys.executable, '-c', program, str(BOOTSTRAP), str(scratch)],
                            capture_output=True, text=True, timeout=60)
    evidence = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
    return evidence


def inspect(report):
    bootstrap_bytes = BOOTSTRAP.read_bytes()
    module = load_bootstrap()
    terminal_context = module.run(sys.executable, '-c',
                                 'import sys; sys.stdout.write("context\\n \\r\\n")')
    report['run_preserves_terminal_context_space'] = terminal_context == 'context\n '
    if not report['run_preserves_terminal_context_space']:
        raise RuntimeError('Bootstrap output trimming removes a required patch context space')
    head = run(SOURCE, 'rev-parse', 'HEAD').stdout.decode().strip()
    if head != module.COMMIT:
        raise RuntimeError('Active SDK HEAD differs from registered pin')
    report.update(pinned_commit=head, bootstrap_sha256=digest(bootstrap_bytes),
                  registry_hash=module.patch_hash())
    patches = []
    owners = {}
    snapshots = {}
    for name in module.PATCHES:
        patch_path = ROOT / 'tools/build/patches' / name
        raw = patch_path.read_bytes()
        text = normalize(raw).decode()
        paths = re.findall(r'^diff --git a/(\S+) b/(\S+)$', text, re.M)
        if not paths:
            raise RuntimeError(f'Empty patch {name}')
        paths_checked = []
        for before, after in paths:
            relative = PurePosixPath(after)
            if before != after or relative.is_absolute() or '..' in relative.parts or ':' in after:
                raise RuntimeError(f'Unexpected or unsafe patch path: {before} -> {after}')
            if after in owners:
                raise RuntimeError(f'Overlapping patch owners for {after}: {owners[after]} and {name}')
            owners[after] = name
            active = SOURCE / after
            snapshots[after] = active.read_bytes() if active.exists() else None
            paths_checked.append(after)
        patches.append({'name': name, 'raw_sha256': digest(raw),
                        'normalized_sha256': digest(normalize(raw)), 'paths': paths_checked,
                        'ends_with_context_space': text.rstrip('\n').endswith('\n ')})
    report['patches'] = patches
    scratch = Path(tempfile.mkdtemp(prefix='slang-rhi-patch-check-', dir=ROOT / 'build/dependencies'))
    report['scratch_directory'] = str(scratch)
    run(scratch, 'init', '--quiet')
    run(scratch, 'config', 'core.autocrlf', 'false')
    run(scratch, 'config', 'core.safecrlf', 'false')
    baseline = []
    for path in owners:
        blob = run(SOURCE, 'show', f'{head}:{path}', check=False)
        if blob.returncode:
            exists = run(SOURCE, 'cat-file', '-e', f'{head}:{path}', check=False)
            if exists.returncode == 0:
                raise RuntimeError(f'Could not read pinned blob {path}')
            baseline.append({'path': path, 'new_file': True})
            continue
        destination = scratch / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(blob.stdout)
        baseline.append({'path': path, 'new_file': False, 'sha256': digest(blob.stdout)})
    report['baseline'] = baseline
    run(scratch, 'add', '--all')
    run(scratch, '-c', 'user.name=Octaryn Patch Validation', '-c', 'user.email=validation@localhost',
        'commit', '--quiet', '--no-gpg-sign', '-m', f'Isolated registered-path subset of {head}')
    report['clean_apply'] = apply_registered(scratch)
    if report['clean_apply']['exit_code']:
        raise RuntimeError('Production bootstrap rejected clean registered patch application')
    report['idempotent_apply'] = apply_registered(scratch)
    if report['idempotent_apply']['exit_code']:
        raise RuntimeError('Production bootstrap rejected already applied patches')
    comparisons = []
    for path, original in snapshots.items():
        produced_path = scratch / path
        produced = produced_path.read_bytes() if produced_path.exists() else None
        active_path = SOURCE / path
        active = active_path.read_bytes() if active_path.exists() else None
        same = produced is None and active is None
        if produced is not None and active is not None:
            same = normalize(produced) == normalize(active)
        comparisons.append({'path': path, 'matches_active_normalized': same,
                            'active_unchanged': active == original,
                            'normalized_sha256': digest(normalize(active)) if active is not None else None})
    report['files'] = comparisons
    for entry in patches:
        actual = normalize(run(scratch, 'diff', '--binary', '--no-ext-diff', 'HEAD', '--',
                               *entry['paths']).stdout).rstrip(b'\n')
        expected = normalize((ROOT / 'tools/build/patches' / entry['name']).read_bytes()).rstrip(b'\n')
        entry['exact_canonical_diff'] = actual == expected
        entry['unchanged'] = digest((ROOT / 'tools/build/patches' / entry['name']).read_bytes()) == entry['raw_sha256']
    report['bootstrap_unchanged'] = BOOTSTRAP.read_bytes() == bootstrap_bytes
    report['registry_unchanged'] = module.patch_hash() == report['registry_hash']
    if not report['bootstrap_unchanged'] or not report['registry_unchanged']:
        raise RuntimeError('Bootstrap/registry changed during validation; rerun after source freeze')
    if any(not entry['matches_active_normalized'] or not entry['active_unchanged'] for entry in comparisons):
        raise RuntimeError('Replayed source differs from active SDK or active source changed during check')
    if any(not entry['exact_canonical_diff'] or not entry['unchanged'] for entry in patches):
        raise RuntimeError('Patch canonical diff mismatch or patch changed during check')
    report['status'] = 'passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, default=ROOT / 'logs/tools/slang-rhi-patches.json')
    args = parser.parse_args()
    report = {'status': 'failed', 'captured_utc': datetime.now(timezone.utc).isoformat(),
              'scope': 'Isolated pinned-source patch replay; active SDK read only; no build or GPU runtime'}
    try:
        inspect(report)
    except Exception as error:
        report['error'] = str(error)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'slang_rhi_patches={report["status"]} report={args.report}')
    if report['status'] != 'passed':
        print(report.get('error', 'Unknown validation failure'), file=sys.stderr)
        return 1
    print(f'patches={len(report["patches"])} files={len(report["files"])} registry={report["registry_hash"]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
