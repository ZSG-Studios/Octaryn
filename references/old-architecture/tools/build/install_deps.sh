#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

install_mode="ask"
with_ui="1"
with_windows="1"
dotnet_channel="10.0"

usage() {
  cat <<'USAGE'
Usage: install_deps.sh [--yes] [--no-ui] [--no-windows]

Installs system packages needed to configure and build Octaryn on a fresh Linux
machine. Third-party source dependencies are fetched by CMake/CPM during
configure; this script only handles OS packages. Supported package managers:
pacman, apt-get, dnf, zypper.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes|-y)
      install_mode="yes"
      shift
      ;;
    --no-ui)
      with_ui="0"
      shift
      ;;
    --no-windows)
      with_windows="0"
      shift
      ;;
    -h|--help)
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
  printf '[error] install_deps.sh only supports Linux hosts.\n' >&2
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

run_install() {
  printf '[deps] %s\n' "$*"
  "${sudo_cmd[@]}" "$@"
}

try_install() {
  printf '[deps] optional: %s\n' "$*"
  if "${sudo_cmd[@]}" "$@"; then
    return 0
  fi
  printf '[warn] optional install failed; continuing with repo-local fallback where possible.\n' >&2
  return 1
}

append_if_enabled() {
  local -n target="$1"
  local enabled="$2"
  shift 2
  if [[ "$enabled" == "1" ]]; then
    target+=("$@")
  fi
}

dotnet_sdk_10_available() {
  command -v dotnet >/dev/null 2>&1 && dotnet --list-sdks | grep -q '^10\.'
}

write_toolchain_env() {
  local dotnet_root="$1"
  local env_path
  env_path="$(octaryn_toolchain_env_path)"
  mkdir -p "$(dirname "$env_path")"
  cat >"$env_path" <<ENV
export DOTNET_ROOT="$dotnet_root"
export OCTARYN_DOTNET_ROOT="$dotnet_root"
case ":\${PATH}:" in
  *":$dotnet_root:"*) ;;
  *) export PATH="$dotnet_root:\${PATH}" ;;
esac
ENV
  printf '[deps] wrote toolchain environment: %s\n' "$env_path"
}

