"""Bound a capture's lifetime and kill only its owned process tree on failure."""
import ctypes
from ctypes import wintypes
import csv
import math
import os
import re
import shutil
import signal
import subprocess
import time


class ProcessTree:
    def __init__(self, process):
        self.process = process
        self.job = None
        if os.name != 'nt':
            return
        class Basic(ctypes.Structure):
            _fields_ = [('process_time', ctypes.c_int64), ('job_time', ctypes.c_int64),
                        ('flags', wintypes.DWORD), ('minimum', ctypes.c_size_t),
                        ('maximum', ctypes.c_size_t), ('active', wintypes.DWORD),
                        ('affinity', ctypes.c_size_t), ('priority', wintypes.DWORD),
                        ('scheduling', wintypes.DWORD)]
        class Limits(ctypes.Structure):
            _fields_ = [('basic', Basic), ('io', ctypes.c_uint64 * 6),
                        ('process_memory', ctypes.c_size_t), ('job_memory', ctypes.c_size_t),
                        ('peak_process', ctypes.c_size_t), ('peak_job', ctypes.c_size_t)]
        api = ctypes.WinDLL('kernel32', use_last_error=True)
        api.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        api.CreateJobObjectW.restype = wintypes.HANDLE
        api.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
        api.SetInformationJobObject.restype = wintypes.BOOL
        api.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        api.AssignProcessToJobObject.restype = wintypes.BOOL
        api.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
        api.TerminateJobObject.restype = wintypes.BOOL
        api.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int,
                                                  ctypes.c_void_p, wintypes.DWORD,
                                                  ctypes.POINTER(wintypes.DWORD)]
        api.QueryInformationJobObject.restype = wintypes.BOOL
        api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        api.OpenProcess.restype = wintypes.HANDLE
        api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        api.WaitForSingleObject.restype = wintypes.DWORD
        api.CloseHandle.argtypes = [wintypes.HANDLE]
        api.CloseHandle.restype = wintypes.BOOL
        self.api = api
        self.job = api.CreateJobObjectW(None, None)
        limits = Limits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if (not self.job or not api.SetInformationJobObject(self.job, 9, ctypes.byref(limits), ctypes.sizeof(limits))
                or not api.AssignProcessToJobObject(self.job, wintypes.HANDLE(int(process._handle)))):
            error = ctypes.get_last_error()
            process.kill()
            self.close()
            raise OSError(error, 'Cannot protect capture with an owned process job')

    def _owned_process_handles(self):
        """Open synchronize-only handles for the processes currently in this job.

        The list is obtained from the job object itself, never from a system-wide
        process scan.  The handles close in ``close`` after each owned process has
        signaled, so PID reuse cannot make teardown affect an unrelated process.
        """
        if not self.job:
            return []
        pointer_size = ctypes.sizeof(ctypes.c_void_p)
        header_size = ctypes.sizeof(wintypes.DWORD) * 2
        size = header_size + pointer_size
        while True:
            buffer = ctypes.create_string_buffer(size)
            returned = wintypes.DWORD()
            if self.api.QueryInformationJobObject(
                    self.job, 3, buffer, size, ctypes.byref(returned)):
                assigned = ctypes.c_uint32.from_buffer(buffer, 0).value
                listed = ctypes.c_uint32.from_buffer(buffer, 4).value
                if listed < assigned:
                    size = header_size + max(assigned, listed + 1) * pointer_size
                    continue
                handles = []
                for index in range(listed):
                    offset = header_size + index * pointer_size
                    pid = int.from_bytes(buffer.raw[offset:offset + pointer_size], 'little')
                    if not pid:
                        continue
                    handle = self.api.OpenProcess(0x100000, False, pid)  # SYNCHRONIZE
                    if handle:
                        handles.append(handle)
                return handles
            error = ctypes.get_last_error()
            if error != 234:  # ERROR_MORE_DATA
                raise ctypes.WinError(error)
            size = max(size * 2, int(returned.value) or size + pointer_size)

    def _wait_owned_processes(self, handles, deadline):
        """Wait until every captured owned process handle is signaled."""
        while handles:
            pending = []
            for handle in handles:
                result = self.api.WaitForSingleObject(handle, 0)
                if result == 258:  # WAIT_TIMEOUT
                    pending.append(handle)
                elif result != 0:
                    raise ctypes.WinError(ctypes.get_last_error())
            if not pending:
                return
            if time.monotonic() >= deadline:
                raise TimeoutError('Owned capture job did not finish teardown within five seconds')
            handles = pending
            time.sleep(.01)

    def close(self):
        if self.job:
            owned_handles = []
            process_list_error = None
            class Accounting(ctypes.Structure):
                _fields_ = [('times', ctypes.c_int64 * 4), ('faults', wintypes.DWORD),
                            ('total', wintypes.DWORD), ('active', wintypes.DWORD),
                            ('terminated', wintypes.DWORD)]
            try:
                try:
                    owned_handles = self._owned_process_handles()
                except OSError as error:
                    # Termination must still happen if the diagnostic PID-list
                    # query is unavailable on an older Windows host.  The
                    # accounting fallback below remains bounded and the error
                    # is reported after the owned job is torn down.
                    process_list_error = error
                # Closing the job starts asynchronous termination. Retain its
                # handle until descendants have released inherited log handles.
                if not self.api.TerminateJobObject(self.job, 1):
                    raise ctypes.WinError(ctypes.get_last_error())
                deadline = time.monotonic() + 5
                self._wait_owned_processes(owned_handles, deadline)
                while True:
                    accounting = Accounting()
                    if not self.api.QueryInformationJobObject(
                            self.job, 1, ctypes.byref(accounting), ctypes.sizeof(accounting), None):
                        raise ctypes.WinError(ctypes.get_last_error())
                    if accounting.active == 0:
                        break
                    if time.monotonic() >= deadline:
                        raise TimeoutError('Owned capture job did not finish teardown within five seconds')
                    time.sleep(.01)
                if process_list_error is not None:
                    raise process_list_error
            finally:
                for handle in owned_handles:
                    self.api.CloseHandle(handle)
                self.api.CloseHandle(self.job)
                self.job = None
        elif os.name != 'nt':
            try:
                os.killpg(self.process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        if self.process.poll() is None:
            self.process.kill()
        if os.name == 'nt':
            # GetExitCodeProcess can report an exit before the process object
            # signals; Popen.wait then returns its cached code without waiting.
            result = self.api.WaitForSingleObject(wintypes.HANDLE(int(self.process._handle)), 5000)
            if result == 258:
                raise TimeoutError('Owned capture process did not finish teardown within five seconds')
            if result != 0:
                raise ctypes.WinError(ctypes.get_last_error())
        self.process.wait(timeout=5)


def resume_owned_process(process):
    """Resume the suspended primary thread only after its process belongs to the job."""
    class ThreadEntry(ctypes.Structure):
        _fields_ = [('size', wintypes.DWORD), ('usage', wintypes.DWORD),
                    ('thread_id', wintypes.DWORD), ('owner_pid', wintypes.DWORD),
                    ('base_priority', wintypes.LONG), ('delta_priority', wintypes.LONG),
                    ('flags', wintypes.DWORD)]
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    api.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    for name in ('Thread32First', 'Thread32Next'):
        method = getattr(api, name)
        method.argtypes = [wintypes.HANDLE, ctypes.POINTER(ThreadEntry)]
        method.restype = wintypes.BOOL
    api.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    api.OpenThread.restype = wintypes.HANDLE
    api.ResumeThread.argtypes = [wintypes.HANDLE]
    api.ResumeThread.restype = wintypes.DWORD
    api.CloseHandle.argtypes = [wintypes.HANDLE]
    api.CloseHandle.restype = wintypes.BOOL
    snapshot = api.CreateToolhelp32Snapshot(4, 0)  # TH32CS_SNAPTHREAD
    if snapshot == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = ThreadEntry()
        entry.size = ctypes.sizeof(entry)
        found = api.Thread32First(snapshot, ctypes.byref(entry))
        while found:
            if entry.owner_pid == process.pid:
                thread = api.OpenThread(2, False, entry.thread_id)  # THREAD_SUSPEND_RESUME
                if not thread:
                    raise ctypes.WinError(ctypes.get_last_error())
                try:
                    if api.ResumeThread(thread) == 0xffffffff:
                        raise ctypes.WinError(ctypes.get_last_error())
                finally:
                    api.CloseHandle(thread)
                return
            entry.size = ctypes.sizeof(entry)
            found = api.Thread32Next(snapshot, ctypes.byref(entry))
        raise RuntimeError('Capture primary thread not found; process remains stopped')
    finally:
        api.CloseHandle(snapshot)


class FrameHealth:
    def __init__(self, now, stall_seconds=2.0, max_frame_ms=50.0, slow_seconds=1.0):
        self.last_frame = now
        self.started = False
        self.slow_ms = 0.0
        self.slow_frames = 0
        self.stall_seconds = stall_seconds
        self.max_frame_ms = max_frame_ms
        self.slow_seconds = slow_seconds

    def frame(self, milliseconds, now):
        if not math.isfinite(milliseconds) or milliseconds < 0:
            raise RuntimeError('Capture stopped: invalid frame timing')
        if milliseconds >= self.stall_seconds * 1000:
            raise RuntimeError(f'Capture stopped: frame heartbeat stalled (completed frame {milliseconds:.1f} ms)')
        self.started = True
        self.last_frame = now
        self.slow_ms = self.slow_ms + milliseconds if milliseconds > self.max_frame_ms else 0.0
        self.slow_frames = self.slow_frames + 1 if milliseconds > self.max_frame_ms else 0
        # A completed isolated setup hitch is not sustained slow rendering.
        # Missing subsequent frames still trips the independent stall deadline.
        if self.slow_frames >= 2 and self.slow_ms >= self.slow_seconds * 1000:
            raise RuntimeError(f'Capture stopped: sustained slow frames '
                               f'(count={self.slow_frames}, total_ms={self.slow_ms:.1f}, '
                               f'cutoff_ms={self.max_frame_ms:.2f})')

    def check(self, now):
        if self.started and now - self.last_frame > self.stall_seconds:
            raise RuntimeError('Capture stopped: frame heartbeat stalled')


class MapStartupHealth:
    def __init__(self):
        self.started = None

    def line(self, line, now):
        if line.startswith('map_ray_startup_begin') and self.started is None:
            self.started = now
        elif line.startswith('map_ray_startup_end'):
            self.check(now)
            self.started = None

    def check(self, now):
        if self.started is not None and now - self.started > 2:
            raise RuntimeError('Capture stopped: map ray startup exceeded two seconds')


class ShutdownFrames:
    def __init__(self, health):
        self.health = health
        self.remaining = None

    def line(self, line, now):
        if not line.startswith('client_shutdown_frame'):
            return
        match = re.fullmatch(r'client_shutdown_frame total_ms=(\S+) remaining=(\d+)\s*', line)
        if match is None:
            raise RuntimeError('Capture stopped: invalid shutdown frame record')
        try:
            milliseconds = float(match[1])
        except ValueError as error:
            raise RuntimeError('Capture stopped: invalid shutdown frame timing') from error
        remaining = int(match[2])
        if self.remaining is not None and remaining > self.remaining:
            raise RuntimeError('Capture stopped: shutdown resource count increased')
        self.health.frame(milliseconds, now)
        self.remaining = remaining


def confirm_process_priority(process, requested, log):
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.GetPriorityClass.argtypes = [wintypes.HANDLE]
    api.GetPriorityClass.restype = wintypes.DWORD
    actual = api.GetPriorityClass(wintypes.HANDLE(int(process._handle)))
    if not actual:
        raise ctypes.WinError(ctypes.get_last_error())
    expected = {'normal': 0x20, 'below-normal': 0x4000}[requested]
    confirmed = int(actual == expected)
    log.write((f'capture_process_priority requested={requested} actual=0x{actual:08x} '
               f'confirmed={confirmed}\n').encode())
    log.flush()
    if not confirmed:
        raise RuntimeError('Capture process priority differs from request; process remains stopped')


def run_capture(command, case, env, log, timeout, *, max_frame_ms=50.0,
                process_priority='below-normal', stall_seconds=2.0):
    # Native stdout and managed/RHI stderr use independent runtime locks. They
    # must not share a file handle while the owned process tree is running.
    closed = [False]
    with (case / 'client.stderr.log').open('w+b') as stderr:
        try:
            return _run_capture(command, case, env, log, stderr, closed, timeout,
                                max_frame_ms=max_frame_ms, process_priority=process_priority,
                                stall_seconds=stall_seconds)
        finally:
            if closed[0]:
                stderr.flush()
                stderr.seek(0)
                log.seek(0, os.SEEK_END)
                log.write(b'\ncapture_stderr_append ordering=nonchronological source=client.stderr.log\n')
                shutil.copyfileobj(stderr, log)
                log.write(b'\ncapture_stderr_append_end ordering=nonchronological\n')
                log.flush()
            # If teardown itself failed, retain the separate file untouched;
            # do not read/append a stream that may still have live writers.


def _run_capture(command, case, env, log, stderr, closed, timeout, *, max_frame_ms,
                 process_priority, stall_seconds):
    if not math.isfinite(max_frame_ms) or max_frame_ms <= 0:
        raise ValueError('max_frame_ms must be finite and positive')
    if not math.isfinite(stall_seconds) or stall_seconds <= 0:
        raise ValueError('stall_seconds must be finite and positive')
    priority_flags = {'normal': 0x20, 'below-normal': 0x4000}
    if process_priority not in priority_flags:
        raise ValueError('process_priority must be normal or below-normal')
    flags = (subprocess.CREATE_NO_WINDOW | priority_flags[process_priority] | 4
             if os.name == 'nt' else 0)  # CREATE_SUSPENDED
    process = subprocess.Popen(command, cwd=case, env=env, stdout=log, stderr=stderr,
                               creationflags=flags, start_new_session=os.name != 'nt')
    tree = ProcessTree(process)
    start = time.monotonic()
    health = FrameHealth(start, stall_seconds=stall_seconds, max_frame_ms=max_frame_ms)
    startup = MapStartupHealth()
    shutdown = ShutdownFrames(health)
    log_offset = 0
    log_pending = ''
    offset = 0
    pending = ''
    path = case / 'frame-timing.csv'
    failure = None
    try:
        if os.name == 'nt':
            confirm_process_priority(process, process_priority, log)
            resume_owned_process(process)
        while True:
            now = time.monotonic()
            return_code = process.poll()
            if now - start > timeout:
                raise RuntimeError('Capture stopped: overall timeout')
            if path.exists():
                with path.open(encoding='utf-8') as stream:
                    stream.seek(offset)
                    pending += stream.read()
                    offset = stream.tell()
                while '\n' in pending:
                    line, pending = pending.split('\n', 1)
                    if line.startswith('frame,'):
                        health.started = True
                        health.last_frame = now
                    elif line.strip():
                        row = next(csv.reader([line]))
                        health.frame(float(row[1]), now)
            with open(log.name, encoding='utf-8', errors='replace') as startup_log:
                startup_log.seek(log_offset)
                log_pending += startup_log.read()
                log_offset = startup_log.tell()
            while '\n' in log_pending:
                line, log_pending = log_pending.split('\n', 1)
                startup.line(line, now)
                shutdown.line(line, now)
            startup.check(now)
            health.check(now)
            if return_code is not None:
                return return_code
            time.sleep(.1)
    except BaseException as error:
        failure = error
        raise
    finally:
        try:
            tree.close()
            closed[0] = True
        except Exception as error:
            if failure is not None:
                raise RuntimeError(f'{failure}; owned process teardown also failed: {error}') from failure
            raise
