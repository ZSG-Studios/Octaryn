#!/usr/bin/env python3
"""Configure, build and run Octaryn directly on a native Linux host or WSL2.

The package action runs the tools/release pipeline (notices, game archive and
relink companion) against the configured build tree.

When invoked on Windows, Linux actions are delegated automatically to a WSL2
distribution (the default, unless --wsl-distro or OCTARYN_WSL_DISTRO names one)
by re-executing this same script inside the guest. Native Linux behavior is
unchanged: the delegation branch only runs on Windows.
"""
import argparse
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]


def load_slang_rhi():
    import importlib.util
    path = Path(__file__).resolve().parent / "slang-rhi.py"
    spec = importlib.util.spec_from_file_location("slang_rhi_bootstrap", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_rhi(args, arch):
    for tool in ("cmake", "ninja", "clang", "clang++", "git"):
        if not shutil.which(tool):
            raise ValueError(f"Missing required tool: {tool}; see docs/build/README.md")
    configuration = "Debug" if args.preset.startswith("debug") else "Release"
    bootstrap = load_slang_rhi()
    plan = bootstrap.build_plan("linux", arch, configuration)
    bootstrap.build(plan, args.jobs)
    print(f"standalone slang-rhi ready: {plan['build']}")


def repo_commit():
    return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"],
                          check=True, text=True, stdout=subprocess.PIPE).stdout.strip()


def decode_wsl_output(data):
    if b"\x00" in data:
        try:
            return data.decode("utf-16")
        except UnicodeError:
            return data.decode("utf-16-le", errors="replace")
    try:
        return data.decode("utf-8")
    except UnicodeError:
        return data.decode("utf-8", errors="replace")


def find_wsl():
    for candidate in ("wsl", "wsl.exe"):
        path = shutil.which(candidate)
        if path:
            return path
    return None


