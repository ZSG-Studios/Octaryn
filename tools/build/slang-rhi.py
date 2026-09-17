#!/usr/bin/env python3
"""Standalone slang-rhi dependency bootstrap for Linux, macOS and Windows.

Windows uses the same flow with an automatically downloaded Slang SDK (manual
extract to the same directory remains the fallback), static DX12/Vulkan and
DXC fetching. Patches below are the single authoritative registry; dependency
source must match them exactly and contain no other edits.
"""
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
import zipfile

REPO = Path(__file__).resolve().parents[2]
COMMIT = "e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc"
VERSION = "2026.17.1"
PATCHES = (
    "slang-rhi-descriptor-capacity.patch",
    "slang-rhi-multi-draw-capabilities.patch",
    "slang-rhi-d3d12-sampler-cache.patch",
    "slang-rhi-d3d12-draw-capabilities.patch",
)
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


def windows_plan(arch, configuration, sdk_root):
    if arch not in ("x64", "arm64"):
        raise ValueError("Windows x64 and arm64 are supported RHI targets")
    deps = REPO / "build/dependencies"
    # Forward slashes throughout: CMake treats backslash escapes in -D paths.
    sdk = Path(sdk_root or deps / f"slang-{VERSION}").resolve().as_posix()
    source = (deps / "slang-rhi").resolve().as_posix()
    build = (deps / f"slang-rhi-windows-{arch}-{configuration}").as_posix()
    repo = REPO.resolve().as_posix()
    # Bare tool names: build() resolves them to absolute paths after PATH setup,
    # so reconfiguration never reuses a stale cached compiler location.
    options = ["cmake", "-S", source, "-B", build, "-G", "Ninja",
               "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
               f"-DCMAKE_BUILD_TYPE={configuration}",
               f"-DCMAKE_PROJECT_INCLUDE={repo}/cmake/Dependencies/SlangRhiCompiler.cmake",
               "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL",
               "-DSLANG_RHI_BUILD_SHARED=OFF", "-DSLANG_RHI_BUILD_TESTS=OFF",
               "-DSLANG_RHI_BUILD_TESTS_WITH_GLFW=OFF", "-DSLANG_RHI_BUILD_EXAMPLES=OFF",
               "-DSLANG_RHI_INSTALL=OFF", "-DSLANG_RHI_FETCH_SLANG=OFF", "-DSLANG_RHI_FETCH_DXC=ON",
               f"-DSLANG_RHI_SLANG_INCLUDE_DIR={sdk}/include",
               f"-DSLANG_RHI_SLANG_BINARY_DIR={sdk}",
               "-DSLANG_RHI_ENABLE_VULKAN=ON", "-DSLANG_RHI_ENABLE_D3D12=ON"]
    for backend in ("CPU", "D3D11", "AGILITY_SDK", "NVAPI", "METAL", "CUDA", "OPTIX",
                    "WGPU", "AFTERMATH"):
        options.append(f"-DSLANG_RHI_ENABLE_{backend}=OFF")
    if arch == "arm64":
        options += ["-DCMAKE_C_COMPILER_TARGET=aarch64-pc-windows-msvc",
                    "-DCMAKE_CXX_COMPILER_TARGET=aarch64-pc-windows-msvc"]
    return dict(system="windows", arch=arch, configuration=configuration, sdk=sdk,
                source=source, build=build, configure=options)


