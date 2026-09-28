"""Exercise production texture publication/accounting without a GPU device."""
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    ninja = (ROOT / 'build/release-windows/cmake/build.ninja').read_text()
    block = next(b for b in ninja.split('\n\n') if b.startswith(
        'build CMakeFiles/octaryn_client_render_backend.dir/octaryn-client/Source/MapWorld/MapRendererIndirect.cpp.obj:'))
    includes = shlex.split(re.search(r'^  INCLUDES = (.*)$', block, re.M).group(1))
    output = ROOT / 'build/release-windows/tools/map-texture-reuse'
    output.mkdir(parents=True, exist_ok=True)
    command = ['clang-cl', '/nologo', '/O1', '/MD', '/EHsc', '/std:c++20', '/Gy', *includes,
               str(ROOT / 'tools/Source/MapTextureReuseProbe/main.cpp'),
               str(ROOT / 'octaryn-client/Source/MapWorld/MapAssetTextureUpload.cpp'),
               '/Fe:map_texture_reuse.exe', '/link', '/OPT:REF']
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'map_texture_reuse.exe')], check=True)


if __name__ == '__main__':
    main()
