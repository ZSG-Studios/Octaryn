"""Compile only the CPU offline cooker and run bounded codec/cache fixtures."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/map-texture-cache'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'octaryn-client/Source/MapWorld'
    codec = ROOT / 'build/dependencies/src/bc7enc_rdo'
    tool = ROOT / 'tools/Source/MapTextureCook'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(source), '/I' + str(codec),
               '/I' + str(ROOT / 'build/dependencies/src/fastgltf/include'),
               '/I' + str(ROOT / 'build/dependencies/src/stb')]
    command += [str(tool / name) for name in ('main.cpp', 'Encode.cpp', 'Test.cpp', 'Compare.cpp')]
    command += [str(source / name) for name in ('MapModel.cpp', 'MapMaterials.cpp', 'MapImages.cpp',
                'MapMipmaps.cpp', 'MapTextureCache.cpp', 'MapTextureHash.cpp')]
    command += [str(codec / 'bc7enc.cpp'), str(codec / 'bc7decomp.cpp'),
                '/Fe:map_texture_cook.exe', '/link',
                str(ROOT / 'build/release-windows/deps/build/fastgltf/fastgltf.lib')]
    subprocess.run(command, cwd=output, check=True,
                   creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    fixture_root = ROOT / 'build/release-windows/tools/map-texture-cache-test'
    subprocess.run([str(output / 'map_texture_cook.exe'), '--self-test', str(fixture_root)],
                   check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)


if __name__ == '__main__':
    main()
