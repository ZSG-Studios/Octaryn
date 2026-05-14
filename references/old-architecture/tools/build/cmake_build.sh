#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

preset=""
target=""
verbose_build="0"
repair_enabled="1"
build_tool_args=()

usage() {
  cat <<'USAGE'
Usage: cmake_build.sh --preset <name> [--target <name>] [--verbose] [--no-repair] [-- <build-tool-args>...]

Runs `cmake --build` and automatically repairs stale generated Ninja/CMake
dependency state when the failure matches known dyndep or stale graph
signatures.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset)
      preset="${2:-}"
      shift 2
      ;;
    --target)
      target="${2:-}"
      shift 2
      ;;
    --verbose)
      verbose_build="1"
      shift
      ;;
    --no-repair)
      repair_enabled="0"
      shift
      ;;
    --)
      shift
      build_tool_args=("$@")
      break
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

if [[ -z "$preset" ]]; then
  usage >&2
  exit 2
fi

if ! octaryn_known_presets | grep -qx "$preset"; then
  printf '[error] unknown preset: %s\n' "$preset" >&2
  exit 2
fi

octaryn_acquire_build_lock "build ${preset}${target:+/${target}}"

run_build() {
  local log_path="$1"
  local force_serial="$2"
  local command=(cmake --build --preset "$preset")

  if [[ -n "$target" ]]; then
    command+=(--target "$target")
  fi
  if [[ "$verbose_build" == "1" ]]; then
    command+=(--verbose)
  fi
  if [[ ${#build_tool_args[@]} -gt 0 ]]; then
    command+=(-- "${build_tool_args[@]}")
    if [[ "$force_serial" == "1" ]]; then
      command+=(-j1)
    fi
  elif [[ "$force_serial" == "1" ]]; then
    command+=(-- -j1)
  fi

  printf '[build] command: %s\n' "${command[*]}"
  set +e
  (cd "$(octaryn_engine_root)" && stdbuf -oL -eL "${command[@]}") 2>&1 | tee "$log_path"
  local status="${PIPESTATUS[0]}"
  set -e
  return "$status"
}

log_path="$(mktemp -t octaryn-cmake-build.XXXXXX.log)"
trap 'rm -f "$log_path"' EXIT

max_attempts=4
attempt=1
force_serial="0"

while (( attempt <= max_attempts )); do
  : >"$log_path"
  set +e
  run_build "$log_path" "$force_serial"
  build_status="$?"
  set -e

  if [[ "$build_status" -eq 0 ]]; then
    exit 0
  fi

  if [[ "$repair_enabled" != "1" ]]; then
    exit "$build_status"
  fi

  if (( attempt == max_attempts )); then
    printf '[build] automatic repair limit reached after %d attempts.\n' "$attempt" >&2
    exit "$build_status"
  fi

  if octaryn_log_has_dyndep_failure "$log_path"; then
    printf '[build] stale CMake/Ninja dyndep state detected; repairing generated dependency dirs and retrying serialized (%d/%d).\n' \
      "$((attempt + 1))" "$max_attempts" >&2
    bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/repair_dyndep.sh" --preset "$preset"
    bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/configure.sh" --preset "$preset"
    force_serial="1"
  elif octaryn_log_has_stale_build_graph_failure "$log_path"; then
    printf '[build] stale generated build graph detected; reconfiguring and retrying (%d/%d).\n' \
      "$((attempt + 1))" "$max_attempts" >&2
    bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/configure.sh" --preset "$preset"
  else
    exit "$build_status"
  fi

  attempt=$((attempt + 1))
done
