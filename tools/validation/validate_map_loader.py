"""Compile and run CPU regression fixtures against the production GLB loader."""
from pathlib import Path
import argparse
import subprocess
import sys

from map_loader_fixtures import write_fixtures

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--asset', type=Path)
    args = parser.parse_args()
    output = ROOT / 'build/release-windows/tools/map-loader'
    write_fixtures(output / 'fixtures')
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'octaryn-client/Source/MapWorld'
    server_source = ROOT / 'octaryn-server/Source/World/MapWorld'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(source),
               '/I' + str(server_source),
               '/I' + str(ROOT / 'build/dependencies/src/fastgltf/include'),
               str(ROOT / 'tools/validation/map_loader_test.cpp'),
               str(source / 'MapModel.cpp'), str(source / 'MapMaterials.cpp'),
               str(server_source / 'MapSceneGeometry.cpp'),
               '/Fe:map_loader_test.exe', '/link',
               str(ROOT / 'build/release-windows/deps/build/fastgltf/fastgltf.lib')]
    subprocess.run(command, cwd=output, check=True)
    command = [str(output / 'map_loader_test.exe'), str(output / 'fixtures')]
    if args.asset:
        command.append(str(args.asset.resolve()))
    subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
