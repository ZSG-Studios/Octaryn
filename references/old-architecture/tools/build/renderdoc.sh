#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

workspace_root="$(octaryn_workspace_root)"
log_dir="${workspace_root}/logs/engine_control"
log_path="${log_dir}/renderdoc.log"
source_root="$(octaryn_shared_cpm_cache_dir)/renderdoc"
tag="${OCTARYN_RENDERDOC_TAG:-v1.43}"
repo_url="${OCTARYN_RENDERDOC_REPOSITORY:-https://github.com/baldurk/renderdoc.git}"
source_dir="${source_root}/${tag}"
build_dir="$(octaryn_product_build_root)/tools/renderdoc-${tag}"

mkdir -p "$log_dir" "$source_root"

usage() {
  cat <<'USAGE'
Usage: renderdoc.sh [options]

Options:
  --print-path       Build if needed, print qrenderdoc path, and exit.
  --print-cmd-path   Build if needed, print renderdoccmd path, and exit.
  --build-only       Fetch/build project-local RenderDoc and exit.
  --clean-build      Remove the RenderDoc tool build directory before building.
  --tag <tag>        RenderDoc git tag. Default: v1.43.
  -h, --help         Show this help.
USAGE
}

find_renderdoc_binary() {
  local name="$1"
  find "$build_dir" -type f -name "$name" -perm -111 2>/dev/null | sort | head -n 1
}

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf '[error] required tool is missing: %s\n' "$1" >&2
    return 2
  fi
}

fetch_renderdoc_source() {
  if [[ -d "${source_dir}/.git" ]]; then
    git -C "$source_dir" fetch --tags --depth 1 origin "$tag"
    git -C "$source_dir" checkout --detach "refs/tags/${tag}"
    git -C "$source_dir" submodule update --init --recursive --depth 1
    return 0
  fi

  rm -rf -- "$source_dir"
  printf '[info] fetching RenderDoc %s into %s\n' "$tag" "$source_dir"
  git clone --recursive --depth 1 --branch "$tag" "$repo_url" "$source_dir"
}

build_renderdoc() {
  require_tool git
  require_tool cmake
  require_tool make

  if [[ "$(uname -m)" != "x86_64" ]]; then
    printf '[error] RenderDoc Linux builds are only supported on x86_64.\n' >&2
    return 2
  fi

  fetch_renderdoc_source

  if [[ ! -f "${source_dir}/CMakeLists.txt" ]]; then
    printf '[error] RenderDoc source is missing CMakeLists.txt: %s\n' "$source_dir" >&2
    return 2
  fi

  if [[ ! -x "$(find_renderdoc_binary qrenderdoc)" || ! -x "$(find_renderdoc_binary renderdoccmd)" ]]; then
    printf '[info] building project RenderDoc from %s\n' "$source_dir"
    cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build_dir" --parallel
  fi
}

register_renderdoc_vulkan_layer() {
  local renderdoccmd_path
  renderdoccmd_path="$(find_renderdoc_binary renderdoccmd)"
  if [[ -z "$renderdoccmd_path" || ! -x "$renderdoccmd_path" ]]; then
    printf '[warning] renderdoccmd was not found; skipping Vulkan layer registration.\n'
    return 0
  fi

  local layer_status
  layer_status="$("$renderdoccmd_path" vulkanlayer --explain 2>&1 || true)"
  printf '%s\n' "$layer_status" >&2
  if grep -q 'not correctly registered' <<<"$layer_status"; then
    printf '[info] registering project RenderDoc Vulkan layer for this user\n'
    "$renderdoccmd_path" vulkanlayer --register --user
  fi
}

main() {
  local print_path="0"
  local print_cmd_path="0"
  local build_only="0"
  local clean_build="0"

  while (($#)); do
    case "$1" in
      --print-path)
        print_path="1"
        shift
        ;;
      --print-cmd-path)
        print_cmd_path="1"
        shift
        ;;
      --build-only)
        build_only="1"
        shift
        ;;
      --clean-build)
        clean_build="1"
        shift
        ;;
      --tag)
        tag="${2:-}"
        source_dir="${source_root}/${tag}"
        build_dir="$(octaryn_product_build_root)/tools/renderdoc-${tag}"
        shift 2
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

  if [[ -z "$tag" ]]; then
    printf '[error] --tag must not be empty.\n' >&2
    return 2
  fi

  if [[ "$print_path" != "1" && "$print_cmd_path" != "1" ]]; then
    exec >>"$log_path" 2>&1
    printf '[%s] renderdoc launcher\n' "$(date --iso-8601=seconds)"
    printf '[info] tag=%s source=%s build=%s\n' "$tag" "$source_dir" "$build_dir"
  fi

  if [[ "$clean_build" == "1" ]]; then
    rm -rf -- "$build_dir"
  fi

  build_renderdoc
  register_renderdoc_vulkan_layer

  local qrenderdoc_path
  qrenderdoc_path="$(find_renderdoc_binary qrenderdoc)"
  if [[ -z "$qrenderdoc_path" || ! -x "$qrenderdoc_path" ]]; then
    printf '[error] qrenderdoc was not produced by the project RenderDoc build.\n' >&2
    return 3
  fi

  local renderdoccmd_path
  renderdoccmd_path="$(find_renderdoc_binary renderdoccmd)"

  if [[ "$print_path" == "1" ]]; then
    printf '%s\n' "$qrenderdoc_path"
    return 0
  fi

  if [[ "$print_cmd_path" == "1" ]]; then
    if [[ -z "$renderdoccmd_path" || ! -x "$renderdoccmd_path" ]]; then
      printf '[error] renderdoccmd was not produced by the project RenderDoc build.\n' >&2
      return 3
    fi
    printf '%s\n' "$renderdoccmd_path"
    return 0
  fi

  if [[ "$build_only" == "1" ]]; then
    printf '[info] project RenderDoc built: %s\n' "$qrenderdoc_path"
    if [[ -n "$renderdoccmd_path" ]]; then
      printf '[info] project renderdoccmd built: %s\n' "$renderdoccmd_path"
    fi
    return 0
  fi

  printf '[info] launching %s\n' "$qrenderdoc_path"
  local qpa_platform="${QT_QPA_PLATFORM:-xcb}"
  if command -v setsid >/dev/null 2>&1; then
    setsid env QT_QPA_PLATFORM="$qpa_platform" "$qrenderdoc_path" "$@" >>"$log_path" 2>&1 < /dev/null &
  else
    nohup env QT_QPA_PLATFORM="$qpa_platform" "$qrenderdoc_path" "$@" >>"$log_path" 2>&1 < /dev/null &
  fi
  printf '[info] RenderDoc pid=%d\n' "$!"
}

main "$@"