ensure_dotnet_sdk_10() {
  if dotnet_sdk_10_available; then
    printf '[deps] .NET SDK 10 already available: %s\n' "$(dotnet --version)"
    return 0
  fi

  local dotnet_root install_script downloader
  dotnet_root="$(octaryn_toolchain_root)/dotnet"
  install_script="$(octaryn_toolchain_root)/dotnet-install.sh"
  mkdir -p "$(dirname "$install_script")"

  if command -v curl >/dev/null 2>&1; then
    downloader=(curl -fsSL -o "$install_script" https://dot.net/v1/dotnet-install.sh)
  elif command -v wget >/dev/null 2>&1; then
    downloader=(wget -O "$install_script" https://dot.net/v1/dotnet-install.sh)
  else
    printf '[error] curl or wget is required to install repo-local .NET SDK 10.\n' >&2
    exit 1
  fi

  printf '[deps] downloading .NET installer for SDK %s fallback\n' "$dotnet_channel"
  "${downloader[@]}"
  bash "$install_script" --channel "$dotnet_channel" --install-dir "$dotnet_root" --no-path
  write_toolchain_env "$dotnet_root"
  # shellcheck disable=SC1090
  source "$(octaryn_toolchain_env_path)"

  if ! dotnet_sdk_10_available; then
    printf '[error] .NET SDK 10 was not available after repo-local install.\n' >&2
    exit 1
  fi
  printf '[deps] repo-local .NET SDK 10 ready: %s\n' "$(dotnet --version)"
}

install_arch() {
  local packages=(
    base-devel
    git
    curl
    util-linux
    cmake
    ninja
    clang
    lld
    pkgconf
    python
    python-pip
    python-pillow
    vulkan-headers
    vulkan-icd-loader
    wayland
    wayland-protocols
    libxkbcommon
    libx11
    libxcursor
    libxi
    libxrandr
    libxss
    libxtst
    libxext
    libxrender
    libxfixes
    libdecor
    fribidi
    libthai
    libdrm
    mesa
    liburing
    pipewire
    libpulse
    alsa-lib
    jack2
    sndio
    libusb
  )
  append_if_enabled packages "$with_ui" pyside6
  append_if_enabled packages "$with_windows" mingw-w64-gcc mingw-w64-binutils

  local args=(-S --needed)
  [[ "$install_mode" == "yes" ]] && args+=(--noconfirm)
  run_install pacman "${args[@]}" "${packages[@]}"
  try_install pacman "${args[@]}" dotnet-sdk || true
}

install_debian() {
  local packages=(
    build-essential
    git
    ca-certificates
    curl
    util-linux
    cmake
    ninja-build
    clang
    lld
    pkg-config
    python3
    python3-pip
    python3-pil
    libvulkan1
    libvulkan-dev
    vulkan-tools
    xorg-dev
    libxkbcommon-dev
    libwayland-dev
    wayland-protocols
    libegl1-mesa-dev
    libgl-dev
    libdecor-0-dev
    libfribidi-dev
    libthai-dev
    libdrm-dev
    libgbm-dev
    liburing-dev
    libudev-dev
    libdbus-1-dev
    libibus-1.0-dev
    libasound2-dev
    libpipewire-0.3-dev
    libpulse-dev
    libjack-jackd2-dev
    libsndio-dev
    libusb-1.0-0-dev
  )
  append_if_enabled packages "$with_ui" python3-pyside6
  append_if_enabled packages "$with_windows" gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64

  run_install apt-get update
  local args=(install)
  [[ "$install_mode" == "yes" ]] && args+=(-y)
  run_install apt-get "${args[@]}" "${packages[@]}"
  try_install apt-get "${args[@]}" dotnet-sdk-10.0 || true
}

install_fedora() {
  local packages=(
    @development-tools
    git
    curl
    util-linux
    cmake
    ninja-build
    clang
    lld
    pkgconf-pkg-config
    python3
    python3-pip
    python3-pillow
    vulkan-loader
    vulkan-headers
    wayland-devel
    wayland-protocols-devel
    libxkbcommon-devel
    libX11-devel
    libXcursor-devel
    libXext-devel
    libXfixes-devel
    libXi-devel
    libXrandr-devel
    libXrender-devel
    libXScrnSaver-devel
    libXxf86vm-devel
    libXtst-devel
    mesa-libEGL-devel
    libglvnd-devel
    libdecor-devel
    fribidi-devel
    libthai-devel
    libdrm-devel
    mesa-libgbm-devel
    liburing-devel
    systemd-devel
    dbus-devel
    ibus-devel
    alsa-lib-devel
    pipewire-devel
    pulseaudio-libs-devel
    jack-audio-connection-kit-devel
    libsndio-devel
    libusb1-devel
  )
  append_if_enabled packages "$with_ui" python3-pyside6
  append_if_enabled packages "$with_windows" mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-headers

  local args=(install)
  [[ "$install_mode" == "yes" ]] && args+=(-y)
  run_install dnf "${args[@]}" "${packages[@]}"
  try_install dnf "${args[@]}" dotnet-sdk-10.0 || true
}

install_opensuse() {
  local packages=(
    gcc
    gcc-c++
    make
    git
    curl
    util-linux
    cmake
    ninja
    clang
    lld
    pkgconf
    python3
    python3-pip
    python3-Pillow
    libvulkan1
    vulkan-headers
    wayland-devel
    wayland-protocols-devel
    libxkbcommon-devel
    libX11-devel
    libXcursor-devel
    libXext-devel
    libXfixes-devel
    libXi-devel
    libXrandr-devel
    libXrender-devel
    libXss-devel
    libXtst-devel
    Mesa-libEGL-devel
    Mesa-libGL-devel
    libdecor-devel
    fribidi-devel
    libthai-devel
    libdrm-devel
    Mesa-libgbm-devel
    liburing-devel
    libudev-devel
    dbus-1-devel
    ibus-devel
    alsa-devel
    pipewire-devel
    libpulse-devel
    libjack-devel
    libsndio-devel
    libusb-1_0-devel
  )
  append_if_enabled packages "$with_ui" python3-pyside6
  append_if_enabled packages "$with_windows" mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-headers

  local args=(install)
  [[ "$install_mode" == "yes" ]] && args+=(-y)
  run_install zypper "${args[@]}" "${packages[@]}"
  try_install zypper "${args[@]}" dotnet-sdk-10.0 || true
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

ensure_dotnet_sdk_10
printf '[deps] installed system dependencies for %s\n' "$(octaryn_workspace_root)"