def build_plan(system, arch, configuration, sdk_root=None):
    if system == "windows":
        return windows_plan(arch, configuration, sdk_root)
    if (system, arch) not in SDK_HASHES:
        raise ValueError("Only native desktop Linux, macOS and Windows are supported")
    deps = REPO / "build/dependencies"
    sdk = Path(sdk_root or deps / f"slang-{VERSION}-{system}-{arch}").resolve()
    source = deps / "slang-rhi"
    build = deps / f"slang-rhi-{system}-{arch}-{configuration}"
    archive_arch = "x86_64" if arch == "x64" else "aarch64"
    name = f"slang-{VERSION}-{system}-{archive_arch}.tar.gz"
    options = ["cmake", "--fresh", "-S", str(source), "-B", str(build), "-G", "Ninja",
               "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
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
    if system == "windows":
        for relative in ["include/slang.h", "bin/slangc.exe", "bin/slang-compiler.dll",
                         "lib/slang-compiler.lib"]:
            if not (root / relative).is_file():
                raise ValueError(f"Incomplete native Slang SDK: {root / relative}")
        return
    suffix = "so" if system == "linux" else "dylib"
    for relative in ["include/slang.h", "bin/slangc"] + [f"lib/lib{name}.{suffix}" for name in
                                                       ("slang-compiler", "slang-rt")]:
        if not (root / relative).is_file():
            raise ValueError(f"Incomplete native Slang SDK: {root / relative}")
    if not any(path.is_file() for path in (root / "lib").glob(f"libslang-glslang*.{suffix}*")):
        raise ValueError(f"Missing Slang SPIR-V optimizer module under {root / 'lib'}")
    if not os.access(root / "bin/slangc", os.X_OK):
        raise ValueError("Slang compiler is not executable")


def acquire_sdk(plan):
    root = Path(plan["sdk"])
    if root.exists():
        validate_sdk(root, plan["system"])
        return
    if plan["system"] == "windows":
        acquire_windows_sdk(root, plan["arch"])
        return
    root.parent.mkdir(parents=True, exist_ok=True)
    downloads = root.parent / "downloads"
    downloads.mkdir(exist_ok=True)
    archive = downloads / plan["url"].rsplit("/", 1)[1]
    if not archive.is_file():
        partial = archive.with_suffix(archive.suffix + ".part")
        with urllib.request.urlopen(plan["url"], timeout=120) as response, partial.open("wb") as output:
            shutil.copyfileobj(response, output)
        partial.rename(archive)
    with archive.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    if digest != plan["sha256"]:
        raise ValueError(f"Slang SDK checksum mismatch; inspect the retained download: {archive}")
    # Extract beside the destination; never replace or clean an existing SDK.
    with tempfile.TemporaryDirectory(prefix="slang-acquire-", dir=root.parent) as temporary:
        temporary = Path(temporary)
        extracted = temporary / "sdk"
        extracted.mkdir()
        with tarfile.open(archive) as package:
            package.extractall(extracted, filter="data")
        validate_sdk(extracted, plan["system"])
        extracted.rename(root)


def acquire_windows_sdk(root, arch):
    """Download the official Slang Windows SDK zip into build/dependencies.

    The extracted tree is verified with validate_sdk before it is moved into
    place. If the download or layout is unusable, the error names the manual
    fallback: extracting the official SDK to the same directory.
    """
    asset_arch = {"x64": "x86_64", "arm64": "aarch64"}[arch]
    name = f"slang-{VERSION}-windows-{asset_arch}.zip"
    url = f"https://github.com/shader-slang/slang/releases/download/v{VERSION}/{name}"
    sys.path.insert(0, str(Path(__file__).resolve().parent / "support"))
    import provision_tools
    downloads = root.parent / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    archive = downloads / name
    try:
        if not archive.is_file():
            provision_tools.download_file(url, archive)
        # Extract beside the destination; never replace or clean an existing SDK.
        with tempfile.TemporaryDirectory(prefix="slang-acquire-", dir=root.parent) as temporary:
            temporary = Path(temporary)
            with zipfile.ZipFile(archive) as package:
                package.extractall(temporary)
            candidate = temporary
            if not (candidate / "include" / "slang.h").is_file():
                subdirs = [entry for entry in temporary.iterdir() if entry.is_dir()]
                if len(subdirs) != 1:
                    raise ValueError(f"Unexpected Slang SDK archive layout: {url}")
                candidate = subdirs[0]
            validate_sdk(candidate, "windows")
            candidate.rename(root)
    except Exception as error:  # noqa: BLE001 - manual fallback carries the detail
        raise ValueError(f"Automatic Slang Windows SDK download failed ({error}); "
                         f"extract the official Slang {VERSION} Windows SDK to {root}")


def patch_checkout(source):
    allowed = set()
    for name in PATCHES:
        patch = REPO / "tools/build/patches" / name
        expected = patch.read_text().replace("\r\n", "\n").rstrip("\n")
        paths = re.findall(r"^diff --git a/(\S+) b/\S+$", expected, re.M)
        if not paths:
            raise ValueError(f"Empty patch: {name}")
        allowed.update(paths)
        actual = run("git", "-C", source, "diff", "--binary", "--no-ext-diff", "HEAD", "--", *paths)
        if not actual:
            # Windows checkouts may use CRLF; git apply consumes patch bytes literally.
            patch_input = expected + "\n"
            for arguments in (("--check", "-"), ("-",)):
                subprocess.run(["git", "-C", str(source), "apply", *arguments],
                               input=patch_input, text=True, check=True)
            actual = run("git", "-C", source, "diff", "--binary", "--no-ext-diff", "HEAD", "--", *paths)
        if actual.replace("\r\n", "\n") != expected:
            raise ValueError(f"Dependency edits differ from exact registered patch: {name}")
    if set(run("git", "-C", source, "diff", "--name-only", "HEAD").splitlines()) - allowed:
        raise ValueError("Unapproved pinned slang-rhi source edits")


def prepare_windows_environment(arch):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import vsenv
    vs_root = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs_root, arch)
    vsenv.prepend_tool_dirs(REPO, vs_root, arch)
    return vsenv


def resolve_windows_configure(vsenv, configure):
    """Substitute absolute tool paths so CMake re-detects the real toolchain."""
    clang = vsenv.resolve_tool("clang-cl")
    resolved = [vsenv.resolve_tool("cmake")]
    for argument in configure[1:]:
        if argument.startswith("-DCMAKE_C_COMPILER="):
            argument = f"-DCMAKE_C_COMPILER={clang}"
        elif argument.startswith("-DCMAKE_CXX_COMPILER="):
            argument = f"-DCMAKE_CXX_COMPILER={clang}"
        resolved.append(argument)
    resolved.append(f"-DCMAKE_MAKE_PROGRAM={vsenv.resolve_tool('ninja')}")
    return resolved


def build(plan, jobs):
    configure = plan["configure"]
    if plan["system"] == "windows":
        if platform.system() != "Windows":
            raise ValueError("Build the Windows RHI natively on Windows")
        helper = prepare_windows_environment(plan["arch"])
        sys.path.insert(0, str(Path(__file__).resolve().parent / "support"))
        import provision_tools
        provision_tools.ensure_pinned_tools(REPO)
        helper.require_tools("cmake", "ninja", "clang-cl", "git")
        configure = resolve_windows_configure(helper, configure)
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
    subprocess.run(configure, check=True)
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
    parser.add_argument("--platform", choices=["linux", "macos", "windows"], help="Override only for --print-plan")
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
