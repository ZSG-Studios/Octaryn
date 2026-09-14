"""Compile the actual patched FSR2 passes with Slang; source proof, not GPU proof."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slangc", required=True, type=Path)
    parser.add_argument("--vendor", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--target", choices=("spirv", "dxil", "metal"), default="spirv")
    parser.add_argument("--dxc", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    args.output.mkdir(parents=True, exist_ok=True)
    profile = {"spirv": "spirv_1_3", "dxil": "sm_6_0", "metal": "sm_6_0"}[args.target]
    results = []
    for path in sorted((root / "octaryn-client/Shaders/Fsr2").glob("*.slang")):
        if path.stem == "Options":
            continue
        variants = [(0, 0)]
        if path.stem in ("Accumulate", "Reconstruct", "DepthClip"):
            variants += [(1, 0)]
        if path.stem == "Accumulate":
            variants += [(0, 1), (1, 1)]
        for inverted, sharpen, hdr in [(depth, sharp, hdr) for depth, sharp in variants for hdr in (0, 1)]:
            name = f"{path.stem}-hdr{hdr}-depth{inverted}-sharpen{sharpen}-{args.target}"
            command = [str(args.slangc.resolve()), str(path), "-I", str(args.vendor.resolve()),
                       "-entry", "CS", "-stage", "compute", "-target", args.target,
                       "-profile", profile, "-o", str(args.output / (name + ".bin")),
                       f"-DFFX_FSR2_OPTION_INVERTED_DEPTH={inverted}",
                       f"-DFFX_FSR2_OPTION_HDR_COLOR_INPUT={hdr}",
                       f"-DFFX_FSR2_OPTION_APPLY_SHARPENING={sharpen}"]
            if args.target == "metal":
                command += ["-capability", "metallib_3_1"]
            environment = os.environ.copy()
            if args.dxc:
                environment["PATH"] = str(args.dxc.resolve().parent) + os.pathsep + environment.get("PATH", "")
            result = subprocess.run(command, capture_output=True, text=True, timeout=120, env=environment)
            (args.output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            results.append({"case": name, "exit": result.returncode})
            print(f"fsr2_shader case={name} exit={result.returncode}", flush=True)
    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    return 1 if any(result["exit"] for result in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())
