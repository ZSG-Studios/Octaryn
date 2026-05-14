#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

# shellcheck source=build/tool_environment.sh
source "${script_dir}/build/tool_environment.sh"
# shellcheck source=build/host_tool_bootstrap.sh
source "${script_dir}/build/host_tool_bootstrap.sh"

preset="${1:-debug-linux}"
shift || true

octaryn_validate_preset_name "${preset}"

if [[ "$(octaryn_host_platform)" == "linux" ]] && [[ "$(octaryn_in_managed_builder)" != "1" ]]; then
  if ! command -v dotnet >/dev/null 2>&1; then
    printf '[setup] host dotnet is missing; installing Octaryn graphical runtime requirements\n'
    "${repo_root}/tools/build/linux_build_environment.sh" --yes --runtime
  fi
fi

"${repo_root}/tools/build/cmake_build.sh" "${preset}" --target octaryn_client_bundle

client_bundle="${repo_root}/build/${preset}/client/bundle"
client_exe="${client_bundle}/Octaryn.Client"
log_path="${OCTARYN_CLIENT_APP_LOG_PATH:-${repo_root}/logs/client/octaryn-client-run-${preset}.log}"

if [[ ! -x "${client_exe}" ]]; then
  printf '[error] client launcher is missing or not executable: %s\n' "${client_exe}" >&2
  exit 1
fi

mkdir -p "$(dirname "${log_path}")"

export OCTARYN_CLIENT_APP_LOG_PATH="${log_path}"

cd "${client_bundle}"
exec "${client_exe}" "$@"
