"""Self-only sampled memory/time guard for a background offline CPU cook."""
import ctypes
import os
import threading
import time


class Memory(ctypes.Structure):
    _fields_ = [('length', ctypes.c_ulong), ('load', ctypes.c_ulong)] + [
        (name, ctypes.c_ulonglong) for name in ('total', 'free', 'page', 'free_page', 'virtual', 'free_virtual', 'extended')]


class ProcessMemory(ctypes.Structure):
    _fields_ = [('size', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
        (name, ctypes.c_size_t) for name in ('peak_working', 'working', 'peak_paged', 'paged',
                                           'peak_nonpaged', 'nonpaged', 'page', 'peak_page', 'private')]


def start_guard(max_private_gib=1.5, min_free_gib=4, seconds=1800):
    if os.name != 'nt':
        raise ValueError('This measured memory guard is currently Windows-only')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    psapi = ctypes.WinDLL('psapi', use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.SetPriorityClass.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
    process = kernel.GetCurrentProcess()
    if not kernel.SetPriorityClass(process, 0x4000):
        raise OSError('Cannot set BelowNormal cook priority')
    started = time.monotonic()
    state = {'peak_private_bytes': 0, 'peak_working_bytes': 0}

    def check():
        memory = Memory(); memory.length = ctypes.sizeof(memory)
        own = ProcessMemory(); own.size = ctypes.sizeof(own)
        if not kernel.GlobalMemoryStatusEx(ctypes.byref(memory)) or not psapi.GetProcessMemoryInfo(
                process, ctypes.byref(own), ctypes.sizeof(own)):
            raise OSError('Cannot read cook memory budget')
        state['peak_private_bytes'] = max(state['peak_private_bytes'], own.private)
        state['peak_working_bytes'] = max(state['peak_working_bytes'], own.working)
        if own.private > max_private_gib * 2**30 or memory.free < min_free_gib * 2**30:
            raise MemoryError('Offline cook memory budget exceeded')
        if time.monotonic() - started > seconds:
            raise TimeoutError('Offline cook time budget exceeded')

    check()
    stop = threading.Event()

    def watch():
        while not stop.wait(.5):
            try:
                check()
            except Exception as error:
                print(f'cook_guard_abort={error}', flush=True)
                os._exit(76)
    threading.Thread(target=watch, daemon=True).start()
    return state, stop
