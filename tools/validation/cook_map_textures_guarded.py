"""Run only the offline texture cooker with owned-job memory/time guards."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import time

from capture_watchdog import ProcessTree, resume_owned_process

GIB = 1024 ** 3
ROOT = Path(__file__).resolve().parents[2]


class MemoryStatus(ctypes.Structure):
    _fields_ = [('length', wintypes.DWORD), ('load', wintypes.DWORD)] + [
        (name, ctypes.c_uint64) for name in ('total_phys', 'avail_phys', 'total_page', 'avail_page',
                                            'total_virtual', 'avail_virtual', 'avail_extended')]


class ProcessMemory(ctypes.Structure):
    _fields_ = [('length', wintypes.DWORD), ('faults', wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in ('peak_rss', 'rss', 'peak_paged', 'paged',
                                           'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile', 'private')]


class JobBasic(ctypes.Structure):
    _fields_ = [('process_time', ctypes.c_int64), ('job_time', ctypes.c_int64),
                ('flags', wintypes.DWORD), ('minimum', ctypes.c_size_t), ('maximum', ctypes.c_size_t),
                ('active', wintypes.DWORD), ('affinity', ctypes.c_size_t),
                ('priority', wintypes.DWORD), ('scheduling', wintypes.DWORD)]


class JobLimits(ctypes.Structure):
    _fields_ = [('basic', JobBasic), ('io', ctypes.c_uint64 * 6),
                ('process_memory', ctypes.c_size_t), ('job_memory', ctypes.c_size_t),
                ('peak_process', ctypes.c_size_t), ('peak_job', ctypes.c_size_t)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, default=ROOT / 'build/release-windows/tools/map-texture-cache/map_texture_cook.exe')
    parser.add_argument('--map', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('--reuse-cache', type=Path)
    parser.add_argument('--max-textures', type=int)
    parser.add_argument('--timeout', type=float, default=300)
    args = parser.parse_args()
    if os.name != 'nt' or not 0 < args.timeout <= 1800:
        parser.error('Windows only; timeout must be positive and at most 1800 seconds')
    if args.max_textures is not None and args.max_textures <= 0:
        parser.error('max-textures must be positive')
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.GlobalMemoryStatusEx.argtypes = [ctypes.POINTER(MemoryStatus)]
    api.GlobalMemoryStatusEx.restype = wintypes.BOOL
    api.K32GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessMemory), wintypes.DWORD]
    api.K32GetProcessMemoryInfo.restype = wintypes.BOOL

    def available():
        status = MemoryStatus(); status.length = ctypes.sizeof(status)
        if not api.GlobalMemoryStatusEx(ctypes.byref(status)):
            raise ctypes.WinError(ctypes.get_last_error())
        return status.avail_phys

    command = [str(args.tool.resolve()), str(args.map.resolve()), '--output-dir', str(args.output_dir.resolve())]
    if args.reuse_cache:
        command += ['--reuse-cache', str(args.reuse_cache.resolve())]
    if args.max_textures is not None:
        command += ['--max-textures', str(args.max_textures)]
    args.log.parent.mkdir(parents=True, exist_ok=True)
    report = dict(command=command, status='not_started', peak_rss_bytes=0,
                  peak_private_bytes=0, minimum_available_bytes=available(),
                  process_private_limit_bytes=GIB, minimum_free_bytes=4 * GIB, timeout_seconds=args.timeout)
    start = time.monotonic(); process = None; tree = None
    try:
        if report['minimum_available_bytes'] < 4 * GIB:
            raise RuntimeError('less than 4GiB available before launch')
        with args.log.open('w', encoding='utf-8') as log:
            process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS | 4)
            tree = ProcessTree(process)
            limits = JobLimits(); limits.basic.flags = 0x2100  # KILL_ON_JOB_CLOSE | PROCESS_MEMORY
            limits.process_memory = GIB
            if not tree.api.SetInformationJobObject(tree.job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            resume_owned_process(process)
            while process.poll() is None:
                memory = ProcessMemory(); memory.length = ctypes.sizeof(memory)
                if not api.K32GetProcessMemoryInfo(wintypes.HANDLE(int(process._handle)), ctypes.byref(memory), ctypes.sizeof(memory)):
                    if process.poll() is not None:
                        break
                    raise ctypes.WinError(ctypes.get_last_error())
                free = available()
                report['minimum_available_bytes'] = min(report['minimum_available_bytes'], free)
                report['peak_rss_bytes'] = max(report['peak_rss_bytes'], memory.peak_rss)
                report['peak_private_bytes'] = max(report['peak_private_bytes'], memory.private, memory.peak_pagefile)
                if free < 4 * GIB or memory.private > GIB:
                    raise RuntimeError('memory guard exceeded')
                if time.monotonic() - start > args.timeout:
                    raise RuntimeError('cooker overall timeout')
                time.sleep(.1)
            report['exit_code'] = process.wait(timeout=5)
            report['status'] = 'completed' if process.returncode == 0 else 'failed'
    except Exception as error:
        report['status'] = 'stopped'; report['error'] = str(error)
    finally:
        if tree:
            tree.close()
        elif process and process.poll() is None:
            process.kill(); process.wait(timeout=5)
        report['duration_seconds'] = round(time.monotonic() - start, 3)
        report['dds_bytes'] = sum(path.stat().st_size for path in args.output_dir.glob('*.dds'))
        report['dds_files'] = len(list(args.output_dir.glob('*.dds')))
        args.log.with_suffix('.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(json.dumps(report, indent=2))
    return 0 if report['status'] == 'completed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
