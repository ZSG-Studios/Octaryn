#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

preset=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset)
      preset="$2"
      shift 2
      ;;
    *)
      printf 'unknown argument: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$preset" ]]; then
  printf 'usage: %s --preset <name>\n' "$0" >&2
  exit 2
fi

if ! octaryn_known_presets | grep -qx "$preset"; then
  printf 'unknown preset: %s\n' "$preset" >&2
  exit 2
fi

octaryn_acquire_build_lock "configure ${preset}"

(cd "$(octaryn_engine_root)" && cmake -Wno-deprecated --preset "$preset")
octaryn_prune_generated_extras "$(octaryn_workspace_cmake_dir "$preset")"

printf 'Configure complete for preset %s\n' "$preset"
