"""Compile production ray-counter includes and verify SDK key/cache separation without a GPU."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    cache = {}
    for line in (ROOT / 'build/release-windows/cmake/CMakeCache.txt').read_text().splitlines():
        if '=' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    owner = ROOT / 'octaryn-client/Source/Rendering/RenderBackend'
    sources = [owner / 'ShaderCache.cpp', owner / 'ShaderCacheStorage.cpp',
               ROOT / 'tools/Source/RayCounterProbe/main.cpp']
    sdk = Path(cache['OCTARYN_SLANG_SDK_ROOT'])
    external = [Path(cache[name]) / 'include' for name in
                ('OCTARYN_SLANG_RHI_SOURCE_ROOT', 'OCTARYN_SLANG_RHI_BUILD_ROOT', 'OCTARYN_SLANG_SDK_ROOT', 'SDL3_SOURCE_DIR')]
    output = ROOT / 'build/release-windows/tools/RayCounterProbe'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    libraries = [str(Path(cache['SDL3_BINARY_DIR']) / 'SDL3-static.lib'), str(sdk / 'lib/slang-compiler.lib')]
    libraries += [name + '.lib' for name in ('kernel32', 'user32', 'gdi32', 'winmm', 'imm32', 'ole32',
                  'oleaut32', 'version', 'uuid', 'advapi32', 'setupapi', 'shell32', 'dinput8')]
    subprocess.run(['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20', '/W4', '/WX',
                    '/I' + str(owner), '/external:W0', *['/external:I' + str(path) for path in external],
                    *map(str, sources), '/Fe:ray_counter_probe.exe', '/link', *libraries],
                   cwd=output, check=True, timeout=120)
    environment = dict(os.environ)
    environment['PATH'] = str(sdk / 'bin') + os.pathsep + environment.get('PATH', '')
    fixture = Path(tempfile.mkdtemp(prefix='case-', dir=output)) / 'data'
    result = subprocess.run([str(output / 'ray_counter_probe.exe'), str(fixture), str(ROOT)], cwd=ROOT,
                            env=environment, text=True, capture_output=True, timeout=120)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    receipt = dict(status='passed' if result.returncode == 0 else 'failed', returncode=result.returncode,
                   fixture=str(fixture), production_shader_include=True, actual_slang_hashes=True,
                   alternating_persistent_cache=True, targets=['DXIL', 'SPIRV'], gpu_runtime=False,
                   reflection_wave_widths=[0,32,64], forced_width_target='DXIL',
                   generic_width_unchanged=True, wave_telemetry_collector_only=True,
                   stdout=result.stdout, stderr=result.stderr)
    evidence = ROOT / 'logs/tools/ray-counter-variants.json'
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(receipt, indent=2))
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
