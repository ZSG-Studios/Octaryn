"""Bound the CPU-only worker regression; no GPU device or client is launched."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
command = [sys.argv[1]]
started = time.monotonic()
report = {'command': command, 'gpu_runtime': False, 'timeout_seconds': 30}
try:
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=30)
    report.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr,
                  status='passed' if result.returncode == 0 else 'failed')
except subprocess.TimeoutExpired as error:
    report.update(status='failed', error='CPU retirement probe exceeded 30 seconds',
                  stdout=str(error.stdout or ''), stderr=str(error.stderr or ''))
report['wall_seconds'] = time.monotonic() - started
sources = list(Path(__file__).parent.glob('*.cpp')) + [Path(__file__).parent / 'Probe.h']
sources += [root / 'build/dependencies/slang-rhi/src/core/resource-retirement.h',
            root / 'build/dependencies/slang-rhi/src/d3d12/d3d12-bindless-descriptor-set.h',
            root / 'octaryn-client/Source/Rendering/RenderBackend/WorldRetirementProgress.h']
report['source_sha256'] = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sources}
output = root / 'logs/tools/resource-retirement-cpu.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(report.get('stdout', ''), end='')
print(report.get('stderr', ''), end='', file=sys.stderr)
print(f'resource_retirement_report={output} status={report["status"]}')
sys.exit(0 if report['status'] == 'passed' else 1)
