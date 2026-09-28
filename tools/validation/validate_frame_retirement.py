"""Exercise the actual bounded profiling writer and fence state machine without a GPU."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs
from frame_retirement_report import read_trace


def main():
    cache = {}
    for line in (ROOT / 'build/release-windows/cmake/CMakeCache.txt').read_text().splitlines():
        if '=' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    output = ROOT / 'build/release-windows/tools/FrameRetirementProbe'
    output.mkdir(parents=True, exist_ok=True)
    owner = ROOT / 'octaryn-client/Source/Rendering/RenderBackend'
    sources = [ROOT / 'tools/Source/FrameRetirementProbe/main.cpp',
               ROOT / 'octaryn-client/Source/Threading/ThreadCpuTime.cpp']
    external = [Path(cache[name]) / 'include' for name in ('OCTARYN_SLANG_SDK_ROOT', 'SDL3_SOURCE_DIR')]
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    libraries = [str(Path(cache['SDL3_BINARY_DIR']) / 'SDL3-static.lib')]
    libraries += [name + '.lib' for name in ('kernel32', 'user32', 'gdi32', 'winmm', 'imm32', 'ole32',
                  'oleaut32', 'version', 'uuid', 'advapi32', 'setupapi', 'shell32', 'dinput8')]
    subprocess.run(['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20', '/W4', '/WX', '/D_CRT_SECURE_NO_WARNINGS',
                    '/I' + str(owner), '/external:W0', *['/external:I' + str(path) for path in external],
                    *map(str, sources), '/Fe:frame_retirement_probe.exe', '/link', *libraries],
                   cwd=output, check=True, timeout=120)
    fixture = Path(tempfile.mkdtemp(prefix='case-', dir=output)) / 'data'
    result = subprocess.run([str(output / 'frame_retirement_probe.exe'), str(fixture)], cwd=ROOT,
                            text=True, capture_output=True, timeout=30)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    receipt = dict(status='failed', returncode=result.returncode, fixture=str(fixture),
                   gpu_runtime=False, stdout=result.stdout, stderr=result.stderr)
    try:
        if result.returncode:
            raise RuntimeError('Native retirement fixture failed')
        # Failure markers are expected only in the deliberately failing writers.
        gpu = fixture / 'gpu.csv'
        gpu.write_text('frame\n42\n')
        receipt['valid_trace'] = read_trace(fixture / 'retirement.csv', gpu_csv=gpu)
        try:
            read_trace(fixture / 'trace-overflow.csv')
        except ValueError:
            receipt['overflow_trace_rejected'] = True
        else:
            raise RuntimeError('Saturated trace was accepted')
        receipt['status'] = 'passed'
    finally:
        evidence = ROOT / 'logs/tools/frame-retirement-probe.json'
        evidence.parent.mkdir(parents=True, exist_ok=True)
        evidence.write_text(json.dumps(receipt, indent=2))
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
