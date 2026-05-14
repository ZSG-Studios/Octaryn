#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tool_environment.sh
source "${script_dir}/tool_environment.sh"
# shellcheck source=host_tool_bootstrap.sh
source "${script_dir}/host_tool_bootstrap.sh"

image="${OCTARYN_PODMAN_BUILD_IMAGE:-localhost/octaryn-arch-builder:latest}"
containerfile="${octaryn_workspace_root}/tools/build/Containerfile.arch-build"
podman_context="${octaryn_workspace_root}/tools/build"
workspace_container_path="${octaryn_workspace_root}"
action="${1:-status}"

usage() {
  cat <<'USAGE'
Usage: podman_build.sh <command> [args]

Commands:
  status                    Print host and Podman build environment status.
  list-presets              List CMake build presets inside Podman.
  configure <preset>        Configure one preset inside Podman.
  build <preset> [args...]  Build one preset inside Podman. Extra args go to cmake_build.sh.
  validate <preset>         Build octaryn_validate_all inside Podman.
  build-all                 Configure and build every active preset for x64 and arm64.

Environment:
  OCTARYN_TARGET_ARCH          x64 or arm64. Default: x64.
  OCTARYN_PODMAN_BUILD_IMAGE   Local builder image tag.
  OCTARYN_PODMAN_REBUILD       Set to 1 to rebuild the builder image before running.
USAGE
}

host_platform() {
  case "$(uname -s)" in
    Linux) printf 'linux\n' ;;
    *) printf 'unknown\n' ;;
  esac
}

require_linux_host() {
  if [[ "$(host_platform)" != "linux" ]]; then
    printf '[error] Octaryn builds run from Linux with Podman. Windows is a cross-build target only.\n' >&2
    exit 1
  fi
}

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    octaryn_ensure_host_tool "$1" 0
  fi
}

ensure_builder_image() {
  require_tool podman
  local fingerprint
  fingerprint="$(builder_fingerprint)"
  if [[ "${OCTARYN_PODMAN_REBUILD:-0}" != "1" ]] \
    && podman image exists "${image}" >/dev/null 2>&1 \
    && [[ "$(image_fingerprint)" == "${fingerprint}" ]]; then
    return
  fi
  if [[ ! -f "${containerfile}" ]]; then
    printf '[error] missing Podman builder Containerfile: %s\n' "${containerfile}" >&2
    exit 1
  fi
  printf '[info] building Octaryn Podman build image: %s\n' "${image}"
  podman build \
    --file "${containerfile}" \
    --label "org.octaryn.arch-builder.fingerprint=${fingerprint}" \
    --tag "${image}" \
    "${podman_context}"
}

builder_fingerprint() {
  sha256sum "${containerfile}" "${podman_context}/arch_packages.txt" | sha256sum | awk '{print $1}'
}

image_fingerprint() {
  podman image inspect \
    --format '{{ index .Config.Labels "org.octaryn.arch-builder.fingerprint" }}' \
    "${image}" 2>/dev/null || true
}

require_linux_host

workspace_volume="${octaryn_workspace_root}:${workspace_container_path}:Z"

podman_args=(
  --rm
  --volume "${workspace_volume}"
  --workdir "${workspace_container_path}"
  --env "OCTARYN_WORKSPACE_ROOT=${workspace_container_path}"
  --env "DOTNET_CLI_TELEMETRY_OPTOUT=1"
  --env "DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1"
  --env "OCTARYN_IN_BUILDER=1"
)

mount_optional_host_path() {
  local env_name="$1"
  local default_path="${2:-}"
  local host_path="${!env_name:-${default_path}}"
  if [[ -z "${host_path}" || ! -d "${host_path}" ]]; then
    return
  fi

  local volume="${host_path}:${host_path}:ro,Z"
  podman_args+=(--volume "${volume}" --env "${env_name}=${host_path}")
}

run_in_builder() {
  ensure_builder_image
  podman run "${podman_args[@]}" --env "OCTARYN_TARGET_ARCH=$(octaryn_target_arch)" "${image}" "$@"
}

configure_preset() {
  local preset="$1"
  octaryn_validate_preset_name "${preset}"
  run_in_builder bash "tools/build/cmake_configure.sh" "${preset}"
}

build_preset() {
  local preset="$1"
  shift
  octaryn_validate_preset_name "${preset}"
  run_in_builder bash "tools/build/cmake_build.sh" "${preset}" "$@"
}

mount_optional_host_path "OCTARYN_WINDOWS_CLANG_ROOT" "/opt/llvm-mingw"
mount_optional_host_path "OCTARYN_LINUX_ARM64_SYSROOT"

case "${action}" in
  status)
    printf 'host=%s\n' "$(host_platform)"
    printf 'workspace=%s\n' "${octaryn_workspace_root}"
    printf 'target_arch=%s\n' "$(octaryn_target_arch)"
    printf 'builder_image=%s\n' "${image}"
    printf 'containerfile=%s\n' "${containerfile}"
    if command -v podman >/dev/null 2>&1; then
      printf 'podman=%s\n' "$(command -v podman)"
      if podman image exists "${image}" >/dev/null 2>&1; then
        printf 'builder_image_status=ready\n'
      else
        printf 'builder_image_status=missing\n'
      fi
    else
      printf 'podman=missing\n'
      printf 'builder_image_status=unavailable\n'
    fi
    ;;
  list-presets)
    run_in_builder cmake --list-presets=build
    ;;
  configure)
    shift
    configure_preset "${1:-debug-linux}"
    ;;
  build)
    shift
    preset="${1:-debug-linux}"
    shift || true
    build_preset "${preset}" "$@"
    ;;
  validate)
    shift
    preset="${1:-debug-linux}"
    build_preset "${preset}" --target octaryn_validate_all
    ;;
  build-all)
    for arch in x64 arm64; do
      export OCTARYN_TARGET_ARCH="${arch}"
      for preset in debug-linux release-linux debug-windows release-windows; do
        configure_preset "${preset}"
        build_preset "${preset}" --target octaryn_all
      done
    done
    ;;
  --help|-h|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
