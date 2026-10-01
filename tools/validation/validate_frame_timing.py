"""Exercise production DRS query accounting without submitting GPU work."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    output = ROOT / 'build/release-windows/tools/FrameTimingProbe'
    output.mkdir(parents=True, exist_ok=True)
    slang = sorted((ROOT / 'build/dependencies').glob('slang-*/include/slang.h'))
    if len(slang) != 1:
        raise RuntimeError('Requires one configured Slang SDK header directory')
    command = ['clang-cl', '/nologo', '/O1', '/MD', '/EHsc', '/std:c++20',
               '/I' + str(ROOT / 'build/dependencies/slang-rhi/include'),
               '/I' + str(ROOT / 'build/dependencies/slang-rhi-windows-x64-Release/include'),
               '/I' + str(slang[0].parent),
               str(ROOT / 'tools/Source/FrameTimingProbe/main.cpp'), '/Fe:frame_timing_probe.exe']
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    subprocess.run([str(output / 'frame_timing_probe.exe')], check=True)


if __name__ == '__main__':
    main()
