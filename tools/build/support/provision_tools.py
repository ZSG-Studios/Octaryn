"""Automatic provisioning of pinned CMake/Ninja tool binaries on Windows.

The Windows entrypoints expect repo-pinned tools under
``build/dependencies/tools`` (see vsenv.prepend_tool_dirs), but nothing
populated those directories: a fresh machine failed with
``Missing required tool: cmake``. This module downloads the official upstream
release archives for the pinned versions, verifies them by running
``--version``, and lays them out exactly where the PATH setup expects them:

- ``build/dependencies/tools/cmake/bin/cmake.exe`` (+ share/, doc/, man/)
- ``build/dependencies/tools/ninja/ninja.exe``

System prerequisites stay manual and documented (Visual Studio C++ tools,
.NET SDK, Git, Python): only these fetchable version-pinned archives are
automatic. Re-runs are no-ops via ``.provisioned`` stamp files. Set
``OCTARYN_NO_TOOL_PROVISION=1`` on locked-down machines to skip provisioning
and fall back to the explicit missing-tool error.
"""
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile

CMAKE_VERSION = "3.30.5"
NINJA_VERSION = "1.12.1"

_CMAKE_ASSET = {
    "x64": "cmake-{version}-windows-x86_64.zip",
    "arm64": "cmake-{version}-windows-arm64.zip",
}
_NINJA_ASSET = {
    "x64": "ninja-win.zip",
    "arm64": "ninja-winarm64.zip",
}


def _host_arch():
    arch = {"amd64": "x64", "x86_64": "x64", "arm64": "arm64"}.get(platform.machine().lower())
    if not arch:
        raise ValueError("Automatic tool provisioning supports Windows x64 and arm64")
    return arch


def _cmake_url(arch):
    asset = _CMAKE_ASSET[arch].format(version=CMAKE_VERSION)
    return f"https://github.com/Kitware/CMake/releases/download/v{CMAKE_VERSION}/{asset}"


def _ninja_url(arch):
    return f"https://github.com/ninja-build/ninja/releases/download/v{NINJA_VERSION}/{_NINJA_ASSET[arch]}"


def download_file(url, destination, attempts=3):
    """Download a file with visible progress; retryable via .part files."""
    partial = destination.with_suffix(destination.suffix + ".part")
    last_error = None
    for attempt in range(1, attempts + 1):
        try:
            print(f"provision download attempt={attempt} url={url}")
            sys.stdout.flush()
            request = urllib.request.Request(url, headers={"User-Agent": "OctarynToolProvision/1.0"})
            with urllib.request.urlopen(request, timeout=180) as response, partial.open("wb") as output:
                total = int(response.headers.get("Content-Length") or 0)
                received = 0
                started = time.monotonic()
                while True:
                    chunk = response.read(1024 * 256)
                    if not chunk:
                        break
                    output.write(chunk)
                    received += len(chunk)
                    if total:
                        print(f"\rprovision progress received_mb={received / 1048576:.1f} "
                              f"total_mb={total / 1048576:.1f} "
                              f"percent={100.0 * received / total:.0f}",
                              end="", flush=True)
                elapsed = max(time.monotonic() - started, 0.001)
                print(f"\rprovision progress received_mb={received / 1048576:.1f} "
                      f"elapsed_s={elapsed:.0f} rate_mbps={received / 1048576 / elapsed:.1f}")
            if partial.stat().st_size == 0:
                raise ValueError("Empty download")
            partial.rename(destination)
            return
        except Exception as error:  # noqa: BLE001 - retried, then reported
            last_error = error
            print(f"provision download failed attempt={attempt} error={error}")
            try:
                partial.unlink()
            except OSError:
                pass
    raise ValueError(f"Tool download failed after {attempts} attempts: {url} ({last_error})")


def _run_version(binary):
    try:
        output = subprocess.run([str(binary), "--version"], check=True, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        raise ValueError(f"Provisioned tool does not run: {binary} ({error})")
    return output.splitlines()[0] if output else ""


def _provision_cmake(tools_root, downloads, arch):
    target = tools_root / "cmake"
    binary = target / "bin" / "cmake.exe"
    stamp = target / ".provisioned"
    if binary.is_file() and stamp.is_file() and stamp.read_text().strip() == CMAKE_VERSION:
        first_line = _run_version(binary)
        if CMAKE_VERSION in first_line:
            return True
    url = _cmake_url(arch)
    archive = downloads / url.rsplit("/", 1)[1]
    if not archive.is_file():
        download_file(url, archive)
    with tempfile.TemporaryDirectory(prefix="cmake-provision-", dir=tools_root) as temporary:
        temporary = Path(temporary)
        with zipfile.ZipFile(archive) as package:
            package.extractall(temporary)
        roots = [entry for entry in temporary.iterdir() if entry.is_dir()]
        if len(roots) != 1 or not (roots[0] / "bin" / "cmake.exe").is_file():
            raise ValueError(f"Unexpected CMake archive layout: {url}")
        first_line = _run_version(roots[0] / "bin" / "cmake.exe")
        if CMAKE_VERSION not in first_line:
            raise ValueError(f"CMake archive is not version {CMAKE_VERSION}: {first_line} ({url})")
        if target.exists():
            shutil.rmtree(target)
        roots[0].rename(target)
    stamp.write_text(CMAKE_VERSION + "\n")
    print(f"provision ready tool=cmake version={CMAKE_VERSION} path={binary}")
    return True


def _provision_ninja(tools_root, downloads, arch):
    target = tools_root / "ninja"
    binary = target / "ninja.exe"
    stamp = target / ".provisioned"
    if binary.is_file() and stamp.is_file() and stamp.read_text().strip() == NINJA_VERSION:
        if NINJA_VERSION in _run_version(binary):
            return True
    url = _ninja_url(arch)
    archive = downloads / url.rsplit("/", 1)[1]
    if not archive.is_file():
        download_file(url, archive)
    with tempfile.TemporaryDirectory(prefix="ninja-provision-", dir=tools_root) as temporary:
        temporary = Path(temporary)
        with zipfile.ZipFile(archive) as package:
            package.extractall(temporary)
        candidate = temporary / "ninja.exe"
        if not candidate.is_file():
            raise ValueError(f"Unexpected Ninja archive layout: {url}")
        if NINJA_VERSION not in _run_version(candidate):
            raise ValueError(f"Ninja archive is not version {NINJA_VERSION} ({url})")
        if target.exists():
            shutil.rmtree(target)
        target.mkdir(parents=True)
        candidate.rename(binary)
    stamp.write_text(NINJA_VERSION + "\n")
    print(f"provision ready tool=ninja version={NINJA_VERSION} path={binary}")
    return True


def ensure_pinned_tools(repo_root):
    """Provision pinned Windows tool binaries. Returns True when handled.

    Returns False on non-Windows platforms or when OCTARYN_NO_TOOL_PROVISION
    is set, so callers keep their existing require_tools behavior there.
    """
    if platform.system() != "Windows":
        return False
    if os.environ.get("OCTARYN_NO_TOOL_PROVISION"):
        print("provision skipped tool=cmake,ninja reason=OCTARYN_NO_TOOL_PROVISION")
        return False
    repo_root = Path(repo_root)
    tools_root = repo_root / "build/dependencies/tools"
    tools_root.mkdir(parents=True, exist_ok=True)
    downloads = repo_root / "build/dependencies/downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    arch = _host_arch()
    _provision_cmake(tools_root, downloads, arch)
    _provision_ninja(tools_root, downloads, arch)
    return True
