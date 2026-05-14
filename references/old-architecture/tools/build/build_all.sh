#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

include_release="0"
install_deps="0"
profiles=(linux-debug windows-debug)

usage() {
  cat <<'USAGE'
Usage: build_all.sh [--install] [--release]

Configures and builds the default Linux and Windows profiles. Debug profiles are
the default because they are fast enough for fresh container validation.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      install_deps="1"
      shift
      ;;
    --release)
      include_release="1"
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

if [[ "$install_deps" == "1" ]]; then
  bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/install_deps.sh" --yes
  octaryn_source_toolchain_env
fi

if [[ "$include_release" == "1" ]]; then
  profiles=(linux-debug linux-release windows-debug windows-release)
fi

for preset in "${profiles[@]}"; do
  printf '[all] configure %s\n' "$preset"
  bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/configure.sh" --preset "$preset"

  target="octaryn_engine_runtime_bundle"
  case "$preset" in
    windows-*)
      target="octaryn_engine_runtime"
      ;;
  esac

  printf '[all] build %s target %s\n' "$preset" "$target"
  bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/cmake_build.sh" --preset "$preset" --target "$target"
done

printf '[all] complete\n'
