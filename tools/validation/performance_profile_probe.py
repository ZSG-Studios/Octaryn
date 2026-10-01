"""Execute the native profile controller and bounded asynchronous writer."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
import vsenv


def main():
    vs = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs, 'x64')
    vsenv.prepend_tool_dirs(ROOT, vs, 'x64')
    output = ROOT / 'build/release-windows/tools/performance-profile-probe'
    output.mkdir(parents=True, exist_ok=True)
    executable = output / 'probe.exe'
    subprocess.run([vsenv.resolve_tool('clang-cl'), '/nologo', '/std:c++20', '/EHsc',
                    '/W4', '/I' + str(ROOT), str(Path(__file__).with_suffix('.cpp')),
                    '/Fe' + str(executable), '/Fo' + str(output / 'probe.obj')], check=True, cwd=output)
    subprocess.run([str(executable), str(output / 'rows.csv')], check=True, timeout=20)


if __name__ == '__main__':
    main()
