"""Build and run production map mip filtering CPU tests; no GPU or engine launch."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/map-mips'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'octaryn-client/Source/MapWorld'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(source), str(ROOT / 'tools/validation/map_mip_test.cpp'),
               str(source / 'MapMipmaps.cpp'), '/Fe:map_mip_test.exe']
    subprocess.run(command, cwd=output, check=True,
                   creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'map_mip_test.exe')], check=True,
                   creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)


if __name__ == '__main__':
    main()
