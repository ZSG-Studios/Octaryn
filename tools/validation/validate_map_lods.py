"""Compile and exercise the offline geometry cooker without GPU/build contention."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/map-geometry-cache'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'octaryn-client/Source/MapWorld'
    tool = ROOT / 'tools/Source/MapGeometryCook'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(source), '/I' + str(ROOT / 'build/dependencies/src/stb'), '/I' + str(ROOT / 'build/dependencies/src/meshoptimizer/src'),
               '/I' + str(ROOT / 'build/dependencies/src/fastgltf/include')]
    command += [str(tool / name) for name in ('main.cpp', 'Simplify.cpp', 'Test.cpp', 'PrepareTest.cpp', 'Tiles.cpp')]
    command += [str(source / name) for name in ('MapModel.cpp', 'MapMaterials.cpp',
                'MapMeshOptimization.cpp', 'MapLodCache.cpp', 'MapTextureHash.cpp', 'MapAssetPrepare.cpp', 'MapMeshlets.cpp', 'MapImages.cpp', 'MapMipmaps.cpp', 'MapTextureCache.cpp')]
    command += ['/Fe:map_geometry_cook.exe', '/link',
                str(ROOT / 'build/release-windows/deps/build/fastgltf/fastgltf.lib'),
                str(ROOT / 'build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib')]
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'map_geometry_cook.exe'), '--self-test',
                    str(ROOT / 'build/release-windows/tools/map-lod-test')],
                   check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)


if __name__ == '__main__':
    main()
