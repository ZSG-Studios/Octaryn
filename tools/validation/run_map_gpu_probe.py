"""Run the tiny headless material/raster fixture under an owned process watchdog."""
import argparse
import os
from pathlib import Path
import re

from capture_watchdog import run_capture


def inspect_log(text):
    failures = re.findall(r'^.*(?:rhi_validation severity=error|Validation Error|VUID-|'
                          r'D3D12 ERROR|D3D12 CORRUPTION).*$', text, re.MULTILINE)
    if failures:
        raise RuntimeError('\n'.join(failures[:8]))
    for marker in ('map_material_gpu=passed', 'map_raster_gpu=passed',
                   'map_pbr_environment_gpu=passed',
                   'map_ray_enable_submitted', 'map_ray_enable_complete',
                   'map_uploaded_mips_gpu=passed cached=0',
                   'map_uploaded_mips_gpu=passed cached=1'):
        if marker not in text:
            raise RuntimeError(f'Missing GPU fixture result: {marker}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', choices=('d3d12', 'vulkan'), required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    fixture = root / 'build/release-windows/tools/validation/world-mesh'
    log_path = root / f'logs/tools/map-opaque-{args.backend}.log'
    env = dict(os.environ, OCTARYN_CLIENT_MESH_PROBE_BACKEND=args.backend)
    with log_path.open('wb') as log:
        code = run_capture([str(fixture / 'octaryn_client_world_mesh_probe.exe'), '--map-materials-only'],
                           fixture, env, log, timeout=10)
    if code:
        raise RuntimeError(f'GPU fixture failed with exit {code}; see {log_path}')
    inspect_log(log_path.read_text(encoding='utf-8', errors='replace'))
    print(f'map_gpu_probe=passed backend={args.backend} log={log_path}')


if __name__ == '__main__':
    main()
