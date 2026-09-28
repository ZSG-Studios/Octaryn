"""Execute production screen-ray DDA against an independent segment/box oracle."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/screen-grid'
    output.mkdir(parents=True, exist_ok=True)
    probe = ROOT / 'tools/Source/ScreenGridProbe'
    compiler = ROOT / 'build/dependencies/slang-2026.17.1/bin/slangc.exe'
    subprocess.run([str(compiler), str(probe / 'Probe.slang'), '-entry', 'main',
                    '-target', 'cpp', '-o', str(output / 'ScreenGrid.cpp')], check=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', '/std:c++20', '/I' + str(output),
                    str(probe / 'main.cpp'), '/Fe:screen_grid_test.exe'], cwd=output, check=True)
    subprocess.run([str(output / 'screen_grid_test.exe')], check=True)


if __name__ == '__main__':
    main()
