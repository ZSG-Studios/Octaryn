"""Run bounded, headless production resource-lifecycle GPU fixtures serially."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import tempfile

from capture_watchdog import run_capture


CASES = {
    'mesh-counters': ('world_mesh_counters=passed', 'world_mesh_draw_counters=passed',
                      'world_mesh_allocation=passed'),
    'ray-allocation': ('world_ray_allocation=passed', 'world_resource_budget=passed'),
    'ray-private-progress': ('world_ray_private_progress=passed',),
    'ray-tracing': ('world_ray_probe=passed', 'world_ray_reuse=passed', 'world_ray_idle_reuse=passed'),
    'retirement': ('world_retirement=passed',),
    'delivery-lifecycle': ('world_mesh_async_delivery=passed',),
    'delivery-dual': ('world_mesh_dual_delivery=passed',),
    'delivery-prefetch': ('world_mesh_delivery_prefetch=passed',),
    'delivery-preload-invalidation': ('world_mesh_preload_invalidation=passed',),
    'direct-lighting': ('direct_lighting=passed', 'local_light_upload=passed'),
}
DELIVERY_CASES = ('delivery-lifecycle', 'delivery-dual', 'delivery-prefetch', 'delivery-preload-invalidation')
ERRORS = re.compile(r'^.*(?:rhi_validation severity=error|Validation Error|VUID-|'
                    r'D3D12 ERROR|D3D12 CORRUPTION).*$', re.MULTILINE)


def timing_evidence(path):
    with path.open(newline='', encoding='utf-8') as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ['frame', 'total_ms', 'work_ms', 'cap_sleep_ms', 'phase']:
            raise RuntimeError('Missing resource-probe completed-work timing columns')
        rows = list(reader)
    if not rows:
        raise RuntimeError('Resource probe emitted no completed-work heartbeat')
    for index, row in enumerate(rows):
        values = [float(row[name]) for name in ('total_ms', 'work_ms', 'cap_sleep_ms')]
        total, work, sleep = values
        if (int(row['frame']) != index or not row['phase'] or
                any(not math.isfinite(value) or value < 0 for value in values) or
                total < 1000 / 30 - .02 or abs(total - work - sleep) > .02):
            raise RuntimeError(f'Invalid or uncapped resource-probe heartbeat at row {index}')
    return {'completed_frames': len(rows), 'cap_fps': 30,
            'max_total_ms': max(float(row['total_ms']) for row in rows),
            'max_work_wall_ms': max(float(row['work_ms']) for row in rows),
            'total_cap_sleep_ms': sum(float(row['cap_sleep_ms']) for row in rows),
            'phases': sorted({row['phase'] for row in rows})}


def selected_cases(requested):
    selected = []
    for name in requested or CASES:
        for concrete in DELIVERY_CASES if name == "delivery" else (name,):
            if concrete not in selected:
                selected.append(concrete)
    return selected


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', choices=('d3d12', 'vulkan'), default='d3d12')
    parser.add_argument('--probe-root', type=Path,
                        default=root / 'build/release-windows/tools/validation/world-mesh')
    parser.add_argument('--evidence-root', type=Path, default=root / 'logs/tools/resource-lifecycle')
    parser.add_argument('--case', choices=(*CASES, 'delivery'), action='append',
                        help='delivery runs all four delivery groups as separate bounded processes')
    args = parser.parse_args()
    executable = args.probe_root.resolve() / 'octaryn_client_world_mesh_probe.exe'
    if not executable.is_file():
        raise FileNotFoundError(executable)
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('OCTARYN_CLIENT_') and not key.startswith('VK_')}
    env.pop('SLANG_RHI_D3D12_RESOURCE_TIMING', None)
    env['OCTARYN_CLIENT_MESH_PROBE_BACKEND'] = args.backend
    env['OCTARYN_CLIENT_RESOURCE_PROBE_PACING'] = '1'
    env['OCTARYN_CLIENT_FENCE_TIMEOUT_MS'] = '2000'
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    for name in selected_cases(args.case):
        case = Path(tempfile.mkdtemp(prefix=f'{name}-{args.backend}-', dir=args.evidence_root.resolve()))
        log_path = case / 'probe.log'
        result = {'case': name, 'requested_cases': args.case, 'backend': args.backend,
                  'command': [str(executable), f'--{name}-only'], 'executable': str(executable),
                  'sha256': digest, 'timeout_seconds': 30, 'process_priority': 'normal',
                  'headless': True, 'cap_fps': 30, 'heartbeat_timeout_seconds': 2,
                  'slow_frame_ms': 50, 'sustained_slow_seconds': 1, 'status': 'running'}
        print(f'resource_gpu_start case={name} evidence={case}', flush=True)
        try:
            with log_path.open('wb') as log:
                result['exit_code'] = run_capture(result['command'],
                                                 case, env, log, 30, process_priority='normal')
            text = log_path.read_text(encoding='utf-8', errors='replace')
            failures = ERRORS.findall(text)
            result['validation_errors'] = failures
            result['markers'] = {marker: marker in text for marker in CASES[name]}
            result['markers']['resource_probe_pacing=started'] = 'resource_probe_pacing=started' in text
            result['timing'] = timing_evidence(case / 'frame-timing.csv')
            if result['exit_code'] or failures or not all(result['markers'].values()):
                raise RuntimeError(f'Resource GPU fixture failed: {name}; see {log_path}')
            result['status'] = 'passed'
        except Exception as error:
            result.update(status='failed', error=str(error))
            raise
        finally:
            (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(f'resource_gpu_pass case={name} log={log_path}', flush=True)


if __name__ == '__main__':
    main()
