#!/usr/bin/env python3
"""Native Linux/Metal dependency bootstrap. Windows retains slang-rhi.ps1."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

REPO = Path(__file__).resolve().parents[2]
COMMIT = "e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc"
VERSION = "2026.17.1"
# Upstream release API digests, pinned with the version rather than fetched at build time.
SDK_HASHES = {
    ("linux", "x64"): "31d7e53377dd9a80a1b7b9ec86f800562db91ca4ed2fb4c77573cba4d3e5095d",
    ("linux", "arm64"): "9fde649a04957d854292f315a77fea44365943af6fc0854ab6a01a72cbc24cd7",
    ("macos", "x64"): "2b702d8a0e6571cee32f3d67db0e4eb7847ac99ecb819e0c22592b4df1941f47",
    ("macos", "arm64"): "ab82adcbeb19ec3246fc3746499f64608fb8e640cbc9d6a55628b9c972abee2c",
}


def run(*args):
    return subprocess.run([str(a) for a in args], check=True, text=True,
                          stdout=subprocess.PIPE).stdout.strip()


def native_platform():
    system = {"Linux": "linux", "Darwin": "macos", "Windows": "windows"}.get(platform.system())
    arch = {"amd64": "x64", "x86_64": "x64", "arm64": "arm64", "aarch64": "arm64"}.get(platform.machine().lower())
    if not system or not arch:
        raise ValueError("Only native desktop x64 and arm64 are supported")
    return system, arch


def build_plan(system, arch, configuration, sdk_root=None):
    if (system, arch) not in SDK_HASHES:
        raise ValueError("Use tools/build/slang-rhi.ps1 for Windows")
    deps = REPO / "build/dependencies"
    sdk = Path(sdk_root or deps / f"slang-{VERSION}-{system}-{arch}").resolve()
    source = deps / "slang-rhi"
    build = deps / f"slang-rhi-{system}-{arch}-{configuration}"
    archive_arch = "x86_64" if arch == "x64" else "aarch64"
    name = f"slang-{VERSION}-{system}-{archive_arch}.tar.gz"
    options = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
               f"-DCMAKE_BUILD_TYPE={configuration}", "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
               f"-DSLANG_RHI_SLANG_INCLUDE_DIR={sdk}/include", f"-DSLANG_RHI_SLANG_BINARY_DIR={sdk}"]
    disabled = ["BUILD_SHARED", "BUILD_TESTS", "BUILD_TESTS_WITH_GLFW", "BUILD_EXAMPLES", "INSTALL",
                "FETCH_SLANG", "FETCH_DXC", "ENABLE_CPU", "ENABLE_D3D11", "ENABLE_D3D12",
                "ENABLE_AGILITY_SDK", "ENABLE_NVAPI", "ENABLE_CUDA", "ENABLE_OPTIX", "ENABLE_WGPU", "ENABLE_AFTERMATH"]
    options += [f"-DSLANG_RHI_{option}=OFF" for option in disabled]
    options += [f"-DSLANG_RHI_ENABLE_VULKAN={'ON' if system == 'linux' else 'OFF'}",
                f"-DSLANG_RHI_ENABLE_METAL={'ON' if system == 'macos' else 'OFF'}"]
    if system == "macos":
        options += [f"-DCMAKE_OSX_ARCHITECTURES={'x86_64' if arch == 'x64' else 'arm64'}"]
    return dict(system=system, arch=arch, configuration=configuration, sdk=str(sdk), source=str(source),
                build=str(build), url=f"https://github.com/shader-slang/slang/releases/download/v{VERSION}/{name}",
                sha256=SDK_HASHES[system, arch], configure=options)


def validate_sdk(root, system):
    suffix = "so" if system == "linux" else "dylib"
    for relative in ["include/slang.h", "bin/slangc"] + [f"lib/lib{name}.{suffix}" for name in
                                                       ("slang-compiler", "slang-rt", "slang-glslang")]:
        if not (root / relative).is_file():
            raise ValueError(f"Incomplete native Slang SDK: {root / relative}")
    if not os.access(root / "bin/slangc", os.X_OK):
        raise ValueError("Slang compiler is not executable")


def acquire_sdk(plan):
    root = Path(plan["sdk"])
    if root.exists():
        validate_sdk(root, plan["system"])
        return
    root.parent.mkdir(parents=True, exist_ok=True)
    # Extract beside the destination; never replace or clean an existing SDK.
    with tempfile.TemporaryDirectory(prefix="slang-acquire-", dir=root.parent) as temporary:
        temporary = Path(temporary)
        archive = temporary / "sdk.tar.gz"
        with urllib.request.urlopen(plan["url"], timeout=120) as response, archive.open("wb") as output:
            shutil.copyfileobj(response, output)
        with archive.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        if digest != plan["sha256"]:
            raise ValueError("Slang SDK checksum mismatch; SDK was not installed")
        extracted = temporary / "sdk"
        extracted.mkdir()
        with tarfile.open(archive) as package:
            package.extractall(extracted, filter="data")
        validate_sdk(extracted, plan["system"])
        extracted.rename(root)


def patch_checkout(source):
    # One authoritative patch list for Windows and Unix; include later registered patches.
    registry = (REPO / "tools/build/apply-slang-rhi-patch.ps1").read_text()
    match = re.search(r"\$patches\s*=\s*@\((.*?)\)", registry, re.S)
    names = re.findall(r"'([^']+\.patch)'", match[1]) if match else []
    if not names:
        raise ValueError("Pinned patch registry is empty or unsupported")
    allowed = set()
    for name in names:
        patch = REPO / "tools/build/patches" / name
        expected = patch.read_text().replace("\r\n", "\n").rstrip("\n")
        paths = re.findall(r"^diff --git a/(\S+) b/\S+$", expected, re.M)
        if not paths:
            raise ValueError(f"Empty patch: {name}")
        allowed.update(paths)
        actual = run("git", "-C", source, "diff", "--binary", "--no-ext-diff", "HEAD", "--", *paths)
        if not actual:
            run("git", "-C", source, "apply", "--check", patch)
            run("git", "-C", source, "apply", patch)
            actual = run("git", "-C", source, "diff", "--binary", "--no-ext-diff", "HEAD", "--", *paths)
        if actual.replace("\r\n", "\n") != expected:
            raise ValueError(f"Dependency edits differ from exact registered patch: {name}")
    if set(run("git", "-C", source, "diff", "--name-only", "HEAD").splitlines()) - allowed:
        raise ValueError("Unapproved pinned slang-rhi source edits")


def build(plan, jobs):
    acquire_sdk(plan)
    source = Path(plan["source"])
    if not source.exists():
        run("git", "init", source)
        run("git", "-C", source, "fetch", "--depth", "1", "https://github.com/shader-slang/slang-rhi.git", COMMIT)
        run("git", "-C", source, "checkout", "--detach", "FETCH_HEAD")
    if run("git", "-C", source, "rev-parse", "HEAD") != COMMIT:
        raise ValueError("Existing slang-rhi checkout has a different pin; it was not changed")
    patch_checkout(source)
    # Subprocess output remains visible for configure/build failures and progress.
    subprocess.run(plan["configure"], check=True)
    subprocess.run(["cmake", "--build", plan["build"], "--target", "slang-rhi", "--parallel", str(jobs)], check=True)
    receipt = {"COMMIT": COMMIT, "SDK_ROOT": plan["sdk"], "ARCH": plan["arch"],
               "CONFIG": plan["configuration"], "PLATFORM": plan["system"]}
    def quote(value):
        return str(value).replace("\\", "/").replace('"', '\\"').replace("$", "\\$").replace(";", "\\;")
    text = "".join(f'set(OCTARYN_SLANG_RHI_BUILT_{key} "{quote(value)}")\n' for key, value in receipt.items())
    (Path(plan["build"]) / "octaryn-dependency.cmake").write_text(text)
    print(f"Standalone slang-rhi native dependency built: {plan['build']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], default="Release")
    parser.add_argument("--sdk-root", type=Path)
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--print-plan", action="store_true", help="No downloads, changes, configure or build")
    parser.add_argument("--platform", choices=["linux", "macos"], help="Override only for --print-plan")
    parser.add_argument("--architecture", choices=["x64", "arm64"], help="Override only for --print-plan")
    args = parser.parse_args()
    native, arch = native_platform()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if not args.print_plan and (args.platform or args.architecture):
        parser.error("Cross compilation is not supported; platform overrides are for --print-plan only")
    plan = build_plan(args.platform or native, args.architecture or arch, args.configuration, args.sdk_root)
    if args.print_plan:
        print(json.dumps(plan, indent=2))
    else:
        build(plan, args.jobs)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
