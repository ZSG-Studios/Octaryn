"""Qualify the bounded resident block-light selector without RHI or GPU work."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    owner = ROOT / 'octaryn-client/Source/Rendering/RenderBackend'
    source = owner / 'BlockLightSelection.h'
    test = ROOT / 'tools/validation/block_light_selection_test.cpp'
    output = ROOT / 'build/release-windows/tools/block-light-selection'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', '/std:c++20', '/W4', '/WX',
                    '/I' + str(owner), str(test), '/Fe:block_light_selection_test.exe'],
                   cwd=output, check=True)
    result = subprocess.run([str(output / 'block_light_selection_test.exe')], cwd=ROOT,
                            text=True, capture_output=True, timeout=30)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    report = dict(status='passed' if result.returncode == 0 else 'failed',
                  production_selector=True, gpu_runtime=False, stdout=result.stdout, stderr=result.stderr,
                  source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in (source, owner / 'BlockLights.cpp', owner / 'LocalLight.h', test)})
    path = ROOT / 'logs/tools/block-light-selection.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    result.check_returncode()


if __name__ == '__main__':
    main()
