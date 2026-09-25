"""Compile production key/ABI checks and the independent GI path reference."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    owner = ROOT / 'octaryn-client/Source/Rendering/BlockTransportGI'
    test = ROOT / 'tools/validation/block_transport_test.cpp'
    lighting_test = ROOT / 'tools/validation/block_transport_lighting_test.cpp'
    reference = ROOT / 'tools/validation/block_transport_oracle.h'
    output = ROOT / 'build/release-windows/tools/block-transport'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', '/std:c++20', '/W4', '/WX',
                    '/I' + str(owner), str(test), str(lighting_test), '/Fe:block_transport_test.exe'],
                   cwd=output, check=True)
    result = subprocess.run([str(output / 'block_transport_test.exe')], cwd=ROOT,
                            text=True, capture_output=True, timeout=30)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    sources = (owner / 'BlockTransportTypes.h', owner / 'BlockTransportLighting.h', test, lighting_test,
               reference, Path(__file__).resolve())
    report = dict(status='passed' if result.returncode == 0 else 'failed',
                  production_keys=True, production_abi=True, lighting_temporal_policy=True, gpu_runtime=False,
                  transport_gpu_validation='requires --block-transport-only hardware probe',
                  stdout=result.stdout, stderr=result.stderr,
                  source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in sources})
    path = ROOT / 'logs/tools/block-transport-cpu.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    result.check_returncode()


if __name__ == '__main__':
    main()
