"""Run production streaming budget arithmetic at threshold/overflow boundaries."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

def main():
    output = ROOT / 'build/release-windows/tools/tile-budget'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(ROOT / 'octaryn-client/Source/WorldStreaming'),
               '/I' + str(ROOT / 'octaryn-client/Source/MapWorld'),
               str(ROOT / 'tools/Source/TileBudgetProbe/main.cpp'), '/Fe:tile_budget_probe.exe']
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(output / 'tile_budget_probe.exe')], check=True)
    slang = sorted((ROOT / 'build/dependencies').glob('slang-*/include/slang.h'))
    if len(slang) != 1:
        raise RuntimeError('Expected exactly one pinned Slang header installation')
    command = command[:command.index(str(ROOT / 'tools/Source/TileBudgetProbe/main.cpp'))]
    command += ['/I' + str(ROOT / 'build/dependencies/slang-rhi/include'), '/I' + str(slang[0].parent),
                '/I' + str(ROOT / 'build/dependencies/slang-rhi-windows-x64-Release/include'),
                str(ROOT / 'tools/Source/TileBudgetProbe/RayInputs.cpp'),
                str(ROOT / 'octaryn-client/Source/MapWorld/MapRayResources.cpp'), '/Fe:ray_input_probe.exe']
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(output / 'ray_input_probe.exe')], check=True)

if __name__ == '__main__':
    main()
