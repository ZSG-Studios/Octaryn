#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

workspace_root="$(octaryn_workspace_root)"
preset="linux-profile"
capture_seconds="30"
raise_blocks="40"
speed_blocks_per_second="100"
startup_wait_seconds="4"
present_mode="${OCTARYN_BENCH_PRESENT_MODE:-immediate}"
acquire_mode="${OCTARYN_BENCH_ACQUIRE_MODE:-late}"
frames_in_flight="${OCTARYN_BENCH_FRAMES_IN_FLIGHT:-3}"
edit_under_player="0"
edit_interval_ms="100"
save_path="${OCTARYN_BENCH_SAVE_PATH:-}"
log_dir="$(octaryn_product_log_dir "$preset")"
runtime_log="${log_dir}/runtime-session.log"
runtime_path="$(octaryn_product_bin_dir "$preset")/octaryn-engine-runtime"
tracy_capture_script="${workspace_root}/references/old-architecture/tools/build/tracy_capture.sh"
declare -a runtime_args=()

usage() {
  cat <<'USAGE'
Usage: stream_benchmark_tracy.sh [options]

Options:
  --seconds <n>       Tracy capture duration. Default: 30.
  --raise <blocks>    Raise player by this many blocks. Default: 40.
  --speed <blocks/s>  Move in +X at this speed. Default: 100.
  --startup-wait <n>  Seconds to wait before capture. Default: 4.
  --present <mode>    GPU present mode: immediate, mailbox, vsync, auto. Default: immediate.
  --acquire <mode>    Swapchain acquire mode: early, late, nonblocking. Default: late.
  --frames <n>        Frames in flight. Default: 3.
  --edit-under-player Break terrain surface blocks under the moving player.
  --edit-interval <n> Milliseconds between benchmark block edits. Default: 100.
  --save-path <path>  Override engine save root. Default: normal user settings/save path.
USAGE
}

while (($#)); do
  case "$1" in
    --seconds)
      capture_seconds="${2:-}"
      shift 2
      ;;
    --raise)
      raise_blocks="${2:-}"
      shift 2
      ;;
    --speed)
      speed_blocks_per_second="${2:-}"
      shift 2
      ;;
    --startup-wait)
      startup_wait_seconds="${2:-}"
      shift 2
      ;;
    --present)
      present_mode="${2:-}"
      shift 2
      ;;
    --acquire)
      acquire_mode="${2:-}"
      shift 2
      ;;
    --frames)
      frames_in_flight="${2:-}"
      shift 2
      ;;
    --edit-under-player)
      edit_under_player="1"
      shift
      ;;
    --edit-interval)
      edit_interval_ms="${2:-}"
      shift 2
      ;;
    --save-path)
      save_path="${2:-}"
      shift 2
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

if [[ -n "$save_path" ]]; then
  runtime_args+=(--save-path "$save_path")
fi

mkdir -p "$log_dir"
bash "${workspace_root}/references/old-architecture/tools/build/cmake_build.sh" --preset linux-profile --target octaryn_engine_runtime_bundle

if [[ ! -x "$runtime_path" ]]; then
  printf '[error] runtime executable is missing: %s\n' "$runtime_path" >&2
  exit 2
fi

rm -f "$runtime_log"

runtime_pid=""
cleanup() {
  if [[ -n "$runtime_pid" ]] && kill -0 "$runtime_pid" 2>/dev/null; then
    kill "$runtime_pid" 2>/dev/null || true
    sleep 0.5
    if kill -0 "$runtime_pid" 2>/dev/null; then
      kill -9 "$runtime_pid" 2>/dev/null || true
    fi
  fi
}
trap cleanup EXIT

printf '[info] launching stream benchmark runtime: %s\n' "$runtime_path"
printf '[info] benchmark pacing: present=%s acquire=%s frames_in_flight=%s\n' \
  "$present_mode" \
  "$acquire_mode" \
  "$frames_in_flight"
printf '[info] benchmark edits: under_player=%s interval_ms=%s save_path=%s\n' \
  "$edit_under_player" \
  "$edit_interval_ms" \
  "${save_path:-engine-default}"
env \
  OCTARYN_STREAM_BENCH=1 \
  OCTARYN_STREAM_BENCH_EDIT_UNDER_PLAYER="${edit_under_player}" \
  OCTARYN_STREAM_BENCH_EDIT_INTERVAL_MS="${edit_interval_ms}" \
  OCTARYN_STREAM_BENCH_RAISE="${raise_blocks}" \
  OCTARYN_STREAM_BENCH_SPEED="${speed_blocks_per_second}" \
  OCTARYN_LOG_RENDER_PROFILE=1 \
  OCTARYN_PRESENT_MODE="${present_mode}" \
  OCTARYN_ACQUIRE_MODE="${acquire_mode}" \
  OCTARYN_FRAMES_IN_FLIGHT="${frames_in_flight}" \
  OCTARYN_FPS_CAP=0 \
  "$runtime_path" "${runtime_args[@]}" >>"$runtime_log" 2>&1 &
runtime_pid="$!"
printf '[info] runtime pid=%s\n' "$runtime_pid"
printf '[info] runtime log=%s\n' "$runtime_log"

sleep "$startup_wait_seconds"
if ! kill -0 "$runtime_pid" 2>/dev/null; then
  printf '[error] runtime exited before capture. Last log lines:\n' >&2
  tail -n 80 "$runtime_log" >&2 || true
  exit 1
fi

OCTARYN_TRACY_RUNTIME_LOG="$runtime_log" "$tracy_capture_script" --seconds "${capture_seconds}"
latest_capture="$("$tracy_capture_script" --print-latest)"
printf '[info] latest tracy capture artifact=%s\n' "$latest_capture"
printf '[info] latest real FPS summary=%s\n' "${latest_capture%.tracy}.txt"
