"""Exercise production empty/solid collision tile ownership with real Box3D."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/collision-tiles'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    owner = ROOT / 'octaryn-shared/Source/Libraries/CharacterMotion'
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(owner), '/I' + str(ROOT / 'build/dependencies/src/box3d/include'),
               str(ROOT / 'tools/Source/TileBudgetProbe/CollisionTiles.cpp'),
               str(owner / 'MeshCollisionScene.cpp'), str(owner / 'MeshCollisionWorld.cpp'),
               '/Fe:collision_tiles.exe', '/link',
               str(ROOT / 'build/release-windows/deps/build/box3d/src/box3d.lib')]
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(output / 'collision_tiles.exe')], check=True)


if __name__ == '__main__':
    main()
