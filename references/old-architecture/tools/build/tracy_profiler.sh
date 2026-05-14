#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

workspace_root="$(octaryn_workspace_root)"
log_dir="${workspace_root}/logs/engine_control"
log_path="${log_dir}/tracy-profiler.log"
build_dir="$(octaryn_product_build_root)/tools/tracy-profiler"
binary_path="${build_dir}/tracy-profiler"

mkdir -p "$log_dir"

find_tracy_source() {
  local cache_dir
  cache_dir="$(octaryn_shared_cpm_cache_dir)/tracy"
  if [[ ! -d "$cache_dir" ]]; then
    return 1
  fi
  find "$cache_dir" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1
}

main() {
  local print_path="0"
  if [[ "${1:-}" == "--print-path" ]]; then
    print_path="1"
    shift
  fi

  if [[ "$print_path" != "1" ]]; then
    exec >>"$log_path" 2>&1
  fi

  if [[ "$print_path" != "1" ]]; then
    printf '[%s] tracy profiler launcher\n' "$(date --iso-8601=seconds)"
  fi

  local tracy_source
  if ! tracy_source="$(find_tracy_source)" || [[ -z "$tracy_source" ]]; then
    printf '[error] project Tracy source is missing. Configure with OCTARYN_ENABLE_TRACY=ON first.\n' >&2
    exit 2
  fi

  local profiler_source="${tracy_source}/profiler"
  if [[ ! -f "${profiler_source}/CMakeLists.txt" ]]; then
    printf '[error] Tracy profiler source is missing: %s\n' "$profiler_source" >&2
    exit 2
  fi

  if [[ ! -x "$binary_path" ]]; then
    printf '[info] building project Tracy profiler from %s\n' "$profiler_source"
    cmake -S "$profiler_source" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DNO_ISA_EXTENSIONS=ON
    cmake --build "$build_dir" --target tracy-profiler --parallel
  fi

  if [[ "$print_path" == "1" ]]; then
    printf '%s\n' "$binary_path"
    return 0
  fi

  printf '[info] launching %s\n' "$binary_path"
  if command -v setsid >/dev/null 2>&1; then
    setsid "$binary_path" "$@" >>"$log_path" 2>&1 < /dev/null &
  else
    nohup "$binary_path" "$@" >>"$log_path" 2>&1 < /dev/null &
  fi
  printf '[info] Tracy profiler pid=%d\n' "$!"
}

main "$@"
