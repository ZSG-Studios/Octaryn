"""Run exact production BLAS poll body against CPU-only lifecycle doubles."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def run():
    source_path = ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldRayBuild.cpp"
    source = source_path.read_text(encoding="utf-8")
    start = source.index("bool WorldRayTracing::State::poll(")
    end = source.index("bool WorldRayTracing::State::start(", start)
    body = source[start:end]
    output = ROOT / "build/release-windows/tools/world-ray-poll"
    output.mkdir(parents=True, exist_ok=True)
    (output / "WorldRayPollUnderTest.h").write_text(body, encoding="utf-8")
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/W4", "/WX",
                    "/I" + str(output), "/I" + str(source_path.parent),
                    str(ROOT / "tools/validation/world_ray_poll_test.cpp"),
                    "/Fe:world_ray_poll_test.exe"], cwd=output, check=True)
    result = subprocess.run([str(output / "world_ray_poll_test.exe")], cwd=ROOT,
                            text=True, capture_output=True)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()
    assert "phase=blas_poll step=fence_value last_success=begin result=0xffffffd6" in result.stderr
    assert "step=fence_device_alive last_success=fence_value" in result.stderr
    report = dict(status="passed", production_poll_body=True, gpu_runtime=False,
                  real_rhi_integration=False, poll_sha256=hashlib.sha256(body.encode()).hexdigest(),
                  stdout=result.stdout, stderr=result.stderr)
    log = ROOT / "logs/tools/world-ray-poll.json"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    run()
