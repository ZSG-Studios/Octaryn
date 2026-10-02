"""Compile and execute the portable scene preview camera contract; no GPU or CTest."""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
import vsenv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "build/windows-x64/tools/scene-preview-camera")
    args = parser.parse_args()
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    vs = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs, "x64")
    vsenv.prepend_tool_dirs(ROOT, vs, "x64")
    executable = output / "ScenePreviewCameraProbe.exe"
    command = [vsenv.resolve_tool("clang-cl"), "/nologo", "/std:c++20", "/EHsc", "/MD", "/O2", "/W4", "/WX",
               "/D_CRT_SECURE_NO_WARNINGS", str(ROOT / "tools/Source/ScenePreviewCameraProbe/main.cpp"),
               "/Fe" + str(executable), "/Fo" + str(output / "main.obj")]
    started = time.perf_counter()
    receipt = {"kind": "authored-cpu-contract", "gpu": False, "command": command, "status": "failed"}
    with (output / "checks.log").open("w", encoding="utf-8") as log:
        compiled = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        receipt["compileExitCode"] = compiled.returncode
        if compiled.returncode == 0:
            checked = subprocess.run([str(executable)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=20)
            receipt["checksExitCode"] = checked.returncode
            if checked.returncode == 0:
                receipt["status"] = "passed"
    receipt["seconds"] = time.perf_counter() - started
    (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print((output / "checks.log").read_text(encoding="utf-8"))
    print(json.dumps({"status": receipt["status"], "seconds": receipt["seconds"], "receipt": str(output / "result.json")}))
    return 0 if receipt["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
