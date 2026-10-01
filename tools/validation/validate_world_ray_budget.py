"""Exercise production ray-build budgets with a deterministic CPU clock."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    source = ROOT / 'octaryn-client/Source/Rendering/RenderBackend/WorldRayBuildBudget.h'
    output = ROOT / 'build/release-windows/tools/world-ray-budget'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', '/std:c++20', '/W4', '/WX',
                    '/I' + str(source.parent), str(ROOT / 'tools/validation/world_ray_budget_test.cpp'),
                    '/Fe:world_ray_budget_test.exe'], cwd=output, check=True)
    result = subprocess.run([str(output / 'world_ray_budget_test.exe')], cwd=ROOT,
                            text=True, capture_output=True)
    print(result.stdout, end='')
    print(result.stderr, end='', file=sys.stderr)
    result.check_returncode()
    report = dict(status='passed', production_budget=True, fake_clock=True, gpu_runtime=False,
                  source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), stdout=result.stdout)
    path = ROOT / 'logs/tools/world-ray-budget.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
