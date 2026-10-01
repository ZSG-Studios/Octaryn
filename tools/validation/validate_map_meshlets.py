"""Exercise production meshlet topology, material boundaries, winding and bounds."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/map-meshlet-probe'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    maps = ROOT / 'octaryn-client/Source/MapWorld'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(maps), '/I' + str(ROOT / 'build/dependencies/src/meshoptimizer/src'),
               str(ROOT / 'tools/Source/MapMeshletProbe/main.cpp'), str(maps / 'MapMeshlets.cpp'),
               '/Fe:map_meshlet_probe.exe', '/link',
               str(ROOT / 'build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib')]
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(output / 'map_meshlet_probe.exe')], check=True)


if __name__ == '__main__':
    main()
