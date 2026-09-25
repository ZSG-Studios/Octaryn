"""Compile/run production loader CPU checks in a capped, owned Windows job."""
import argparse
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/validation'))
from capture_watchdog import ProcessTree, resume_owned_process
from cook_map_textures_guarded import JobLimits, MemoryStatus


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--asset', type=Path, required=True)
    parser.add_argument('--log', type=Path, required=True)
    args = parser.parse_args()
    if args.log.exists() or args.log.with_suffix('.json').exists():
        raise ValueError('Validation log already exists')
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.GlobalMemoryStatusEx.argtypes = [ctypes.POINTER(MemoryStatus)]
    def free_memory():
        status = MemoryStatus(); status.length = ctypes.sizeof(status)
        if not api.GlobalMemoryStatusEx(ctypes.byref(status)):
            raise ctypes.WinError(ctypes.get_last_error())
        return status.avail_phys
    report = {'status': 'not_started', 'private_limit_bytes': 2 * 1024**3,
              'minimum_free_bytes': free_memory()}
    if report['minimum_free_bytes'] < 4 * 1024**3:
        raise MemoryError('Less than4GiB available')
    command = [sys.executable, str(ROOT / 'tools/validation/validate_map_loader.py'), '--asset', str(args.asset.resolve())]
    started = time.monotonic()
    with args.log.open('x') as log:
        process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS | 4)
        tree = ProcessTree(process)
        try:
            limits = JobLimits(); limits.basic.flags = 0x2100
            limits.process_memory = report['private_limit_bytes']
            if not tree.api.SetInformationJobObject(tree.job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            resume_owned_process(process)
            while process.poll() is None:
                free = free_memory()
                report['minimum_free_bytes'] = min(free, report['minimum_free_bytes'])
                if free < 4 * 1024**3 or time.monotonic() - started > 300:
                    raise RuntimeError('Loader validation memory/time guard exceeded')
                time.sleep(.1)
            report['exit_code'] = process.wait()
            report['status'] = 'passed' if process.returncode == 0 else 'failed'
        finally:
            tree.close()
    report['elapsed_seconds'] = time.monotonic() - started
    args.log.with_suffix('.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
