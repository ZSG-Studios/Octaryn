#!/usr/bin/env bash
set -eu

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tool_environment.sh
source "${script_dir}/tool_environment.sh"
# shellcheck source=host_tool_bootstrap.sh
source "${script_dir}/host_tool_bootstrap.sh"

preset="${1:-debug-linux}"
octaryn_validate_preset_name "${preset}"
target_arch="$(octaryn_target_arch)"
shift || true
octaryn_use_podman_for_cmake build "${preset}" "$@"
if [[ "${target_arch}" == "x64" ]]; then
  exec cmake --build --preset "$preset" --parallel "$(nproc)" "$@"
fi

exec cmake --build "$(octaryn_cmake_build_dir "${preset}")" --parallel "$(octaryn_host_core_count)" "$@"
