"""CPU-only sampler descriptor identity regression; no device or GPU creation."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/map-sampler-probe'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    includes = ['octaryn-client/Source/MapWorld', 'build/dependencies/slang-rhi/include',
                'build/dependencies/slang-rhi-windows-x64-Release/include']
    slang = sorted((ROOT / 'build/dependencies').glob('slang-*/include/slang.h'))
    if len(slang) != 1:
        raise RuntimeError('Expected exactly one pinned Slang header installation')
    command = ['clang-cl', '/nologo', '/MD', '/EHsc', '/std:c++20']
    command += ['/I' + str(ROOT / path) for path in includes] + ['/I' + str(slang[0].parent)]
    command += [str(ROOT / 'tools/Source/MapSamplerProbe/main.cpp'),
                str(ROOT / 'octaryn-client/Source/MapWorld/MapSamplerCache.cpp'), '/Fe:map_sampler_probe.exe']
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'map_sampler_probe.exe')], check=True)


if __name__ == '__main__':
    main()
