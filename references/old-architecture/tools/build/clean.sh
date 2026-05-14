#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

preset=""
all_presets="0"
purge_runtime="0"

clean_build_dir() {
  local build_dir="$1"
  local runtime_dir="${build_dir}/runtime"
  local preserve_dir=""

  if [[ ! -d "$build_dir" ]]; then
    return 0
  fi

  if [[ "$purge_runtime" != "1" && -d "$runtime_dir" ]]; then
    preserve_dir="$(mktemp -d)"
    mv "$runtime_dir" "${preserve_dir}/runtime"
  fi

  rm -rf -- "$build_dir"

  if [[ -n "$preserve_dir" && -d "${preserve_dir}/runtime" ]]; then
    mkdir -p "$build_dir"
    mv "${preserve_dir}/runtime" "$runtime_dir"
    rmdir "$preserve_dir"
  fi

  printf 'Cleaned %s\n' "$build_dir"
}

clean_shared_deps_preset() {
  local preset_name="$1"
  local shared_deps_dir
  shared_deps_dir="$(octaryn_shared_deps_root "$preset_name")"
  if [[ -d "$shared_deps_dir" ]]; then
    rm -rf -- "$shared_deps_dir"
    printf 'Cleaned %s\n' "$shared_deps_dir"
  fi
}

clean_workspace_preset() {
  local preset_name="$1"
  local workspace_dir
  workspace_dir="$(octaryn_workspace_preset_root "$preset_name")"
  if [[ -d "$workspace_dir" ]]; then
    rm -rf -- "$workspace_dir"
    printf 'Cleaned %s\n' "$workspace_dir"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset)
      preset="$2"
      shift 2
      ;;
    --all-presets)
      all_presets="1"
      shift
      ;;
    --purge-runtime)
      purge_runtime="1"
      shift
      ;;
    *)
      printf 'unknown argument: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done

if [[ "$all_presets" != "1" && -z "$preset" ]]; then
  printf 'usage: %s (--preset <name> | --all-presets) [--purge-runtime]\n' "$0" >&2
  exit 2
fi

if [[ "$all_presets" == "1" ]]; then
  mapfile -t presets < <(octaryn_known_presets)
else
  presets=("$preset")
fi

octaryn_acquire_build_lock "clean"

for preset_name in "${presets[@]}"; do
  clean_build_dir "$(octaryn_product_preset_dir "$preset_name")"
  if [[ "$purge_runtime" == "1" ]]; then
    rm -rf -- "$(octaryn_product_shared_dir)/install"
  fi
done

if [[ "$all_presets" == "1" ]]; then
  shopt -s nullglob
  for shared_dir in "$(octaryn_shared_build_root)/deps"/*; do
    if [[ -d "$shared_dir" ]]; then
      clean_shared_deps_preset "$(basename "$shared_dir")"
    fi
  done
else
  clean_shared_deps_preset "$preset"
fi

if [[ "$all_presets" == "1" ]]; then
  shopt -s nullglob
  for workspace_dir in "$(octaryn_shared_build_root)/workspace"/*; do
    if [[ -d "$workspace_dir" ]]; then
      clean_workspace_preset "$(basename "$workspace_dir")"
    fi
  done
else
  clean_workspace_preset "$preset"
fi
