"""Compile and check the production selected-emitter membership builder."""
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
    source, test = owner / 'GICoverage.cpp', ROOT / 'tools/validation/gi_emitter_coverage_test.cpp'
    output = ROOT / 'build/release-windows/tools/gi-emitter-coverage'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', '/std:c++20', '/W4', '/WX',
                    '/I' + str(owner), str(source), str(test), '/Fe:gi_emitter_coverage_test.exe'],
                   cwd=output, check=True)
    result = subprocess.run([str(output / 'gi_emitter_coverage_test.exe')], cwd=ROOT,
                            text=True, capture_output=True, timeout=30)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    report = dict(status='passed' if result.returncode == 0 else 'failed',
                  production_membership=True, gpu_runtime=False, stdout=result.stdout, stderr=result.stderr,
                  source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in (owner / 'GICoverage.h', source, test)})
    path = ROOT / 'logs/tools/gi-emitter-coverage.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n')
    result.check_returncode()


if __name__ == '__main__':
    main()
