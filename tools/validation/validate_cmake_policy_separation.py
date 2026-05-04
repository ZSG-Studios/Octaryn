#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys


OWNER_FORBIDDEN = (
    r"\bCMAKE_SYSTEM_NAME\b",
    r"\bCMAKE_HOST_SYSTEM_NAME\b",
    r"\bCMAKE_SYSTEM_PROCESSOR\b",
    r"\bfind_program\s*\(",
    r"/etc/os-release",
    r"include\s*\(\s*Platforms/",
)

TOOLCHAIN_FORBIDDEN = (
    r"\badd_library\s*\(",
    r"\badd_executable\s*\(",
    r"\badd_custom_target\s*\(",
    r"\bfind_package\s*\(",
    r"\bFetchContent",
    r"\btarget_[A-Za-z_]+\s*\(",
    r"include\s*\(\s*Owners/",
    r"include\s*\(\s*Dependencies/",
    r"\b[a-z0-9_-]*gcc\b",
    r"\b[a-z0-9_-]*g\+\+\b",
)

PLATFORM_FORBIDDEN = (
    r"\badd_library\s*\(",
    r"\badd_executable\s*\(",
    r"\badd_custom_target\s*\(",
    r"\btarget_[A-Za-z_]+\s*\(",
    r"include\s*\(\s*Owners/",
)

REQUIRED_TOOLCHAIN_SNIPPETS = {
    "cmake/Toolchains/Linux/clang.cmake": ("set(CMAKE_SYSTEM_NAME Linux)",),
    "cmake/Toolchains/Windows/clang.cmake": (
        "set(CMAKE_SYSTEM_NAME Windows)",
        "OCTARYN_WINDOWS_CLANG_TRIPLE",
        "aarch64-w64-mingw32",
        "CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY",
    ),
}

REQUIRED_PLATFORM_SNIPPETS = (
    "include(Platforms/Linux/LinuxPlatform)",
    "include(Platforms/Windows/WindowsPlatform)",
)

REQUIRED_OWNER_SNIPPETS = {
    "cmake/Owners/SharedTargets.cmake": (
        "target_compile_definitions(octaryn_shared_host_abi",
        "OCTARYN_ABI_BUILD",
    ),
}

REQUIRED_PLATFORM_FILE_SNIPPETS = {
    "cmake/Platforms/Linux/ArchFamily.cmake": (
        "OCTARYN_TARGET_DOTNET_RID",
        "linux-x64",
        "linux-arm64",
    ),
    "cmake/Platforms/Windows/WindowsPlatform.cmake": (
        "OCTARYN_TARGET_NATIVE_ARCHIVE_FORMAT",
        "OCTARYN_TARGET_DOTNET_RID",
        "OCTARYN_TARGET_OBJDUMP",
    ),
    "cmake/Platforms/Linux/LinuxPlatform.cmake": (
        "OCTARYN_TARGET_NATIVE_ARCHIVE_FORMAT",
        "OCTARYN_TARGET_OBJDUMP",
    ),
}

REQUIRED_DEPENDENCY_SNIPPETS = {
    "cmake/Dependencies/SourceDependencyCache.cmake": (
        "OCTARYN_DEPENDENCY_BUILD_ROOT",
        "FETCHCONTENT_BASE_DIR",
        "CPM_MODULE_PATH",
        "octaryn_fetch_source_dependency",
    ),
    "cmake/Dependencies/DotNetHosting.cmake": (
        "Microsoft.NETCore.App.Host.${OCTARYN_TARGET_DOTNET_RID}",
        "runtimes/${OCTARYN_TARGET_DOTNET_RID}/native",
        "OCTARYN_DOTNET_HOSTFXR_LIBRARY_GLOB",
    ),
}


def check_forbidden(errors, path, text, patterns, label):
    for pattern in patterns:
        if re.search(pattern, text):
            errors.append(f"{path}: {label} contains forbidden CMake policy pattern {pattern}")


def validate(repo_root):
    errors = []
    owners_dir = repo_root / "cmake" / "Owners"
    toolchains_dir = repo_root / "cmake" / "Toolchains"

    for path in sorted(owners_dir.rglob("*.cmake")):
        check_forbidden(errors, path, path.read_text(encoding="utf-8"), OWNER_FORBIDDEN, "owner module")

    for relative_path, snippets in REQUIRED_OWNER_SNIPPETS.items():
        path = repo_root / relative_path
        if not path.exists():
            errors.append(f"{path}: missing required owner module")
            continue

        text = path.read_text(encoding="utf-8")
        for snippet in snippets:
            if snippet not in text:
                errors.append(f"{path}: missing required owner snippet {snippet}")

    for relative_path, snippets in REQUIRED_PLATFORM_FILE_SNIPPETS.items():
        path = repo_root / relative_path
        if not path.exists():
            errors.append(f"{path}: missing required platform module")
            continue

        text = path.read_text(encoding="utf-8")
        for snippet in snippets:
            if snippet not in text:
                errors.append(f"{path}: missing required platform snippet {snippet}")

    platforms_dir = repo_root / "cmake" / "Platforms"
    for path in sorted(platforms_dir.rglob("*.cmake")):
        check_forbidden(errors, path, path.read_text(encoding="utf-8"), PLATFORM_FORBIDDEN, "platform module")

    for relative_path, snippets in REQUIRED_DEPENDENCY_SNIPPETS.items():
        path = repo_root / relative_path
        if not path.exists():
            errors.append(f"{path}: missing required dependency module")
            continue

        text = path.read_text(encoding="utf-8")
        for snippet in snippets:
            if snippet not in text:
                errors.append(f"{path}: missing required dependency snippet {snippet}")

    for path in sorted(toolchains_dir.rglob("*.cmake")):
        check_forbidden(errors, path, path.read_text(encoding="utf-8"), TOOLCHAIN_FORBIDDEN, "toolchain file")

    for relative_path, snippets in REQUIRED_TOOLCHAIN_SNIPPETS.items():
        path = repo_root / relative_path
        if not path.exists():
            errors.append(f"{path}: missing required toolchain file")
            continue

        text = path.read_text(encoding="utf-8")
        for snippet in snippets:
            if snippet not in text:
                errors.append(f"{path}: missing required toolchain snippet {snippet}")

    platform_dispatch = repo_root / "cmake" / "Platforms" / "PlatformDispatch.cmake"
    if not platform_dispatch.exists():
        errors.append(f"{platform_dispatch}: missing platform dispatch file")
    else:
        text = platform_dispatch.read_text(encoding="utf-8")
        for snippet in REQUIRED_PLATFORM_SNIPPETS:
            if snippet not in text:
                errors.append(f"{platform_dispatch}: missing platform dispatch snippet {snippet}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    args = parser.parse_args()

    errors = validate(pathlib.Path(args.repo_root))
    if errors:
        for error in errors:
            print(f"cmake policy separation: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
