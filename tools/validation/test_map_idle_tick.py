"""Compile the real map authority tick against a deterministic motion test double."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

def main():
    output = ROOT / 'build/release-windows/tools/map-idle-tick'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    includes = {next((ROOT/owner/'Source').rglob(name)).parent for owner, name in
                (('octaryn-server', 'MapWorld.h'),
                 ('octaryn-server', 'PlayerSimulation.h'),
                 ('octaryn-shared', 'CharacterMotion.h'),
                 ('octaryn-shared', 'octaryn_shared_abi_types.h'))}
    args = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
            '/DOCTARYN_MAP_WORLD_EXPORTS']
    args += ['/I'+str(path) for path in includes]
    args += [str(ROOT/'tools/validation/map_idle_tick_test.cpp'),
             str(ROOT/'octaryn-server/Source/World/MapWorld/MapWorldSession.cpp'),
             '/Fe:map_idle_tick_test.exe']
    for command in (args, [str(output/'map_idle_tick_test.exe')]):
        subprocess.run(command, cwd=output, check=True, timeout=60,
                       creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)

if __name__ == '__main__':
    main()