def list_wsl_distros(wsl):
    try:
        names = subprocess.run([wsl, "--list", "--quiet"], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        verbose = subprocess.run([wsl, "--list", "--verbose"], check=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except (OSError, subprocess.CalledProcessError) as error:
        raise ValueError(f"Could not list WSL2 distributions: {error}")
    distro_names = [line.strip() for line in
                    decode_wsl_output(names.stdout).splitlines() if line.strip()]
    default = None
    for line in decode_wsl_output(verbose.stdout).splitlines():
        stripped = line.strip().strip("\ufeff")
        if not stripped or stripped.upper().startswith("NAME"):
            continue
        is_default = stripped.startswith("*")
        for name in distro_names:
            if stripped.lstrip("*").strip().startswith(name):
                if is_default:
                    default = name
                break
    return distro_names, default


def select_wsl_distro(wsl, preferred):
    distro_names, default = list_wsl_distros(wsl)
    if not distro_names:
        raise ValueError("No WSL2 distributions are installed; install one first")
    if preferred:
        if preferred not in distro_names:
            raise ValueError(f"WSL2 distribution '{preferred}' not found; "
                             f"available: {', '.join(distro_names)}")
        return preferred
    if default is not None:
        return default
    if len(distro_names) == 1:
        return distro_names[0]
    raise ValueError("Multiple WSL2 distributions found "
                     f"({', '.join(distro_names)}); pass --wsl-distro or set "
                     "OCTARYN_WSL_DISTRO")


def wsl_guest_path(wsl, distro, windows_path):
    # Forward slashes: wsl.exe drops backslashes when forwarding arguments
    # after `--` to the guest, so never send it a backslash path.
    text = str(windows_path).replace("\\", "/")
    try:
        completed = subprocess.run(
            [wsl, "-d", distro, "--", "wslpath", "-a", text],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except (OSError, subprocess.CalledProcessError) as error:
        raise ValueError(f"Could not translate '{windows_path}' to a WSL2 path: {error}")
    return decode_wsl_output(completed.stdout).strip()


def forward_arg(token):
    # The same backslash-dropping applies to forwarded arguments, so rewrite
    # Windows drive-absolute paths (including --option=X:\... values) with
    # forward slashes. Other values pass through untouched.
    name, equals, value = token.partition("=")
    candidate = value if equals else token
    if re.match(r"^[A-Za-z]:[\\/]", candidate):
        candidate = candidate.replace("\\", "/")
        return name + equals + candidate if equals else candidate
    return token


def strip_wsl_options(argv):
    forwarded = []
    skip_next = False
    for token in argv:
        if skip_next:
            skip_next = False
            continue
        if token == "--wsl-distro":
            skip_next = True
            continue
        if token.startswith("--wsl-distro="):
            continue
        forwarded.append(token)
    return forwarded


def forward_args(wsl, distro, argv):
    # File-valued options with Windows drive paths are translated to guest
    # paths, in both --option value and --option=value form; everything else
    # goes through the backslash-safe forwarding.
    path_options = ("--prior-release", "--release-notes")
    forwarded = []
    expect_path = False
    skip_next = False
    for token in argv:
        if skip_next:
            skip_next = False
            continue
        if token == "--wsl-distro":
            skip_next = True
            continue
        if token.startswith("--wsl-distro="):
            continue
        if expect_path:
            expect_path = False
            if re.match(r"^[A-Za-z]:[\\/]", token):
                forwarded.append(wsl_guest_path(wsl, distro, token))
            else:
                forwarded.append(forward_arg(token))
            continue
        name, equals, value = token.partition("=")
        if equals and name in path_options and re.match(r"^[A-Za-z]:[\\/]", value):
            forwarded.append(name + equals + wsl_guest_path(wsl, distro, value))
            continue
        if not equals and token in path_options:
            forwarded.append(token)
            expect_path = True
            continue
        forwarded.append(forward_arg(token))
    return forwarded


def run_through_wsl(args, argv):
    wsl = find_wsl()
    if wsl is None:
        raise ValueError("Linux builds need WSL2 on Windows, but no 'wsl' "
                         "executable was found; install WSL2 first")
    distro = select_wsl_distro(wsl, args.wsl_distro or os.environ.get("OCTARYN_WSL_DISTRO"))
    guest_root = wsl_guest_path(wsl, distro, ROOT)
    guest_script = guest_root.rstrip("/") + "/tools/build/linux.py"
    command = [wsl, "-d", distro, "--"]
    forwarded_env = os.environ.get("OCTARYN_CLIENT_GRAPHICS_API")
    if forwarded_env:
        command += ["env", f"OCTARYN_CLIENT_GRAPHICS_API={forwarded_env}"]
    command += ["python3", guest_script, *forward_args(wsl, distro, argv)]
    print(f"delegating Linux {args.preset} {args.action} to WSL2 '{distro}': {guest_root}")
    return subprocess.call(command)


def run_package(args, preset_root, arch):
    if not args.preset.startswith("release-"):
        raise ValueError("package requires a release preset")
    if not args.name:
        raise ValueError("package requires --name")
    bundle = ROOT / "build" / preset_root / "client/bundle"
    if not bundle.is_dir():
        raise ValueError(f"Build the client bundle first: {bundle}")
    commit = args.source_commit or repo_commit()
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("--source-commit must be a full 40-character Git commit")
    notices = ROOT / "build" / preset_root / "releases/notices"
    output = ROOT / "build" / preset_root / "releases/packages"
    relink_name = args.relink_name or f"{args.name}-relink"
    sys.path.insert(0, str(ROOT / "tools/release"))
    import collect_notices
    import package_linux
    import package_relink_linux
    print(f"packaging release from {bundle}")
    notice_args = ["--repo-root", str(ROOT), "--output", str(notices),
                   "--platform", "linux", "--architecture", arch,
                   "--preset", preset_root]
    if args.prior_release:
        notice_args += ["--prior-release", args.prior_release]
    if collect_notices.main(notice_args):
        raise ValueError("Notice collection is incomplete; inspect THIRD_PARTY/inventory.json")
    package_linux.main(["--repo-root", str(ROOT), "--bundle", str(bundle),
                        "--notices", str(notices), "--release-notes", args.release_notes,
                        "--output", str(output), "--source-commit", commit,
                        "--architecture", arch, "--name", args.name])
    package_relink_linux.main(["--repo-root", str(ROOT), "--preset", preset_root,
                               "--architecture", arch, "--notices", str(notices),
                               "--output", str(output), "--source-commit", commit,
                               "--name", relink_name])
    print(f"release packaged: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action", choices=("configure", "build", "run-client", "package", "rhi"),
                        default="build")
    parser.add_argument("--preset", choices=("debug-linux", "release-linux"), default="release-linux")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 2))
    parser.add_argument("--target", nargs="+", default=["octaryn_all"])
    parser.add_argument("--configure-argument", action="append", default=[])
    parser.add_argument("--client-argument", action="append", default=[])
    parser.add_argument("--name", help="Release archive name (package only)")
    parser.add_argument("--relink-name", help="Relink companion name, defaults to <name>-relink (package only)")
    parser.add_argument("--source-commit", help="Full Git commit for manifests (package only)")
    parser.add_argument("--release-notes", default="docs/releases/2026-09-14-slang-rhi-preview.md",
                        help="Release notes path for the game archive (package only)")
    parser.add_argument("--prior-release", help="Prior attribution ZIP for notice collection (package only)")
    parser.add_argument("--wsl-distro", default=os.environ.get("OCTARYN_WSL_DISTRO"),
                        help="WSL2 distribution for Windows-side delegation (Windows only)")
    args = parser.parse_args()
    if platform.system() == "Windows":
        return run_through_wsl(args, sys.argv[1:])
    if platform.system() != "Linux":
        parser.error("Run this command inside Linux/WSL2; Windows builds use tools/build/windows.py")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    arch = {"x86_64": "x64", "aarch64": "arm64"}.get(platform.machine().lower())
    if arch is None:
        parser.error("Native Linux x64 and arm64 are supported build targets")
    preset_root = args.preset + ("-arm64" if arch == "arm64" else "")
    build = ROOT / "build" / preset_root / "cmake"
    if args.action == "rhi":
        run_rhi(args, arch)
        return 0
    if args.action == "package":
        run_package(args, preset_root, arch)
        return 0
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
    try:
        sys.exit(main())
    except (ValueError, RuntimeError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
