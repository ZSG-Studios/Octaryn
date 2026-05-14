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
octaryn_use_podman_for_cmake configure "${preset}"
if [[ "${target_arch}" == "arm64" && "$(octaryn_preset_target_platform "${preset}")" == "linux" && -z "${OCTARYN_LINUX_ARM64_SYSROOT:-}" ]]; then
  sysroot_path="$("${octaryn_workspace_root}/tools/build/linux_arm64_sysroot.sh" setup)"
  export OCTARYN_LINUX_ARM64_SYSROOT="${sysroot_path}"
fi
if [[ "${target_arch}" == "x64" ]]; then
  exec cmake --preset "$preset" -DOCTARYN_TARGET_ARCH="${target_arch}"
fi

exec cmake -S "${octaryn_workspace_root}" -B "$(octaryn_cmake_build_dir "${preset}")" -G Ninja \
  -DCMAKE_BUILD_TYPE="$(octaryn_preset_build_type "${preset}")" \
  -DCMAKE_C_STANDARD=17 \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DCMAKE_TOOLCHAIN_FILE="$(octaryn_preset_toolchain_file "${preset}")" \
  -DOCTARYN_BUILD_PRESET_NAME="${preset}" \
  -DOCTARYN_TARGET_ARCH="${target_arch}"
