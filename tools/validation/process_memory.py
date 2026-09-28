"""Read Windows process working-set/private memory without a monitoring service."""
import ctypes as C
import os


def process_memory(pid=None):
    if os.name != "nt":
        return {}
    class Memory(C.Structure):
        _fields_ = [("cb", C.c_uint32), ("faults", C.c_uint32)] + [(name, C.c_size_t) for name in (
            "peak_rss", "rss", "peak_paged", "paged", "peak_nonpaged", "nonpaged", "pagefile", "peak_pagefile", "private")]
    kernel = C.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = C.c_void_p
    kernel.OpenProcess.argtypes, kernel.OpenProcess.restype = [C.c_uint32, C.c_int, C.c_uint32], C.c_void_p
    kernel.CloseHandle.argtypes = [C.c_void_p]
    handle = kernel.OpenProcess(0x410, False, pid) if pid is not None else kernel.GetCurrentProcess()
    if not handle:
        raise C.WinError(C.get_last_error())
    try:
        psapi = C.WinDLL("psapi", use_last_error=True)
        psapi.GetProcessMemoryInfo.argtypes = [C.c_void_p, C.POINTER(Memory), C.c_uint32]
        result = Memory()
        result.cb = C.sizeof(result)
        if not psapi.GetProcessMemoryInfo(handle, C.byref(result), result.cb):
            raise C.WinError(C.get_last_error())
        return {"process_rss_bytes": result.rss, "process_private_bytes": result.private}
    finally:
        if pid is not None:
            kernel.CloseHandle(handle)
