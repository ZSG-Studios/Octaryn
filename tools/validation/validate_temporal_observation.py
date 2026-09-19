"""Qualify diagnostic clock exclusion against the production temporal camera."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    output = ROOT / "build/release-windows/tools/temporal-observation"
    output.mkdir(parents=True, exist_ok=True)
    temporal = ROOT / "octaryn-client/Source/Rendering/Temporal"
    backend = ROOT / "octaryn-client/Source/Rendering/RenderBackend"
    source = ROOT / "tools/validation/temporal_observation_test.cpp"
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/W4", "/WX",
                    "/I" + str(temporal), "/I" + str(backend), str(source),
                    "/Fe:temporal_observation_test.exe"], cwd=output, check=True)
    result = subprocess.run([str(output / "temporal_observation_test.exe")],
                            text=True, capture_output=True, check=True)
    report = dict(status="passed", gpu_runtime=False, stdout=result.stdout, stderr=result.stderr,
                  source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in (source, temporal / "TemporalCamera.h", temporal / "TemporalObservation.h")})
    log = ROOT / "logs/tools/temporal-observation.json"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(report, indent=2) + "\n")
    print(result.stdout, end="")
    print(f"report={log}")


if __name__ == "__main__":
    main()
