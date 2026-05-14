#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

usage() {
  printf 'Usage:\n'
  printf '  %s start --preset <preset> --program <path> [-- arg ...]\n' "$(basename "$0")"
  printf '  %s stop --preset <preset>\n' "$(basename "$0")"
  printf '  %s status --preset <preset>\n' "$(basename "$0")"
}

require_user_systemd() {
  if ! command -v systemd-run >/dev/null 2>&1; then
    printf '[error] systemd-run was not found on PATH.\n' >&2
    exit 1
  fi
  if ! systemctl --user is-active default.target >/dev/null 2>&1; then
    printf '[error] user systemd session is not active.\n' >&2
    exit 1
  fi
}

unit_name_for_preset() {
  printf 'octaryn-engine-runtime-%s\n' "$1"
}

valid_preset() {
  octaryn_known_presets | grep -qx "$1"
}

command_name="${1:-}"
if [[ -z "$command_name" ]]; then
  usage >&2
  exit 1
fi
shift

preset="linux-debug"
program=""
declare -a program_args=()

while (($#)); do
  case "$1" in
    --preset)
      preset="${2:-}"
      shift 2
      ;;
    --program)
      program="${2:-}"
      shift 2
      ;;
    --)
      shift
      program_args=("$@")
      break
      ;;
    *)
      printf '[error] unknown argument: %s\n' "$1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if ! valid_preset "$preset"; then
  printf '[error] unknown preset: %s\n' "$preset" >&2
  exit 1
fi

unit_name="$(unit_name_for_preset "$preset")"
log_dir="$(octaryn_product_log_dir "$preset")"
mkdir -p "$log_dir"
log_path="$log_dir/runtime-session.log"

case "$command_name" in
  start)
    require_user_systemd
    if [[ -z "$program" ]]; then
      printf '[error] --program is required for start.\n' >&2
      exit 1
    fi
    if [[ ! -x "$program" ]]; then
      printf '[error] runtime executable is missing or not executable: %s\n' "$program" >&2
      exit 1
    fi
    if systemctl --user is-active --quiet "$unit_name.service"; then
      printf '[info] runtime unit already active: %s\n' "$unit_name"
      printf '[info] log: %s\n' "$log_path"
      exit 0
    fi

    workspace_root="$(octaryn_workspace_root)"
    session_vars=(WAYLAND_DISPLAY DISPLAY XDG_RUNTIME_DIR DBUS_SESSION_BUS_ADDRESS XDG_SESSION_TYPE)
    declare -a run_args=(
      --user
      --unit "$unit_name"
      --collect
      --same-dir
      --description "Octaryn Engine Runtime (${preset})"
      --setenv "OCTARYN_DETACHED_LAUNCH=1"
      --setenv "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-wayland}"
    )
    for var_name in "${session_vars[@]}"; do
      if [[ -n "${!var_name:-}" ]]; then
        run_args+=(--setenv "${var_name}=${!var_name}")
      fi
    done
    run_args+=(
      /usr/bin/bash
      -lc
      'log_path="$1"; shift; exec "$@" >>"$log_path" 2>&1'
      bash
      "$log_path"
      "$program"
    )
    if ((${#program_args[@]})); then
      run_args+=("${program_args[@]}")
    fi
    systemd-run "${run_args[@]}"
    printf '[info] runtime launched in user session unit: %s\n' "$unit_name"
    printf '[info] log: %s\n' "$log_path"
    ;;
  stop)
    require_user_systemd
    if systemctl --user is-active --quiet "$unit_name.service"; then
      systemctl --user stop "$unit_name.service"
      printf '[info] stopped runtime unit: %s\n' "$unit_name"
    else
      printf '[info] runtime unit not running: %s\n' "$unit_name"
    fi
    ;;
  status)
    require_user_systemd
    if systemctl --user is-active --quiet "$unit_name.service"; then
      printf '[info] runtime unit active: %s\n' "$unit_name"
    else
      printf '[info] runtime unit inactive: %s\n' "$unit_name"
    fi
    printf '[info] log: %s\n' "$log_path"
    ;;
  *)
    printf '[error] unknown command: %s\n' "$command_name" >&2
    usage >&2
    exit 1
    ;;
esac
