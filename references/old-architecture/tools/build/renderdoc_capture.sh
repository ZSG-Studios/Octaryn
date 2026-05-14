#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

usage() {
  printf 'Usage: %s --preset <preset> --program <path> [-- arg ...]\n' "$(basename "$0")"
}

valid_preset() {
  octaryn_known_presets | grep -qx "$1"
}

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
    -h|--help)
      usage
      exit 0
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
if [[ -z "$program" || ! -x "$program" ]]; then
  printf '[error] runtime executable is missing or not executable: %s\n' "$program" >&2
  exit 1
fi

workspace_root="$(octaryn_workspace_root)"
renderdoc_script="${workspace_root}/references/old-architecture/tools/build/renderdoc.sh"
log_dir="${workspace_root}/logs/references/old-architecture/renderdoc"
capture_dir="${log_dir}/captures"
mkdir -p "$log_dir" "$capture_dir"

renderdoccmd_path="$("$renderdoc_script" --print-cmd-path)"
if [[ -z "$renderdoccmd_path" || ! -x "$renderdoccmd_path" ]]; then
  printf '[error] renderdoccmd is unavailable after project RenderDoc setup.\n' >&2
  exit 1
fi

stamp="$(date +%Y%m%d-%H%M%S)"
capture_template="${capture_dir}/${stamp}"
log_path="${log_dir}/${stamp}-capture.log"

printf '[info] RenderDoc command: %s\n' "$renderdoccmd_path"
printf '[info] Runtime: %s\n' "$program"
printf '[info] Capture template: %s\n' "$capture_template"
printf '[info] Capture log: %s\n' "$log_path"
printf '[info] Launching under X11/Xwayland because this project RenderDoc build captures Vulkan through Xlib/XCB.\n'

(
  cd "$workspace_root"
  env \
    SDL_VIDEODRIVER=x11 \
    OCTARYN_PRESENT_MODE="${OCTARYN_PRESENT_MODE:-immediate}" \
    OCTARYN_ACQUIRE_MODE="${OCTARYN_ACQUIRE_MODE:-late}" \
    OCTARYN_FRAMES_IN_FLIGHT="${OCTARYN_FRAMES_IN_FLIGHT:-2}" \
    OCTARYN_FPS_CAP="${OCTARYN_FPS_CAP:-0}" \
    OCTARYN_LOG_RENDER_PROFILE="${OCTARYN_LOG_RENDER_PROFILE:-1}" \
    "$renderdoccmd_path" capture \
      -d "$workspace_root" \
      -c "$capture_template" \
      --opt-disallow-vsync \
      --opt-api-validation \
      "$program" "${program_args[@]}" \
      >"$log_path" 2>&1 &
)

printf '[info] RenderDoc capture target launched. Use RenderDoc UI or F12 to capture a frame.\n'
