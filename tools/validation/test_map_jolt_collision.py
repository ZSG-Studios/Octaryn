"""CPU-only real Jolt collision regression; no renderer or game launch."""
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

def main():
    motion = ROOT/'octaryn-shared/Source/Libraries/CharacterMotion'
    output = ROOT/'build/release-windows/tools/map-jolt-collision'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    args = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
            '/DNDEBUG', '/D_HAS_EXCEPTIONS=0', '/arch:AVX2',
            '-mbmi', '-mpopcnt', '-mlzcnt', '-mf16c', '-mfma']
    args += ['/D'+x for x in ('JPH_DEBUG_RENDERER', 'JPH_OBJECT_STREAM',
        'JPH_PROFILE_ENABLED', 'JPH_USE_AVX', 'JPH_USE_AVX2', 'JPH_USE_F16C',
        'JPH_USE_FMADD', 'JPH_USE_LZCNT', 'JPH_USE_SSE4_1', 'JPH_USE_SSE4_2',
        'JPH_USE_TZCNT')]
    args += ['/I'+str(motion), '/I'+str(ROOT/'build/dependencies/src/joltphysics')]
    args += [str(ROOT/'tools/validation/map_jolt_collision_test.cpp')]
    args += [str(motion/x) for x in ('PlayerJoltMesh.cpp', 'PlayerJoltWorld.cpp',
                                    'PlayerMovement.cpp', 'PlayerJoltMovement.cpp')]
    args += [str(ROOT/'build/release-windows/deps/build/joltphysics/Jolt.lib'),
             '/Fe:map_jolt_collision_test.exe']
    for command in (args, [str(output/'map_jolt_collision_test.exe')]):
        subprocess.run(command, cwd=output, check=True, timeout=120,
                       creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)

if __name__ == '__main__':
    main()
