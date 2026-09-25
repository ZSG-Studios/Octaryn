"""Run all block-transport GPU groups with an unchanged watchdog per process."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

from capture_watchdog import run_capture
from block_transport_gpu_checks import ORACLE_CASES, inspect_log, inspect_group_proofs
from block_transport_gpu_groups import GROUPS, inspect_group, inspect_pacing
from block_transport_gpu_provenance import shader_snapshot, verify_shader_snapshot


def bounded_timeout(value):
    seconds = int(value)
    if not 1 <= seconds <= 30:
        raise argparse.ArgumentTypeError('timeout must be between 1 and 30 seconds')
    return seconds


def select_groups(requested=None):
    if requested is None:
        return list(GROUPS)
    if not requested or len(set(requested)) != len(requested) or any(name not in GROUPS for name in requested):
        raise ValueError('Selected groups must be nonempty, known and unique')
    return [name for name in GROUPS if name in requested]


def run_groups(root, fixture, backend, timeout, evidence, requested=None):
    selected = select_groups(requested)
    full_suite = selected == list(GROUPS)
    executable = fixture / 'octaryn_client_world_mesh_probe.exe'
    if not executable.is_file():
        raise FileNotFoundError(f'Missing staged GPU fixture: {executable}')
    fingerprint = hashlib.sha256(executable.read_bytes()).hexdigest()
    shaders = shader_snapshot(root, fixture)
    (evidence / 'shaders.json').write_text(json.dumps(shaders, indent=2) + '\n')
    result = dict(status='running', backend=backend, executable_sha256=fingerprint,
                  shaders_sha256=shaders['sha256'], shader_manifest='shaders.json',
                  timeout_per_process=timeout, scope='full_suite' if full_suite else 'selected_groups',
                  full_suite=full_suite, expected_groups=selected, suite_groups=list(GROUPS), groups=[])
    env = dict(os.environ, OCTARYN_CLIENT_MESH_PROBE_BACKEND=backend)
    heartbeat = fixture / 'frame-timing.csv'
    convergence = fixture / 'block-transport-convergence.csv'
    local_area = fixture / 'block-transport-local-area.csv'
    aggregate_path = evidence / 'combined.log'
    logs = []
    failures = []
    try:
        for name in selected:
            directory = evidence / name
            directory.mkdir()
            log_path = directory / 'stdout.log'
            record = dict(name=name, status='running')
            result['groups'].append(record)
            # These are exact owned fixture outputs, copied before the next process.
            heartbeat.unlink(missing_ok=True)
            convergence.unlink(missing_ok=True)
            local_area.unlink(missing_ok=True)
            try:
                if hashlib.sha256(executable.read_bytes()).hexdigest() != fingerprint:
                    raise RuntimeError('Staged executable changed during grouped qualification')
                verify_shader_snapshot(root, fixture, shaders)
                record['shaders_sha256'] = shaders['sha256']
                with log_path.open('wb') as log:
                    code = run_capture([str(executable), '--block-transport-only', '--group', name],
                                       fixture, env, log, timeout=timeout)
                record['exit_code'] = code
                verify_shader_snapshot(root, fixture, shaders)
                if code:
                    raise RuntimeError(f'GPU fixture exited with {code}')
                text = log_path.read_text(encoding='utf-8', errors='replace')
                inspect_group_proofs(text, name)
                frames = inspect_pacing(heartbeat, text, name)
                record.update(status='passed', frames=frames)
            except Exception as error:
                record.update(status='failed', error=str(error))
                failures.append(f'{name}: {error}')
            finally:
                for source in (heartbeat, convergence, local_area):
                    if source.is_file():
                        shutil.copy2(source, directory / source.name)
                if log_path.is_file():
                    logs.append(log_path.read_text(encoding='utf-8', errors='replace'))
                (directory / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
                aggregate_path.write_text('\n'.join(logs), encoding='utf-8')
                (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
                print(f'block_transport_gpu_group={record["status"]} name={name} evidence={directory}', flush=True)
        if hashlib.sha256(executable.read_bytes()).hexdigest() != fingerprint:
            failures.append('Staged executable changed before aggregate validation')
        try:
            verify_shader_snapshot(root, fixture, shaders)
        except Exception as error:
            failures.append(str(error))
        if failures:
            raise RuntimeError('GPU groups failed: ' + '; '.join(failures))
        if full_suite:
            inspect_log('\n'.join(logs))
        result.update(status='passed', oracle_frames=len(ORACLE_CASES) * 4 if 'numerical' in selected else 0,
                      paced_frames=sum(record['frames'] for record in result['groups']))
    except Exception as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        suffix = '' if full_suite else '-selected'
        latest = root / f'logs/tools/block-transport-{backend}{suffix}.log'
        latest.parent.mkdir(parents=True, exist_ok=True)
        if aggregate_path.is_file():
            shutil.copy2(aggregate_path, latest)
        print(f'block_transport_gpu_evidence={evidence}', flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', choices=('d3d12', 'vulkan'), required=True)
    parser.add_argument('--timeout', type=bounded_timeout, default=30,
                        help='Maximum seconds for each independently owned process (1..30)')
    parser.add_argument('--evidence-root', type=Path)
    parser.add_argument('--group', choices=tuple(GROUPS), action='append',
                        help='Run only this group; repeat for several groups (default: full suite)')
    args = parser.parse_args()
    try:
        selected = select_groups(args.group)
    except ValueError as error:
        parser.error(str(error))
    root = Path(__file__).resolve().parents[2]
    fixture = root / 'build/release-windows/tools/validation/world-mesh'
    evidence_root = args.evidence_root or root / 'logs/tools/block-transport-gpu'
    evidence_root.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=f'{args.backend}-', dir=evidence_root.resolve()))
    result = run_groups(root, fixture, args.backend, args.timeout, evidence, selected)
    print(f'block_transport_gpu=passed backend={args.backend} groups={len(selected)} '
          f'scope={result["scope"]} full_suite={str(result["full_suite"]).lower()} '
          f'oracle_frames={result["oracle_frames"]} paced_frames={result["paced_frames"]} '
          f'fps_cap=30 validation_errors=0 evidence={evidence}')


if __name__ == '__main__':
    main()
