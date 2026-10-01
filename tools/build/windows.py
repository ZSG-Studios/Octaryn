#!/usr/bin/env python3
"""Configure, build and run Octaryn natively on Windows.

Python twin of linux.py. The only Windows-specific work is importing the Visual
Studio developer environment (see vsenv.py); everything else matches linux.py.
"""
import argparse
import os
from pathlib import Path
import platform
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import vsenv
sys.path.insert(0, str(Path(__file__).resolve().parent / "support"))
import provision_tools


HOST_ARCH = {"amd64": "x64", "x86_64": "x64", "arm64": "arm64"}.get(platform.machine().lower())


def load_slang_rhi():
    import importlib.util
    path = Path(__file__).resolve().parent / "slang-rhi.py"
    spec = importlib.util.spec_from_file_location("slang_rhi_bootstrap", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_rhi(args):
    configuration = "Debug" if args.preset.startswith("debug") else "Release"
    bootstrap = load_slang_rhi()
    plan = bootstrap.build_plan("windows", args.architecture, configuration)
    bootstrap.build(plan, args.jobs)
    print(f"standalone slang-rhi ready: {plan['build']}")


def repo_commit():
    return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"],
                          check=True, text=True, stdout=subprocess.PIPE).stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action", choices=("configure", "build", "run-client", "run-server", "rhi"),
                        default="build")
    parser.add_argument("--preset", choices=("debug-windows", "release-windows"),
                        default="release-windows")
    parser.add_argument("--architecture", choices=("x64", "arm64"), default=HOST_ARCH)
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 2))
    parser.add_argument("--target", nargs="+", default=["octaryn_all"])
    parser.add_argument("--configure-argument", action="append", default=[])
    parser.add_argument("--client-argument", action="append", default=[])
    parser.add_argument("--server-argument", action="append", default=[])
    args = parser.parse_args()
    if platform.system() != "Windows":
        parser.error("Run this command on native Windows; Linux builds use linux.py")
    if args.architecture is None:
        parser.error("Native Windows x64 and arm64 are supported build targets")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    preset_root = args.preset + ("-arm64" if args.architecture == "arm64" else "")
    binary_dir = ROOT / "build" / preset_root / "cmake"
    if args.action == "rhi":
        run_rhi(args)
        return 0
    if args.action == "run-client":
        client = ROOT / "build" / preset_root / "client/bundle/Octaryn.Client.exe"
        if not client.is_file():
            parser.error(f"Build the client bundle first: {client}")
        return subprocess.call([str(client), *args.client_argument], cwd=client.parent)
    if args.action == "run-server":
        server = ROOT / "build" / preset_root / "server/bundle/Octaryn.Server.exe"
        if not server.is_file():
            parser.error(f"Build the server bundle first: {server}")
        return subprocess.call([str(server), *args.server_argument], cwd=server.parent)

    vs_root = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs_root, args.architecture)
    vsenv.prepend_tool_dirs(ROOT, vs_root, args.architecture)
    # Pinned CMake/Ninja land in build/dependencies/tools automatically; system
    # prerequisites (VS, .NET SDK, Git) stay manual. Set
    # OCTARYN_NO_TOOL_PROVISION=1 to require everything from PATH instead.
    provision_tools.ensure_pinned_tools(ROOT)
    vsenv.require_tools("cmake", "ninja", "clang-cl", "dotnet", "git")
    os.environ["OCTARYN_TARGET_ARCH"] = args.architecture
    if args.action == "configure":
        command = ["cmake", "--preset", args.preset, "-B", str(binary_dir),
                   f"-DOCTARYN_TARGET_ARCH={args.architecture}", *args.configure_argument]
    else:
        if not (binary_dir / "CMakeCache.txt").is_file():
            parser.error("Configure first with --action configure")
        command = ["cmake", "--build", str(binary_dir), "--target", *args.target,
                   "--parallel", str(args.jobs)]
    return subprocess.call(command, cwd=ROOT)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, RuntimeError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
