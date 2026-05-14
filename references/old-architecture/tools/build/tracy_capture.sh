#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

workspace_root="$(octaryn_workspace_root)"
log_dir="${workspace_root}/logs/engine_control"
capture_root="${workspace_root}/logs/tracy"
capture_dir="${capture_root}/captures"
export_dir="${capture_root}/exports"
build_root="$(octaryn_product_build_root)/tools"
capture_build_dir="${build_root}/tracy-capture"
csvexport_build_dir="${build_root}/tracy-csvexport"
capture_binary="${capture_build_dir}/tracy-capture"
csvexport_binary="${csvexport_build_dir}/tracy-csvexport"
log_path="${log_dir}/tracy-capture.log"

mkdir -p "$log_dir" "$capture_dir" "$export_dir"

find_tracy_source() {
  local cache_dir
  cache_dir="$(octaryn_shared_cpm_cache_dir)/tracy"
  if [[ ! -d "$cache_dir" ]]; then
    return 1
  fi
  find "$cache_dir" -mindepth 1 -maxdepth 1 -type d | sort | head -n 1
}

usage() {
  cat <<'USAGE'
Usage: tracy_capture.sh [options]

Options:
  --seconds <n>      Capture duration in seconds. Default: 15.
  --address <addr>   Tracy client address. Default: 127.0.0.1.
  --port <n>         Tracy client port. Default: 8086.
  --no-export        Save only the .tracy capture.
  --print-latest     Print the newest capture path and exit.
USAGE
}

latest_capture() {
  find "$capture_dir" -maxdepth 1 -type f -name '*.tracy' -printf '%T@ %p\n' |
    sort -nr |
    head -n 1 |
    cut -d' ' -f2-
}

build_tracy_tool() {
  local source_dir="$1"
  local build_dir="$2"
  local target="$3"
  local binary="$4"

  if [[ -x "$binary" ]]; then
    return 0
  fi
  printf '[info] building %s from %s\n' "$target" "$source_dir"
  cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DNO_ISA_EXTENSIONS=ON
  cmake --build "$build_dir" --target "$target" --parallel
}

tracy_ui_connected() {
  ss -tnp 2>/dev/null | grep -E "(:${port}[[:space:]]|:${port}$)" | grep -q 'tracy-profiler'
}

tracy_client_listening() {
  ss -ltnH "sport = :$port" 2>/dev/null | grep -q .
}

