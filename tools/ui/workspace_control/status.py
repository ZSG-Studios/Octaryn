from __future__ import annotations

import os
import platform
import sys
from pathlib import Path

from PySide6 import QtCore

from .paths import PODMAN_BUILD_SCRIPT, resolve_run_path

PRESET_DETAILS = {
    "debug-linux": "Linux debug build from the active Clang preset.",
    "release-linux": "Linux release build from the active Clang preset.",
    "debug-windows": "Windows debug cross-build from Linux through the Windows Clang toolchain.",
    "release-windows": "Windows release cross-build from Linux through the Windows Clang toolchain.",
}

LINUX_DEBUG_PRESET = "debug-linux"
LINUX_RELEASE_PRESET = "release-linux"
WINDOWS_DEBUG_PRESET = "debug-windows"
WINDOWS_RELEASE_PRESET = "release-windows"
PRESET_LABELS = {
    LINUX_DEBUG_PRESET: "Linux Debug",
    LINUX_RELEASE_PRESET: "Linux Release",
    WINDOWS_DEBUG_PRESET: "Windows Debug",
    WINDOWS_RELEASE_PRESET: "Windows Release",
}
BUILD_PRESETS = (
    LINUX_DEBUG_PRESET,
    LINUX_RELEASE_PRESET,
    WINDOWS_DEBUG_PRESET,
    WINDOWS_RELEASE_PRESET,
)
CROSS_COMPILE_PRESETS = {
    WINDOWS_DEBUG_PRESET,
    WINDOWS_RELEASE_PRESET,
}

ACTIVE_PRODUCT = "octaryn-workspace"
ACTIVE_PRODUCT_LABEL = "Octaryn workspace owners"


def preset_summary(preset: str) -> str:
    return PRESET_DETAILS.get(preset, "No preset summary available.")


def host_platform() -> str:
    if sys.platform.startswith("linux"):
        return "linux"
    return "unknown"


def host_arch() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x64"
    if machine in {"aarch64", "arm64"}:
        return "arm64"
    return machine or "unknown"


def preset_target_platform(preset: str) -> str:
    if preset.endswith("-windows"):
        return "windows"
    if preset.endswith("-linux"):
        return "linux"
    return "unknown"


def host_status_summary() -> str:
    return f"Host: {host_platform()}/{host_arch()}"


def podman_build_environment_summary() -> str:
    podman = QtCore.QStandardPaths.findExecutable("podman")
    image = os.environ.get(
        "OCTARYN_PODMAN_BUILD_IMAGE",
        "localhost/octaryn-arch-builder:latest",
    )
    if not PODMAN_BUILD_SCRIPT.exists():
        return "Podman build env: missing wrapper"
    if not podman:
        return f"Podman build env: missing podman ({image})"
    return f"Podman build env: ready ({image})"


def native_run_state_summary(preset: str, arch: str) -> tuple[bool, str]:
    target_platform = preset_target_platform(preset)
    current_platform = host_platform()
    if target_platform != current_platform:
        return (
            False,
            f"Native run: blocked (target {target_platform}, host {current_platform})",
        )
    current_arch = host_arch()
    if arch != current_arch:
        return False, f"Native run: blocked (target {arch}, host {current_arch})"
    run_path = resolve_run_path(preset, arch)
    if run_path is None:
        return False, "Native run: missing client probe"
    return True, f"Native run: ready ({run_path.name})"


def tool_exists_from_root(env_var: str, relative_path: str, fallback: Path) -> bool:
    root = os.environ.get(env_var)
    if root and (Path(root) / relative_path).is_file():
        return True
    return fallback.is_file()


def missing_cross_toolchains(arch: str = "x64") -> list[str]:
    missing: list[str] = []
    windows_tool = (
        "bin/aarch64-w64-mingw32-clang"
        if arch == "arm64"
        else "bin/x86_64-w64-mingw32-clang"
    )
    windows_fallback = (
        Path("/opt/llvm-mingw/bin/aarch64-w64-mingw32-clang")
        if arch == "arm64"
        else Path("/opt/llvm-mingw/bin/x86_64-w64-mingw32-clang")
    )
    if not tool_exists_from_root(
        "OCTARYN_WINDOWS_CLANG_ROOT",
        windows_tool,
        windows_fallback,
    ):
        missing.append(f"Windows Clang {arch}")
    return missing
