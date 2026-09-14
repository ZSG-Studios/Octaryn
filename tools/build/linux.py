#!/usr/bin/env python3
"""Configure, build and run Octaryn directly on a native Linux host or WSL2."""
import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action", choices=("configure", "build", "run-client"), default="build")
    parser.add_argument("--preset", choices=("debug-linux", "release-linux"), default="release-linux")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 2))
    parser.add_argument("--target", nargs="+", default=["octaryn_all"])
    parser.add_argument("--configure-argument", action="append", default=[])
    parser.add_argument("--client-argument", action="append", default=[])
    args = parser.parse_args()
    if platform.system() != "Linux":
        parser.error("Run this command inside Linux/WSL2; Windows builds use windows.ps1")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    arch = {"x86_64": "x64", "aarch64": "arm64"}.get(platform.machine().lower())
    if arch is None:
        parser.error("Native Linux x64 and arm64 are supported build targets")
    preset_root = args.preset + ("-arm64" if arch == "arm64" else "")
    build = ROOT / "build" / preset_root / "cmake"
    if args.action == "run-client":
        bundle = ROOT / "build" / preset_root / "client/bundle"
        client = bundle / "Octaryn.Client"
        if not client.is_file():
            parser.error(f"Build the client bundle first: {client}")
        return subprocess.call([str(client), *args.client_argument], cwd=bundle)
    for tool in ("cmake", "ninja", "clang", "clang++", "dotnet", "git"):
        if not shutil.which(tool):
            parser.error(f"Missing required tool: {tool}; see docs/build/README.md")
    environment = os.environ.copy()
    environment["OCTARYN_TARGET_ARCH"] = arch
    if args.action == "configure":
        command = ["cmake", "--preset", args.preset, "-B", str(build),
                   f"-DOCTARYN_TARGET_ARCH={arch}", *args.configure_argument]
    else:
        if not (build / "CMakeCache.txt").is_file():
            parser.error("Configure first with --action configure")
        command = ["cmake", "--build", str(build), "--target", *args.target,
                   "--parallel", str(args.jobs)]
    return subprocess.call(command, cwd=ROOT, env=environment)


if __name__ == "__main__":
    sys.exit(main())
