"""Check production scene-capacity arithmetic, exclusive reuse and PMR history allocations."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/scene-capacity'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(ROOT / 'octaryn-client/Source/Rendering/RenderBackend'),
               '/I' + str(ROOT / 'octaryn-client/Source/Rendering/Items'),
               str(ROOT / 'tools/Source/TileBudgetProbe/SceneCapacity.cpp'), '/Fe:scene_capacity_probe.exe']
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(output / 'scene_capacity_probe.exe')], check=True)
    slang = sorted((ROOT / 'build/dependencies').glob('slang-*/include/slang.h'))
    if len(slang) != 1:
        raise RuntimeError('Expected exactly one pinned Slang header installation')
    command = command[:command.index(str(ROOT / 'tools/Source/TileBudgetProbe/SceneCapacity.cpp'))]
    command += ['/I' + str(ROOT / 'build/dependencies/slang-rhi/include'), '/I' + str(slang[0].parent),
                '/I' + str(ROOT / 'build/dependencies/slang-rhi-windows-x64-Release/include'),
                '/I' + str(ROOT / 'octaryn-client/Source/WorldStreaming'),
                str(ROOT / 'tools/Source/TileBudgetProbe/Reservation.cpp'),
                str(ROOT / 'octaryn-client/Source/Rendering/RenderBackend/DeviceMemory.cpp'),
                '/Fe:reservation_probe.exe']
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(output / 'reservation_probe.exe')], check=True)


if __name__ == '__main__':
    main()
