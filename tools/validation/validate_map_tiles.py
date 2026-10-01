"""Compile and check native tile cooking without a concurrent engine build."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/map-tiles'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'octaryn-client/Source/MapWorld'
    tool = ROOT / 'tools/Source/MapTileCook'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(source), '/I' + str(ROOT / 'build/dependencies/src/fastgltf/include')]
    command += [str(tool / name) for name in ('main.cpp', 'Materials.cpp', 'Textures.cpp', 'Write.cpp', 'Partition.cpp', 'Test.cpp', 'Verify.cpp', 'Order.cpp', 'Compare.cpp', 'OrderTest.cpp')]
    command += [str(source / name) for name in ('MapModel.cpp', 'MapMaterials.cpp', 'MapTextureHash.cpp', 'MapMipmaps.cpp', 'MapTextureCache.cpp')]
    command += ['/Fe:map_tile_cook.exe', '/link', str(ROOT / 'build/release-windows/deps/build/fastgltf/fastgltf.lib')]
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'map_tile_cook.exe'), '--self-test', str(ROOT / 'build/release-windows/tools/map-tile-test')],
                   check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    receipt = json.loads((ROOT / 'build/release-windows/tools/map-tile-test/order-morton/compare.json').read_text())
    assert receipt['triangles'] == 32 and len(receipt['multiset_sha256']) == 64
    for name in ('reference_bounds', 'candidate_bounds'):
        assert receipt[name]['tiles'] == len(receipt[name]['bounds']) == 8
    print('map_tile_compare_receipt passed=1 valid_json=1 full_bounds=1')


if __name__ == '__main__':
    main()
