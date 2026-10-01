"""CPU-only fake Vulkan execution of the production private initialization queue."""
from pathlib import Path
import re
import shlex
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

def main():
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    build = ROOT / 'build/dependencies/slang-rhi-windows-x64-Release'
    block = next(b for b in (build / 'build.ninja').read_text().split('\n\n')
                 if b.startswith('build ') and 'vk-device-queue.cpp.obj:' in b)
    args = []
    for name in ('DEFINES', 'INCLUDES'):
        args.extend(shlex.split(re.search(r'^  '+name+r' = (.*)$', block, re.M).group(1), posix=False))
    args = [a.replace('\\"', '"') for a in args]
    args = [a[1:-1] if a.startswith('"') and a.endswith('"') else a for a in args]
    output = ROOT / 'build/release-windows/tools/vulkan-init-probe'
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'build/dependencies/slang-rhi/src'
    for name in ('Concurrency', 'Terminal'):
        executable = f'vulkan_init_{name.lower()}.exe'
        command = ['clang-cl', '/nologo', '/O1', '/MD', '/EHsc', '/std:c++20', *args,
                   str(ROOT / f'tools/Source/VulkanInitProbe/{name}.cpp'),
                   str(source / 'vulkan/vk-device-queue.cpp'), str(source / 'core/assert.cpp'),
                   '/Fe:' + executable]
        subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        subprocess.run([str(output / executable)], check=True, timeout=30)
if __name__ == '__main__':
    main()
