#!/usr/bin/env bash

octaryn_host_bootstrap_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
octaryn_host_bootstrap_setup_script="${octaryn_host_bootstrap_script_dir}/linux_build_environment.sh"

octaryn_host_platform() {
  case "$(uname -s)" in
    Linux) printf 'linux\n' ;;
    *) printf 'unknown\n' ;;
  esac
}

octaryn_in_managed_builder() {
  [[ "${OCTARYN_IN_BUILDER:-0}" == "1" ]]
}

octaryn_auto_install_enabled() {
  [[ "${OCTARYN_AUTO_INSTALL_TOOLS:-1}" != "0" ]]
}

octaryn_run_host_setup() {
  local with_ui="${1:-0}"
  if octaryn_in_managed_builder; then
    printf '[error] missing required builder tool inside Octaryn build image.\n' >&2
    exit 1
  fi
  if [[ "$(octaryn_host_platform)" != "linux" ]]; then
    printf '[error] automatic host setup is only supported on Linux.\n' >&2
    exit 1
  fi
  if ! octaryn_auto_install_enabled; then
    printf '[error] host tool auto-install is disabled by OCTARYN_AUTO_INSTALL_TOOLS=0.\n' >&2
    exit 1
  fi
  if [[ ! -f "${octaryn_host_bootstrap_setup_script}" ]]; then
    printf '[error] missing host setup helper: %s\n' "${octaryn_host_bootstrap_setup_script}" >&2
    exit 1
  fi

  local args=(--yes)
  [[ "${with_ui}" == "1" ]] || args+=(--no-ui)
  bash "${octaryn_host_bootstrap_setup_script}" "${args[@]}"
}

octaryn_ensure_host_tool() {
  local tool_name="$1"
  local with_ui="${2:-0}"
  if command -v "${tool_name}" >/dev/null 2>&1; then
    return 0
  fi

  printf '[setup] missing host tool %s; running Octaryn host setup\n' "${tool_name}"
  octaryn_run_host_setup "${with_ui}"

  if ! command -v "${tool_name}" >/dev/null 2>&1; then
    printf '[error] host tool is still missing after setup: %s\n' "${tool_name}" >&2
    exit 1
  fi
}

octaryn_use_podman_for_cmake() {
  local action="$1"
  local preset="$2"
  shift 2

  if command -v cmake >/dev/null 2>&1 || octaryn_in_managed_builder; then
    return 0
  fi

  printf '[setup] host cmake is missing; routing %s through the Octaryn Podman builder\n' "${action}"
  octaryn_ensure_host_tool podman 0
  exec "${octaryn_host_bootstrap_script_dir}/podman_build.sh" "${action}" "${preset}" "$@"
}
