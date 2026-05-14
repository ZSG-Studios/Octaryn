#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

workspace_root="$(octaryn_workspace_root)"
preset="linux-profile"
run_seconds="15"
capture_frame="500"
raise_blocks="40"
speed_blocks_per_second="100"
present_mode="${OCTARYN_BENCH_PRESENT_MODE:-immediate}"
acquire_mode="${OCTARYN_BENCH_ACQUIRE_MODE:-late}"
frames_in_flight="${OCTARYN_BENCH_FRAMES_IN_FLIGHT:-3}"
edit_under_player="0"
edit_interval_ms="100"
save_path="${OCTARYN_BENCH_SAVE_PATH:-}"
log_dir="$(octaryn_product_log_dir "$preset")"
runtime_log="${log_dir}/runtime-session.log"
runtime_path="$(octaryn_product_bin_dir "$preset")/octaryn-engine-runtime"
capture_root="${workspace_root}/logs/references/old-architecture/renderdoc"
capture_dir="${capture_root}/captures"
capture_log="${capture_root}/stream-benchmark-renderdoc.log"
renderdoc_tool_script="${workspace_root}/references/old-architecture/tools/build/renderdoc.sh"
declare -a runtime_args=()

usage() {
  cat <<'USAGE'
Usage: stream_benchmark_renderdoc.sh [options]

Options:
  --seconds <n>        Benchmark runtime before clean shutdown. Default: 15.
  --capture-frame <n>  Render frame to capture. Default: 500.
  --raise <blocks>     Raise player by this many blocks. Default: 40.
  --speed <blocks/s>   Move in +X at this speed. Default: 100.
  --present <mode>     GPU present mode: immediate, mailbox, vsync, auto. Default: immediate.
  --acquire <mode>     Swapchain acquire mode: early, late, nonblocking. Default: late.
  --frames <n>         Frames in flight. Default: 3.
  --edit-under-player  Break terrain surface blocks under the moving player.
  --edit-interval <n>  Milliseconds between benchmark block edits. Default: 100.
  --save-path <path>   Override engine save root. Default: normal user settings/save path.
USAGE
}

latest_capture() {
  find "$capture_dir" -maxdepth 1 -type f -name '*.rdc' -printf '%T@ %p\n' |
    sort -nr |
    head -n 1 |
    cut -d' ' -f2-
}

while (($#)); do
  case "$1" in
    --seconds)
      run_seconds="${2:-}"
      shift 2
      ;;
    --capture-frame)
      capture_frame="${2:-}"
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

if [[ ! "$run_seconds" =~ ^[0-9]+$ ]] || ((run_seconds < 1)); then
  printf '[error] --seconds must be a positive integer.\n' >&2
  exit 2
fi
if [[ ! "$capture_frame" =~ ^[0-9]+$ ]] || ((capture_frame < 1)); then
  printf '[error] --capture-frame must be a positive integer.\n' >&2
  exit 2
fi

mkdir -p "$log_dir" "$capture_dir"
bash "${workspace_root}/references/old-architecture/tools/build/cmake_build.sh" --preset linux-profile --target octaryn_engine_runtime_bundle

if [[ ! -x "$runtime_path" ]]; then
  printf '[error] runtime executable is missing: %s\n' "$runtime_path" >&2
  exit 2
fi

renderdoccmd_path="$("$renderdoc_tool_script" --print-cmd-path)"
if [[ -z "$renderdoccmd_path" || ! -x "$renderdoccmd_path" ]]; then
  printf '[error] renderdoccmd is missing: %s\n' "$renderdoccmd_path" >&2
  exit 2
fi

rm -f "$runtime_log" "$capture_log"
capture_template="${capture_dir}/octaryn-stream"
timeout_seconds=$((run_seconds + 45))

printf '[info] launching RenderDoc stream benchmark: %s\n' "$runtime_path" | tee -a "$capture_log"
printf '[info] renderdoccmd capture output template=%s\n' "$capture_template" | tee -a "$capture_log"
printf '[info] benchmark pacing: present=%s acquire=%s frames_in_flight=%s\n' \
  "$present_mode" \
  "$acquire_mode" \
  "$frames_in_flight" | tee -a "$capture_log"
printf '[info] benchmark edits: under_player=%s interval_ms=%s save_path=%s\n' \
  "$edit_under_player" \
  "$edit_interval_ms" \
  "${save_path:-engine-default}" | tee -a "$capture_log"
printf '[info] runtime log=%s\n' "$runtime_log" | tee -a "$capture_log"

set +e
timeout --kill-after=5s "${timeout_seconds}s" env \
  SDL_VIDEODRIVER=x11 \
  OCTARYN_STREAM_BENCH=1 \
  OCTARYN_STREAM_BENCH_EDIT_UNDER_PLAYER="${edit_under_player}" \
  OCTARYN_STREAM_BENCH_EDIT_INTERVAL_MS="${edit_interval_ms}" \
  OCTARYN_STREAM_BENCH_RAISE="${raise_blocks}" \
  OCTARYN_STREAM_BENCH_SPEED="${speed_blocks_per_second}" \
  OCTARYN_STREAM_BENCH_QUIT_SECONDS="${run_seconds}" \
  OCTARYN_RENDERDOC_CAPTURE=1 \
  OCTARYN_RENDERDOC_CAPTURE_FRAME="${capture_frame}" \
    OCTARYN_LOG_RENDER_PROFILE=1 \
    OCTARYN_PRESENT_MODE="${present_mode}" \
    OCTARYN_ACQUIRE_MODE="${acquire_mode}" \
    OCTARYN_FRAMES_IN_FLIGHT="${frames_in_flight}" \
    OCTARYN_FPS_CAP=0 \
    "$renderdoccmd_path" capture \
    -d "$workspace_root" \
    -c "$capture_template" \
    --wait-for-exit \
    --opt-disallow-vsync \
    --opt-api-validation \
    "$runtime_path" "${runtime_args[@]}" >>"$capture_log" 2>&1
capture_status="$?"
set -e

if ((capture_status != 0)); then
  printf '[error] renderdoccmd capture failed with exit code %d. Last log lines:\n' "$capture_status" >&2
  tail -n 120 "$capture_log" >&2 || true
  exit "$capture_status"
fi

latest_renderdoc_capture="$(latest_capture)"
if [[ -z "$latest_renderdoc_capture" ]]; then
  printf '[error] RenderDoc completed but no .rdc capture was produced. Last log lines:\n' >&2
  tail -n 120 "$capture_log" >&2 || true
  exit 1
fi

printf '[info] latest renderdoc capture=%s\n' "$latest_renderdoc_capture"
printf '[info] capture log=%s\n' "$capture_log"
if [[ -f "$runtime_log" ]]; then
  printf '[info] latest real submitted FPS:\n'
  grep 'Frame summary 5s' "$runtime_log" | tail -n 1 || true
  printf '[info] latest stream state:\n'
  grep 'Stream benchmark:' "$runtime_log" | tail -n 3 || true
fi
