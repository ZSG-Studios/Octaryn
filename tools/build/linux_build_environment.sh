#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
install_mode="ask"
with_ui="1"
with_podman="1"
with_runtime="0"

usage() {
  cat <<'USAGE'
Usage: linux_build_environment.sh [--yes] [--no-ui] [--no-podman] [--runtime]

Installs host requirements for the Podman-first Octaryn workspace. By default
this covers Git, Python, PySide6, Podman, slirp4netns, and fuse-overlayfs.
Use --runtime when the graphical client should run directly on the host; build
tools and cross-toolchains still belong inside the Arch Podman builder image.

Supported package managers: pacman, apt-get, dnf, zypper.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes|-y|--launcher-only)
      install_mode="yes"
      shift
      ;;
    --no-ui)
      with_ui="0"
      shift
      ;;
    --no-podman)
      with_podman="0"
      shift
      ;;
    --runtime)
      with_runtime="1"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      printf '[error] unknown argument: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ "$(uname -s)" != "Linux" ]]; then
  printf '[error] linux_build_environment.sh only supports Linux hosts.\n' >&2
  exit 2
fi

sudo_cmd=()
if [[ "${EUID}" -ne 0 ]]; then
  if ! command -v sudo >/dev/null 2>&1; then
    printf '[error] sudo is required when not running as root.\n' >&2
    exit 1
  fi
  sudo_cmd=(sudo)
fi

log() {
  printf '[setup] %s\n' "$*"
}

run_install() {
  log "$*"
  "${sudo_cmd[@]}" "$@"
}

try_install() {
  log "optional: $*"
  if "${sudo_cmd[@]}" "$@"; then
    return 0
  fi
  printf '[warn] optional install failed: %s\n' "$*" >&2
  return 1
}

append_if_enabled() {
  local -n target="$1"
  local enabled="$2"
  shift 2
  if [[ "${enabled}" == "1" ]]; then
    target+=("$@")
  fi
}

python_cmd() {
  if command -v python3 >/dev/null 2>&1; then
    command -v python3
    return 0
  fi
  if command -v python >/dev/null 2>&1; then
    command -v python
    return 0
  fi
  return 1
}

python_imports_pyside6() {
  local python_path="$1"
  "${python_path}" -c 'from PySide6 import QtCore, QtGui, QtWidgets' >/dev/null 2>&1
}

install_pyside6_with_pip() {
  local python_path="$1"
  if python_imports_pyside6 "${python_path}"; then
    return 0
  fi
  log "installing PySide6 for native Python user site"
  "${python_path}" -m ensurepip --upgrade >/dev/null 2>&1 || true
  if "${python_path}" -m pip install --user --upgrade PySide6; then
    return 0
  fi
  "${python_path}" -m pip install --user --break-system-packages --upgrade PySide6
}

install_arch() {
  local packages=(git python)
  append_if_enabled packages "${with_ui}" pyside6
  append_if_enabled packages "${with_podman}" podman slirp4netns fuse-overlayfs
  append_if_enabled packages "${with_runtime}" dotnet-sdk vulkan-icd-loader vulkan-tools wayland libx11 libxkbcommon libdecor mesa

  local args=(-S --needed)
  [[ "${install_mode}" == "yes" ]] && args+=(--noconfirm)
  run_install pacman "${args[@]}" "${packages[@]}"
}

install_debian() {
  local packages=(git python3 python3-pip python3-venv)
  append_if_enabled packages "${with_podman}" podman slirp4netns fuse-overlayfs
  append_if_enabled packages "${with_runtime}" dotnet-sdk-10.0 libvulkan1 vulkan-tools libwayland-client0 libx11-6 libxkbcommon0 libdecor-0-0 mesa-vulkan-drivers

  run_install apt-get update
  local args=(install)
  [[ "${install_mode}" == "yes" ]] && args+=(-y)
  run_install apt-get "${args[@]}" "${packages[@]}"
  if [[ "${with_ui}" == "1" ]]; then
    try_install apt-get "${args[@]}" python3-pyside6 || true
  fi
}

install_fedora() {
  local packages=(git python3 python3-pip)
  append_if_enabled packages "${with_podman}" podman slirp4netns fuse-overlayfs
  append_if_enabled packages "${with_runtime}" dotnet-sdk-10.0 vulkan-loader vulkan-tools wayland libX11 libxkbcommon libdecor mesa-vulkan-drivers

  local args=(install)
  [[ "${install_mode}" == "yes" ]] && args+=(-y)
  run_install dnf "${args[@]}" "${packages[@]}"
  if [[ "${with_ui}" == "1" ]]; then
    try_install dnf "${args[@]}" python3-pyside6 || true
  fi
}

install_opensuse() {
  local packages=(git python3 python3-pip)
  append_if_enabled packages "${with_podman}" podman slirp4netns fuse-overlayfs
  append_if_enabled packages "${with_runtime}" dotnet-sdk-10.0 libvulkan1 vulkan-tools wayland libX11-6 libxkbcommon0 libdecor-0-0 Mesa-vulkan-device-select

  local args=(install)
  [[ "${install_mode}" == "yes" ]] && args+=(-y)
  run_install zypper "${args[@]}" "${packages[@]}"
  if [[ "${with_ui}" == "1" ]]; then
    try_install zypper "${args[@]}" python3-pyside6 || true
  fi
}

if command -v pacman >/dev/null 2>&1; then
  install_arch
elif command -v apt-get >/dev/null 2>&1; then
  install_debian
elif command -v dnf >/dev/null 2>&1; then
  install_fedora
elif command -v zypper >/dev/null 2>&1; then
  install_opensuse
else
  printf '[error] no supported Linux package manager found. Supported: pacman, apt-get, dnf, zypper.\n' >&2
  exit 1
fi

python_path="$(python_cmd)" || {
  printf '[error] Python is still missing after package installation.\n' >&2
  exit 1
}

if [[ "${with_ui}" == "1" ]] && ! python_imports_pyside6 "${python_path}"; then
  install_pyside6_with_pip "${python_path}"
fi

if [[ "${with_ui}" == "1" ]] && ! python_imports_pyside6 "${python_path}"; then
  printf '[error] PySide6 is still not importable by %s.\n' "${python_path}" >&2
  exit 1
fi

if [[ "${with_podman}" == "1" ]]; then
  command -v podman >/dev/null 2>&1 || {
    printf '[error] Podman is still missing after package installation.\n' >&2
    exit 1
  }
  podman info >/dev/null
fi

if [[ "${with_runtime}" == "1" ]]; then
  command -v dotnet >/dev/null 2>&1 || {
    printf '[error] dotnet is still missing after runtime package installation.\n' >&2
    exit 1
  }
fi

log "host launcher environment ready for ${repo_root}"
