"""Execute production cloud rejection against reference projected sample depths."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / 'build/release-windows/tools/cloud-occlusion'
    output.mkdir(parents=True, exist_ok=True)
    probe = ROOT / 'tools/Source/CloudOcclusionProbe'
    compiler = ROOT / 'build/dependencies/slang-2026.17.1/bin/slangc.exe'
    subprocess.run([str(compiler), str(probe / 'Probe.slang'), '-entry', 'main',
                    '-target', 'cpp', '-o', str(output / 'CloudOcclusion.cpp')], check=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', '/std:c++20', '/I' + str(output),
                    str(probe / 'main.cpp'), '/Fe:cloud_occlusion_test.exe'], cwd=output, check=True)
    subprocess.run([str(output / 'cloud_occlusion_test.exe')], check=True)


if __name__ == '__main__':
    main()