main() {
  local seconds="15"
  local address="127.0.0.1"
  local port="8086"
  local export_csv="1"

  while (($#)); do
    case "$1" in
      --seconds)
        seconds="${2:-}"
        shift 2
        ;;
      --address)
        address="${2:-}"
        shift 2
        ;;
      --port)
        port="${2:-}"
        shift 2
        ;;
      --no-export)
        export_csv="0"
        shift
        ;;
      --print-latest)
        latest_capture
        return 0
        ;;
      -h|--help)
        usage
        return 0
        ;;
      *)
        printf '[error] unknown argument: %s\n' "$1" >&2
        usage >&2
        return 2
        ;;
    esac
  done

  if [[ ! "$seconds" =~ ^[0-9]+$ ]] || ((seconds < 1)); then
    printf '[error] --seconds must be a positive integer.\n' >&2
    return 2
  fi
  if [[ ! "$port" =~ ^[0-9]+$ ]] || ((port < 1 || port > 65535)); then
    printf '[error] --port must be a valid TCP port.\n' >&2
    return 2
  fi

  local tracy_source
  if ! tracy_source="$(find_tracy_source)" || [[ -z "$tracy_source" ]]; then
    printf '[error] project Tracy source is missing. Configure with OCTARYN_ENABLE_TRACY=ON first.\n' >&2
    return 2
  fi

  local capture_source="${tracy_source}/capture"
  local csvexport_source="${tracy_source}/csvexport"
  if [[ ! -f "${capture_source}/CMakeLists.txt" ]]; then
    printf '[error] Tracy capture source is missing: %s\n' "$capture_source" >&2
    return 2
  fi
  if [[ "$export_csv" == "1" && ! -f "${csvexport_source}/CMakeLists.txt" ]]; then
    printf '[error] Tracy csvexport source is missing: %s\n' "$csvexport_source" >&2
    return 2
  fi

  build_tracy_tool "$capture_source" "$capture_build_dir" tracy-capture "$capture_binary"
  if [[ "$export_csv" == "1" ]]; then
    build_tracy_tool "$csvexport_source" "$csvexport_build_dir" tracy-csvexport "$csvexport_binary"
  fi

  if tracy_ui_connected; then
    printf '[error] Tracy UI is already connected to port %s. Close Tracy UI or disable auto UI launch before capturing.\n' "$port" >&2
    return 6
  fi
  if ! tracy_client_listening; then
    printf '[error] no Tracy client is listening on %s:%s. Start the engine first.\n' "$address" "$port" >&2
    return 7
  fi

  local stamp
  stamp="$(date +%Y%m%d-%H%M%S)"
  local capture_path="${capture_dir}/${stamp}.tracy"
  local summary_path="${capture_dir}/${stamp}.txt"
  local zones_csv="${export_dir}/${stamp}-zones.csv"
  local messages_csv="${export_dir}/${stamp}-messages.csv"

  {
    printf '[%s] tracy capture\n' "$(date --iso-8601=seconds)"
    printf '[info] address=%s port=%s seconds=%s\n' "$address" "$port" "$seconds"
    printf '[info] capture=%s\n' "$capture_path"
  } | tee -a "$log_path"

  local timeout_seconds=$((seconds + 15))
  set +e
  timeout "${timeout_seconds}s" "$capture_binary" -o "$capture_path" -a "$address" -p "$port" -f -s "$seconds" 2>&1 | tee -a "$log_path"
  local capture_status="${PIPESTATUS[0]}"
  set -e
  if ((capture_status != 0)); then
    printf '[error] Tracy capture failed with exit code %d.\n' "$capture_status" | tee -a "$log_path" >&2
    if ((capture_status == 124)); then
      printf '[error] Tracy capture timed out after %d seconds while waiting for client data.\n' "$timeout_seconds" | tee -a "$log_path" >&2
    fi
    return "$capture_status"
  fi

  local runtime_log="${OCTARYN_TRACY_RUNTIME_LOG:-${workspace_root}/logs/octaryn-engine/linux-debug/runtime-session.log}"
  {
    printf '[real_submitted_fps]\n'
    printf 'source=%s\n' "$runtime_log"
    if [[ -f "$runtime_log" ]]; then
      grep 'Frame summary 5s' "$runtime_log" | tail -n 1 || true
    fi
    printf '\n[capture_artifact]\n'
    printf 'tracy_capture=%s\n' "$capture_path"
    printf 'capture_seconds=%s\n' "$seconds"
    printf 'captured_at=%s\n' "$(date --iso-8601=seconds)"
  } >"$summary_path"

  if [[ -f "$runtime_log" ]]; then
    {
      printf '\n[latest_stream_state]\n'
      grep 'Stream benchmark:' "$runtime_log" | tail -n 3 || true
    } >>"$summary_path"
  fi

  if [[ "$export_csv" == "1" ]]; then
    "$csvexport_binary" -u "$capture_path" >"$zones_csv"
    "$csvexport_binary" -m "$capture_path" >"$messages_csv"
    {
      printf 'zones_csv=%s\n' "$zones_csv"
      printf 'messages_csv=%s\n' "$messages_csv"
    } >>"$summary_path"
    printf '[info] exported zones: %s\n' "$zones_csv" | tee -a "$log_path"
    printf '[info] exported messages: %s\n' "$messages_csv" | tee -a "$log_path"
  fi

  printf '[info] wrote real FPS summary: %s\n' "$summary_path" | tee -a "$log_path"
}

main "$@"
