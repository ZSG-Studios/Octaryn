#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

preset=""
quiet="0"

usage() {
  cat <<'USAGE'
Usage: repair_dyndep.sh --preset <name> [--quiet]

Clears generated dependency build directories that can hold stale CMake/Ninja
C++ module dyndep state. Source, cache downloads, runtime data, logs, and
product artifacts are not removed.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset)
      preset="${2:-}"
      shift 2
      ;;
    --quiet)
      quiet="1"
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

if [[ -z "$preset" ]]; then
  usage >&2
  exit 2
fi

if ! octaryn_known_presets | grep -qx "$preset"; then
  printf '[error] unknown preset: %s\n' "$preset" >&2
  exit 2
fi

deps_root="$(octaryn_shared_deps_root "$preset")"
repair_dirs=(
  cpptrace-build
  cpptrace-subbuild
  openalsoft-build
  openalsoft-subbuild
)

if [[ "$quiet" != "1" ]]; then
  printf '[repair] preset=%s dependency_bucket=%s\n' "$preset" "$(octaryn_dependency_bucket_name "$preset")"
fi

for dir_name in "${repair_dirs[@]}"; do
  repair_path="${deps_root}/${dir_name}"
  if [[ -e "$repair_path" ]]; then
    if [[ "$quiet" != "1" ]]; then
      printf '[repair] removing generated dependency build dir: %s\n' "$repair_path"
    fi
    rm -rf -- "$repair_path"
  elif [[ "$quiet" != "1" ]]; then
    printf '[repair] already clean: %s\n' "$repair_path"
  fi
done
