#!/usr/bin/env bash

octaryn_workspace_root() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
  printf '%s\n' "$script_dir"
}

octaryn_engine_root() {
  printf '%s/references/old-architecture\n' "$(octaryn_workspace_root)"
}

octaryn_toolchain_root() {
  printf '%s/.octaryn-toolchain\n' "$(octaryn_workspace_root)"
}

octaryn_toolchain_env_path() {
  printf '%s/env.sh\n' "$(octaryn_toolchain_root)"
}

octaryn_source_toolchain_env() {
  local env_path
  env_path="$(octaryn_toolchain_env_path)"
  if [[ -f "$env_path" ]]; then
    # shellcheck disable=SC1090
    source "$env_path"
  fi
}

octaryn_source_toolchain_env

octaryn_build_root() {
  printf '%s/build/old-architecture\n' "$(octaryn_workspace_root)"
}

octaryn_log_root() {
  printf '%s/logs/old-architecture\n' "$(octaryn_workspace_root)"
}

octaryn_acquire_build_lock() {
  local label="${1:-build}"
  local lock_dir lock_file
  lock_dir="$(octaryn_build_root)/shared/locks"
  lock_file="${lock_dir}/build.lock"
  mkdir -p "$lock_dir"
  exec 9>"$lock_file"
  printf '[lock] waiting for Octaryn build lock: %s\n' "$label"
  flock 9
  printf '[lock] acquired Octaryn build lock: %s\n' "$label"
}

octaryn_product_name() {
  printf '%s\n' octaryn-engine
}

octaryn_known_presets() {
  printf '%s\n' linux-debug linux-profile linux-release windows-debug windows-release
}

octaryn_dependency_bucket_name() {
  case "$1" in
    linux-debug|linux-profile)
      printf '%s\n' linux-debug-shared
      ;;
    linux-release)
      printf '%s\n' linux-release-static
      ;;
    windows-debug)
      printf '%s\n' windows-debug
      ;;
    windows-release)
      printf '%s\n' windows-release
      ;;
    *)
      printf '%s\n' "$1"
      ;;
  esac
}

octaryn_workspace_preset_root() {
  printf '%s/workspace/%s\n' "$(octaryn_build_root)/shared" "$1"
}

octaryn_product_build_root() {
  printf '%s/%s\n' "$(octaryn_build_root)" "$(octaryn_product_name)"
}

octaryn_product_preset_dir() {
  printf '%s/%s\n' "$(octaryn_product_build_root)" "$1"
}

octaryn_product_log_dir() {
  printf '%s/%s/%s\n' "$(octaryn_log_root)" "$(octaryn_product_name)" "$1"
}

octaryn_product_runtime_dir() {
  printf '%s/runtime\n' "$(octaryn_product_preset_dir "$1")"
}

octaryn_product_world_dir() {
  printf '%s/world\n' "$(octaryn_product_runtime_dir "$1")"
}

octaryn_product_ui_dir() {
  printf '%s/ui\n' "$(octaryn_product_runtime_dir "$1")"
}

octaryn_product_bin_dir() {
  printf '%s/bin\n' "$(octaryn_product_preset_dir "$1")"
}

octaryn_product_lib_dir() {
  printf '%s/lib\n' "$(octaryn_product_preset_dir "$1")"
}

octaryn_product_lib_stage_dir() {
  printf '%s/libs/%s\n' "$(octaryn_product_build_root)" "$1"
}

octaryn_product_shared_dir() {
  printf '%s/shared\n' "$(octaryn_product_build_root)"
}

octaryn_shared_build_root() {
  printf '%s/shared\n' "$(octaryn_build_root)"
}

octaryn_shared_cpm_cache_dir() {
  printf '%s/cpm-cache\n' "$(octaryn_shared_build_root)"
}

octaryn_shared_deps_root() {
  printf '%s/deps/%s\n' "$(octaryn_shared_build_root)" "$(octaryn_dependency_bucket_name "$1")"
}

octaryn_shared_deps_log_dir() {
  printf '%s/deps/%s\n' "$(octaryn_log_root)" "$(octaryn_dependency_bucket_name "$1")"
}

octaryn_shared_deps_export_dir() {
  printf '%s/exports\n' "$(octaryn_shared_deps_root "$1")"
}

octaryn_shared_deps_install_dir() {
  printf '%s/install\n' "$(octaryn_shared_deps_root "$1")"
}

octaryn_workspace_cmake_dir() {
  printf '%s/cmake\n' "$(octaryn_workspace_preset_root "$1")"
}

octaryn_product_default_target() {
  printf '%s\n' octaryn_engine_product
}

octaryn_product_default_run_target() {
  printf '%s\n' octaryn_engine_runtime
}

octaryn_known_products() {
  printf '%s\n' octaryn-engine
}

octaryn_prune_generated_extras() {
  local build_dir="$1"
  local clutter=(
    "${build_dir}/check.sh"
    "${build_dir}/debug.sh"
    "${build_dir}/release.sh"
    "${build_dir}/cputypetest.c"
    "${build_dir}/CPackConfig.cmake"
    "${build_dir}/CPackSourceConfig.cmake"
  )

  local path
  for path in "${clutter[@]}"; do
    if [[ -e "$path" ]]; then
      rm -rf -- "$path"
    fi
  done
}

octaryn_log_has_dyndep_failure() {
  local log_path="$1"
  [[ -f "$log_path" ]] && grep -Eq \
    "RefreshDyndepDependents|outputs_ready|generated_by_dep_loader|dyndep requires Plan to have a Builder" \
    "$log_path"
}

octaryn_log_has_stale_build_graph_failure() {
  local log_path="$1"
  [[ -f "$log_path" ]] && grep -Eq \
    "missing and no known rule to make it" \
    "$log_path"
}
