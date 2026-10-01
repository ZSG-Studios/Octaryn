"""Compile the production AS admission policy fixture without graphics execution."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    output = ROOT / 'build/release-windows/tools/tile-ray-scheduling'
    output.mkdir(parents=True, exist_ok=True)
    command = ['clang-cl', '/nologo', '/O1', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(ROOT / 'octaryn-client/Source/MapWorld'),
               '/I' + str(ROOT / 'octaryn-client/Source/WorldStreaming'),
               str(ROOT / 'tools/Source/TileBudgetProbe/RayScheduling.cpp'), '/Fe:tile_ray_scheduling.exe']
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'tile_ray_scheduling.exe')], check=True)


if __name__ == '__main__':
    main()
