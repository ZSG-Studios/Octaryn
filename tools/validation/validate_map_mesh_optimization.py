"""CPU-only exact map mesh optimization regression; never launches the engine."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    source = ROOT / 'octaryn-client/Source/MapWorld'
    dependency = ROOT / 'build/dependencies/src/meshoptimizer/src'
    output = ROOT / 'build/release-windows/tools/map-mesh-optimization'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(source), '/I' + str(dependency),
               str(ROOT / 'tools/validation/map_mesh_optimization_test.cpp'),
               str(source / 'MapMeshOptimization.cpp')]
    command += [str(dependency / (name + '.cpp')) for name in
                ('allocator', 'indexgenerator', 'vcacheoptimizer', 'vfetchoptimizer')]
    command += ['/Fe:map_mesh_optimization_test.exe']
    log = ROOT / 'logs/tools/map-mesh-optimization.log'
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w') as stream:
        for args in (command, [str(output / 'map_mesh_optimization_test.exe')]):
            result = subprocess.run(args, cwd=output, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            print(result.stdout, end='')
            stream.write(result.stdout)
            result.check_returncode()


if __name__ == '__main__':
    main()
