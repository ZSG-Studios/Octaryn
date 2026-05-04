from __future__ import annotations

import os
from pathlib import Path


def find_workspace_root() -> Path:
    env_root = os.environ.get("OCTARYN_WORKSPACE_ROOT")
    if env_root:
        root = Path(env_root).resolve()
        if (root / "CMakePresets.json").is_file() and (root / "tools" / "build").is_dir():
            return root

    for candidate in Path(__file__).resolve().parents:
        if (candidate / "CMakePresets.json").is_file() and (candidate / "tools" / "build").is_dir():
            return candidate

    return Path(__file__).resolve().parents[3]


WORKSPACE_ROOT = find_workspace_root()
PODMAN_BUILD_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "podman_build.sh"
TRACY_TOOL_SCRIPT = WORKSPACE_ROOT / "tools" / "profiling" / "tracy_tool.sh"
BOOTSTRAP_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "workspace_bootstrap.sh"
HOST_SETUP_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "linux_build_environment.sh"
LOG_ROOT = WORKSPACE_ROOT / "logs" / "tools"
WARNINGS_LOG = LOG_ROOT / "workspace_control_warnings.log"
ERRORS_LOG = LOG_ROOT / "workspace_control_errors.log"
STATE_PATH = LOG_ROOT / "workspace_control_status.json"
SETTINGS_PATH = LOG_ROOT / "workspace_control_settings.json"
DASHBOARD_LOG = LOG_ROOT / "workspace_control_dashboard.log"

CLIENT_RUN_TARGET = "octaryn_client_launch_probe"


def product_build_dir(preset: str, arch: str = "x64") -> Path:
    if arch == "arm64":
        return WORKSPACE_ROOT / "build" / f"{preset}-arm64"
    return WORKSPACE_ROOT / "build" / preset


def resolve_run_path(preset: str, arch: str = "x64") -> Path | None:
    build_dir = product_build_dir(preset, arch)
    candidates = [
        build_dir / "client" / "native" / "bin" / CLIENT_RUN_TARGET,
        build_dir / "client" / "native" / "bin" / f"{CLIENT_RUN_TARGET}.exe",
    ]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None
