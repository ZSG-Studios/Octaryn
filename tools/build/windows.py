#!/usr/bin/env python3
"""Configure, build and run Octaryn natively on Windows.

Python twin of linux.py. The only Windows-specific work is importing the Visual
Studio developer environment (see vsenv.py); everything else matches linux.py.
The package action runs the tools/release pipeline (notices, game archive and
relink companion) against the configured build tree.
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


def run_package(args, preset_root):
    if args.preset != "release-windows" or args.architecture != "x64":
        raise ValueError("package supports the release-windows x64 preset; "
                         "the relink companion and manifest are x64-specific")
    bundle = ROOT / "build" / preset_root / "client/bundle"
    if not bundle.is_dir():
        raise ValueError(f"Build the client bundle first: {bundle}")
    commit = args.source_commit or repo_commit()
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("--source-commit must be a full 40-character Git commit")
    notices = ROOT / "build" / preset_root / "releases/notices-draft"
    output = ROOT / "build" / preset_root / "releases"
    sys.path.insert(0, str(ROOT / "tools/release"))
    import collect_notices
    import package_relink
    import package_windows
    print(f"packaging release from {bundle}")
    notice_args = ["--repo-root", str(ROOT), "--output", str(notices),
                   "--platform", "windows", "--architecture", args.architecture,
                   "--preset", preset_root]
    if args.prior_release:
        notice_args += ["--prior-release", args.prior_release]
    if collect_notices.main(notice_args):
        raise ValueError("Notice collection is incomplete; inspect THIRD_PARTY/inventory.json")
    package_args = ["--bundle", str(bundle), "--repo-root", str(ROOT),
                    "--notices", str(notices), "--output", str(output),
                    "--source-commit", commit]
    if args.name:
        package_args += ["--name", args.name]
    if args.release_notes:
        package_args += ["--release-notes", args.release_notes]
    package_windows.main(package_args)
    relink_name = args.relink_name or (f"{args.name}-relink" if args.name else None)
    relink_args = ["--repo-root", str(ROOT), "--output", str(output),
                   "--source-commit", commit]
    if relink_name:
        relink_args += ["--name", relink_name]
    package_relink.main(relink_args)
    print(f"release packaged: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action", choices=("configure", "build", "run-client", "package", "rhi"),
                        default="build")
    parser.add_argument("--preset", choices=("debug-windows", "release-windows"),
                        default="release-windows")
    parser.add_argument("--architecture", choices=("x64", "arm64"), default=HOST_ARCH)
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 2))
    parser.add_argument("--target", nargs="+", default=["octaryn_all"])
    parser.add_argument("--configure-argument", action="append", default=[])
    parser.add_argument("--client-argument", action="append", default=[])
    parser.add_argument("--name", help="Release archive name (package only)")
    parser.add_argument("--relink-name", help="Relink companion name (package only)")
    parser.add_argument("--source-commit", help="Full Git commit for manifests (package only)")
    parser.add_argument("--release-notes", help="Release notes path for the game archive (package only)")
    parser.add_argument("--prior-release", help="Prior attribution ZIP for notice collection (package only)")
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
    if args.action == "package":
        run_package(args, preset_root)
        return 0
    if args.action == "run-client":
        client = ROOT / "build" / preset_root / "client/bundle/Octaryn.Client.exe"
        if not client.is_file():
            parser.error(f"Build the client bundle first: {client}")
        return subprocess.call([str(client), *args.client_argument], cwd=client.parent)

    vs_root = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs_root, args.architecture)
    vsenv.prepend_tool_dirs(ROOT, vs_root, args.architecture)
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
