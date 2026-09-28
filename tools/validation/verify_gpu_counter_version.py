"""Check pinned DLL resource/API version ordering without initializing GPA or creating a GPU device."""
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import sys


def resource_versions(path):
    version = ctypes.WinDLL('version.dll')
    version.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
    version.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    version.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    version.GetFileVersionInfoW.restype = wintypes.BOOL
    version.VerQueryValueW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR,
                                     ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT)]
    version.VerQueryValueW.restype = wintypes.BOOL
    ignored = wintypes.DWORD()
    size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(ignored))
    if not size:
        raise RuntimeError('DLL has no Windows version resource')
    data = ctypes.create_string_buffer(size)
    if not version.GetFileVersionInfoW(str(path), 0, size, data):
        raise RuntimeError('Cannot read DLL version resource')
    pointer, length = ctypes.c_void_p(), wintypes.UINT()
    if not version.VerQueryValueW(data, '\\', ctypes.byref(pointer), ctypes.byref(length)) or length.value < 52:
        raise RuntimeError('Invalid DLL fixed version resource')
    fixed = ctypes.cast(pointer, ctypes.POINTER(wintypes.DWORD * 13)).contents
    if fixed[0] != 0xFEEF04BD:
        raise RuntimeError('DLL fixed version signature mismatch')
    return [[fixed[index] >> 16, fixed[index] & 65535, fixed[index + 1] >> 16, fixed[index + 1] & 65535]
            for index in (2, 4)]


def main():
    root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(root / 'tools/build/support'))
    from acquire_gpu_perf_api import pin, api_version
    spec = pin(root)
    cache = root / 'build/dependencies/tools/gpu-perf-api' / spec['TAG']
    receipt = json.loads((cache / 'receipt.json').read_text())
    relative = spec['SOURCE_SUBDIR'] + '/bin/GPUPerfAPIDX12-x64.dll'
    dll = cache / 'package' / relative
    digest = hashlib.sha256(dll.read_bytes()).hexdigest()
    if receipt['sha256'] != spec['sha256'] or receipt['files'][relative] != digest:
        raise RuntimeError('Pinned SDK DLL identity mismatch')
    file_version, product_version = resource_versions(dll)
    expected_release = list(map(int, spec['TAG'].split('.')))
    if file_version != expected_release or product_version != expected_release:
        raise RuntimeError('DLL file/product resource differs from release pin')
    library = ctypes.WinDLL(str(dll), winmode=0x1100)
    function = library.GpaGetVersion
    function.argtypes = [ctypes.POINTER(ctypes.c_uint32)] * 4
    function.restype = ctypes.c_int32
    values = [ctypes.c_uint32() for _ in range(4)]
    status = function(*(ctypes.byref(value) for value in values))
    actual = [value.value for value in values]
    if status != 0 or actual != api_version(spec['TAG']):
        raise RuntimeError('GpaGetVersion differs from strict pinned API argument order')
    result = dict(status='passed', dll=str(dll), sha256=digest, archive_sha256=spec['sha256'],
                  release_version=spec['TAG'], file_version=file_version, product_version=product_version,
                  api_argument_order=['major', 'minor', 'build', 'update'], api_values=actual,
                  GpaInitialize_called=False, gpu_device_created=False)
    output = root / 'logs/build/gpu-counter-version-verified.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
