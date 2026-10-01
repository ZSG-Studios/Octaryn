"""Capture diagnostic-only AMD hardware traces without UI automation.

The profiler has been observed temporarily selecting peak GPU clocks and restoring
them afterward. Instrumented capture timings do not qualify engine performance.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', required=True, type=Path)
    parser.add_argument('--client-bundle', required=True, type=Path)
    parser.add_argument('--evidence-root', type=Path, default=Path('logs/client/rgp'))
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix='hardware-', dir=args.evidence_root.resolve()))
    command = [str(args.tool.resolve()), '--mode', 'profiling', '--process', 'Octaryn.Client',
               '--rgp-instruction-tracing', '--rgp-counter-collection', '--verbose',
               '--output', str(case / 'frame.rgp')]
    capture = [sys.executable, str(Path(__file__).with_name('capture_map_world.py')),
               '--client-bundle-root', str(args.client_bundle.resolve()),
               '--evidence-root', str(case / 'runtime'), '--backend', args.backend,
               '--frames', '900', '--captures', '0', '--width', '2560', '--height', '1440',
               '--reflection-quality', 'ultra', '--shadow-quality', 'ultra',
               '--max-frame-ms', '250', '--timeout', '180']
    (case / 'commands.json').write_text(json.dumps([command, capture], indent=2))
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    result = dict(status='running', timing_qualification=False, trigger_ready_frames=240)
    print(f'rgp_capture_started evidence={case}', flush=True)
    with (case / 'profiler.log').open('wb') as log, (case / 'harness.log').open('wb') as runtime:
        profiler = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=log,
                                    stderr=subprocess.STDOUT, creationflags=flags)
        try:
            # The profiling connection must exist before the graphics device.
            time.sleep(2)
            if profiler.poll() is not None:
                raise RuntimeError('Profiler exited before target launch')
            process = subprocess.Popen(capture, stdout=runtime, stderr=subprocess.STDOUT,
                                       creationflags=flags)
            deadline = time.monotonic() + 195
            triggered = False
            while process.poll() is None:
                if time.monotonic() > deadline:
                    process.terminate()
                    process.wait(timeout=5)
                    raise TimeoutError('Bounded capture harness did not exit')
                files = list((case / 'runtime').glob('*/frame-timing.csv'))
                if not triggered and files:
                    count = len(files[0].read_text(errors='replace').splitlines()) - 1
                    if count >= 240:
                        profiler.stdin.write(b'c\n')
                        profiler.stdin.flush()
                        triggered = True
                time.sleep(.2)
            result['runtime_exit'] = process.returncode
            result['triggered'] = triggered
            if process.returncode:
                raise RuntimeError('Runtime capture failed; preserve hardware evidence for diagnosis')
            output = case / 'frame.rgp'
            if not triggered or not output.exists() or output.stat().st_size < 1024:
                raise RuntimeError('No completed AMD hardware profile was produced')
            result.update(status='captured', bytes=output.stat().st_size)
        except Exception as error:
            result.update(status='failed', error=str(error))
            raise
        finally:
            if profiler.poll() is None:
                try:
                    profiler.stdin.write(b'q\n')
                    profiler.stdin.flush()
                    profiler.wait(timeout=5)
                except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
                    profiler.terminate()
                    profiler.wait(timeout=5)
            for path in (case / 'runtime').glob('*/result.json'):
                evidence = json.loads(path.read_text())
                evidence['timing_qualification'] = False
                evidence['hardware_capture'] = str(case)
                path.write_text(json.dumps(evidence, indent=2))
            (case / 'result.json').write_text(json.dumps(result, indent=2))
    print(f'rgp_capture_completed evidence={case}', flush=True)


if __name__ == '__main__':
    main()
